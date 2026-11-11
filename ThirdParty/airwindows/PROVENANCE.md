# Airwindows in Audionaut

The built-in Airwindows effects, the `airwindows:` insert types.

- **Upstream:** Airwindows by Chris Johnson, https://www.airwindows.com,
  https://github.com/airwindows/airwindows, through
  **Airwindows Consolidated** (airwin2rack) by Paul Walker and contributors,
  https://github.com/baconpaul/airwin2rack, which restructures every
  Airwindows effect as a class with no dependency on the VST2 SDK.
- **Version:** airwin2rack commit `316a0485b28516347e325645a39c13c623c02962`
  (2026-11-08), which carries Airwindows at commit
  `94cb6793893027fb418ffac7cdbfcb9b4c82aadb`.
- **Licence:** MIT, both. See `LICENSE-airwindows` (Airwindows) and
  `LICENSE-airwin2rack.md` (Airwindows Consolidated). The airwin2rack README
  states that `src/` is MIT and fine to use in closed-source code.

## What is here

From airwin2rack's `src/`:

- `src/airwin_consolidated_base.h`, `src/airwin_consolidated_base.cpp`, unmodified
- `src/autogen_airwin/<Name>.h`, `<Name>.cpp`, `<Name>Proc.cpp` for each
  effect in the list in `audionaut/import_airwindows.py`

One patch, applied by the import script: the VST2 `_canDo` string set is
removed from each effect (the member, its `insert()` calls, and `canDo()`
answers -1). Nothing in Audionaut asks `canDo()`, and without the set every
effect class is plainly copyable, which is how Audionaut resets an effect on
the render thread: by copying a pristine instance over the live one, without
allocating. The DSP code is unchanged.

## Updating or adding effects

    git clone https://github.com/baconpaul/airwin2rack.git /tmp/airwin2rack
    python3 -I ThirdParty/airwindows/audionaut/import_airwindows.py /tmp/airwin2rack

The script copies and patches the sources, and regenerates
`Audionaut/Source/Engine/Effects/Airwindows/AirwindowsCatalog.cpp` (parameter
tables, the display and text-to-value functions lifted from each effect, and
the descriptors). A new effect also needs its three files registered in
`Audionaut/Audionaut.jucer` (group ThirdParty/airwindows/autogen_airwin, the
`.cpp` files with `compilerFlagScheme="airwindows"`) and a Projucer re-save.
Update this file's version line.
