#!/usr/bin/env python3
#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
"""
Imports the curated Airwindows effects from an airwin2rack checkout.

    python3 -I ThirdParty/airwindows/audionaut/import_airwindows.py <path to airwin2rack>

- copies src/airwin_consolidated_base.{h,cpp} unchanged and, per effect,
  src/autogen_airwin/<Name>.h, <Name>.cpp and <Name>Proc.cpp into
  ThirdParty/airwindows/src, with one patch: the VST2 `_canDo` string set
  goes (nothing calls canDo()), which leaves every effect class plainly
  copyable - Audionaut resets an effect by copying a pristine instance
  over it, on the render thread, without allocating;
- generates Audionaut/Source/Engine/Effects/Airwindows/AirwindowsCatalog.cpp:
  the parameter tables, the display and text-to-value functions lifted from
  each effect's own getParameterDisplay() / parameterTextToValue(), and the
  descriptors the registry creates the effects from.

To add an effect, add it to EFFECTS below and run the script again; then
register the three new files in Audionaut.jucer (group ThirdParty/airwindows,
compilerFlagScheme="airwindows") and re-save with the Projucer.
"""

import os, re, shutil, subprocess, sys

# name -> (Audionaut category, tail seconds). The categories group the insert
# menu; the tail is how long the effect rings on after its input stops, for
# the export tails and bounces (Airwindows reports none, so these are
# generous estimates at the longest settings).
EFFECTS = {
    "ButterComp2":  ("Dynamics",   0.0),
    "Pressure4":    ("Dynamics",   0.0),
    "Logical4":     ("Dynamics",   0.0),
    "Pyewacket":    ("Dynamics",   0.0),
    "PurestSquish": ("Dynamics",   0.0),
    "BlockParty":   ("Dynamics",   0.0),
    "ADClip8":      ("Clipping",   0.0),
    "ClipOnly2":    ("Clipping",   0.0),
    "TapeDelay2":   ("Delay",     10.0),
    "PurestEcho":   ("Delay",      2.0),
    "Doublelay":    ("Delay",      6.0),
    "kPlateD":      ("Reverb",     6.0),
    "kCathedral3":  ("Reverb",    10.0),
    "Galactic3":    ("Reverb",    12.0),
    "Chamber2":     ("Reverb",     6.0),
    "Verbity2":     ("Reverb",     8.0),
    "ToTape6":      ("Saturation", 0.0),
    "Density2":     ("Saturation", 0.0),
    "PurestDrive":  ("Saturation", 0.0),
    "Mackity":      ("Saturation", 0.0),
    "Air3":         ("EQ",         0.0),
    "Baxandall2":   ("EQ",         0.0),
    "Pear2":        ("EQ",         0.0),
}

HERE = os.path.dirname(os.path.abspath(__file__))
THIRD_PARTY = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(THIRD_PARTY))
CATALOG = os.path.join(REPO, "Audionaut/Source/Engine/Effects/Airwindows/AirwindowsCatalog.cpp")


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def patch_header(text, name):
    out, n = re.subn(r"[ \t]*std::set<\s*std::string\s*>\s*_canDo;[^\n]*\n", "", text)
    assert n == 1, f"{name}.h: _canDo member not found"
    return out


def patch_source(text, name):
    out, n = re.subn(r"[ \t]*_canDo\.insert\([^\n]*\n", "", text)
    assert n >= 1, f"{name}.cpp: no _canDo.insert"
    out, n = re.subn(r"\{ return \(_canDo\.find\(text\) == _canDo\.end\(\)\) \? -1: 1; \}",
                     "{ return -1; }", out)
    assert n == 1, f"{name}.cpp: canDo() body not found"
    return out


def function_body(text, name, signature):
    m = re.search(name + r"::" + signature + r"\s*\{(.*?)\n\}", text, re.S)
    assert m, f"{name}: {signature} not found"
    return m.group(1)


def switch_of(body):
    """The `switch (index) { ... }` statement inside a function body, verbatim."""
    start = body.index("switch")
    depth, i = 0, body.index("{", start)
    while True:
        if body[i] == "{":
            depth += 1
        elif body[i] == "}":
            depth -= 1
            if depth == 0:
                return body[start:i + 1]
        i += 1


def parameter_id(display_name):
    ident = re.sub(r"[^a-z0-9]+", "_", display_name.lower()).strip("_")
    return ident or "value"


def describe(src, name, descriptions):
    h = read(os.path.join(src, "autogen_airwin", name + ".h"))
    c = read(os.path.join(src, "autogen_airwin", name + ".cpp"))
    num = int(re.search(r"kNumParameters\s*=\s*(\d+)", h).group(1))
    enums = [k for k, i in re.findall(r"(kParam\w+)\s*=\s*(\d+)", h) if int(i) < num]
    variables = dict(re.findall(r"case (kParam\w+):\s*(\w+)\s*=\s*value", c))
    ctor = re.search(name + r"::" + name + r"\(audioMasterCallback audioMaster\)\s*:.*?\{(.*?)\n\}", c, re.S).group(1)
    names_body = function_body(c, name, r"getParameterName\(VstInt32 index, char \*text\)")
    labels_body = function_body(c, name, r"getParameterLabel\(VstInt32 index, char \*text\)")
    params = []
    for k in enums:
        var = variables[k]
        display_name = re.search(r"case " + k + r":\s*vst_strncpy\s*\(text,\s*\"([^\"]*)\"", names_body).group(1)
        label_match = re.search(r"case " + k + r":\s*vst_strncpy\s*\(text,\s*\"([^\"]*)\"", labels_body)
        default = float(re.search(r"\b" + var + r"\s*=\s*([0-9.]+)\s*;", ctor).group(1))
        params.append(dict(enum=k, var=var, name=display_name,
                           label=(label_match.group(1).strip() if label_match else ""), default=default))
    ids = [parameter_id(p["name"]) for p in params]
    for i, ident in enumerate(ids):   # unique within the effect
        if ids.count(ident) > 1:
            ids[i] = f"{ident}_{i + 1}"
    for p, ident in zip(params, ids):
        p["id"] = ident
    display = switch_of(function_body(c, name, r"getParameterDisplay\(VstInt32 index, char \*text\)")) if params else ""
    parse_body = function_body(c, name, r"parameterTextToValue\(VstInt32 index, const char \*text, float &value\)") if params else ""
    parse = switch_of(parse_body) if "switch" in parse_body else ""
    return dict(name=name, params=params, display=display, parse=parse, description=descriptions.get(name, ""))


def cpp_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(effects):
    out = []
    out.append("""//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// Generated by ThirdParty/airwindows/audionaut/import_airwindows.py - do not edit by hand.
// The display and text-to-value functions are the effects' own getParameterDisplay()
// and parameterTextToValue() switches, lifted verbatim into free functions over a value.

#include "AirwindowsEffect.h"
""")
    for e in effects:
        out.append(f'#include "autogen_airwin/{e["name"]}.h"')
    out.append("""
#include <algorithm>
#include <cmath>

#if defined (__clang__) || defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wunused-variable"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wdouble-promotion"
 #pragma GCC diagnostic ignored "-Wimplicit-float-conversion"
 #pragma GCC diagnostic ignored "-Wshorten-64-to-32"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

namespace audium::airwindows
{
namespace
{
    template <typename Plugin>
    std::unique_ptr<AirwinConsolidatedBase> create() { return std::make_unique<Plugin> (0); }

    template <typename Plugin>
    void copyState (AirwinConsolidatedBase& to, const AirwinConsolidatedBase& from)
    {
        static_cast<Plugin&> (to) = static_cast<const Plugin&> (from);
    }
""")
    for e in effects:
        n = e["name"]
        ns = f"airwinconsolidated::{n}"
        out.append(f"    // {n} ------------------------------------------------------------")
        if e["params"]:
            out.append(f"    constexpr EffectParameterInfo {n}Parameters[] {{")
            for p in e["params"]:
                out.append(f'        {{ {cpp_string(p["id"])}, {cpp_string(p["name"])}, "", 0.0f, 1.0f, {p["default"]!r}f }},')
            out.append("    };")
            out.append(f"    constexpr const char* {n}Labels[] {{ " + ", ".join(cpp_string(p["label"]) for p in e["params"]) + " };")
            locals_ = ", ".join(f'{p["var"]} = value' for p in e["params"])
            out.append(f"""
    void {n}Display (int index, float value, char* text)
    {{
        using namespace {ns};
        const float {locals_};
        {e["display"]}
    }}
""")
            if e["parse"]:
                out.append(f"""    bool {n}Parse (int index, const char* text, float& value)
    {{
        using namespace {ns};
        {e["parse"]}
        return false;
    }}
""")
        out.append("")
    out.append("""    const AirwindowsDescriptor descriptors[] {""")
    for e in effects:
        n = e["name"]
        category, tail = EFFECTS[n]
        if e["params"]:
            params = f"{n}Parameters"
            labels = f"{n}Labels"
            display = f"&{n}Display"
            parse = f"&{n}Parse" if e["parse"] else "nullptr"
        else:
            params, labels, display, parse = "{}", "nullptr", "nullptr", "nullptr"
        out.append(f"""        {{ {cpp_string(n)}, {cpp_string(category)}, {cpp_string(e["description"])},
          {params}, {labels}, {tail!r},
          &create<airwinconsolidated::{n}::{n}>, &copyState<airwinconsolidated::{n}::{n}>, {display}, {parse} }},""")
    out.append("""    };
} // namespace

std::span<const AirwindowsDescriptor> getDescriptors() noexcept
{
    return descriptors;
}

} // namespace audium::airwindows
""")
    return "\n".join(out)


def main():
    root = os.path.abspath(sys.argv[1])
    src = os.path.join(root, "src")
    module_add = read(os.path.join(src, "ModuleAdd.h"))
    descriptions = {n: d for n, d in re.findall(r'registerAirwindow\(\{"(\w+)", "[^"]*", \d+, \w+, "((?:[^"\\]|\\.)*)"', module_add)}

    dst = os.path.join(THIRD_PARTY, "src")
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    for base in ("airwin_consolidated_base.h", "airwin_consolidated_base.cpp"):
        write(os.path.join(dst, base), read(os.path.join(src, base)))
    for name in EFFECTS:
        here = os.path.join(src, "autogen_airwin")
        write(os.path.join(dst, "autogen_airwin", name + ".h"), patch_header(read(os.path.join(here, name + ".h")), name))
        write(os.path.join(dst, "autogen_airwin", name + ".cpp"), patch_source(read(os.path.join(here, name + ".cpp")), name))
        write(os.path.join(dst, "autogen_airwin", name + "Proc.cpp"), read(os.path.join(here, name + "Proc.cpp")))
    shutil.copyfile(os.path.join(root, "LICENSE.md"), os.path.join(THIRD_PARTY, "LICENSE-airwin2rack.md"))

    effects = [describe(src, name, descriptions) for name in EFFECTS]
    write(CATALOG, generate(effects))

    commit = subprocess.run(["git", "-C", root, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    print(f"imported {len(effects)} effects from airwin2rack {commit}")


if __name__ == "__main__":
    main()
