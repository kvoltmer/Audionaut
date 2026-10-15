#!/usr/bin/env python3
"""Sync the git pins in app.audionaut.Audionaut.yml to the current checkout.

The manifest pins the app source to a release tag + commit and every
submodule source to the commit recorded at that tag. This script rewrites
those pins from the repository the script lives in, so the manifest builds
exactly what is checked out:

  - the app source (the only git source without a `dest:`) gets the given
    --ref/--commit (defaults: the exact tag on HEAD, or the current branch,
    and the HEAD commit);
  - every git source with `dest: Submodules/...` gets the commit its
    gitlink points to, read with `git ls-tree` (descending into checked-out
    submodules for nested paths like Submodules/link/modules/asio-standalone,
    which must be initialized: git submodule update --init Submodules/link
    Submodules/demucs.cpp).

Used by .github/workflows/release-linux.yml before flatpak-builder, and by
hand when bumping the committed pins for a release. Edits lines in place,
leaving the manifest's comments untouched (a YAML round-trip would drop
them).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
MANIFEST = HERE / "app.audionaut.Audionaut.yml"


def git(*args, cwd=ROOT):
    return subprocess.run(
        ["git", *args], cwd=cwd, check=True, capture_output=True, text=True
    ).stdout.strip()


def gitlink_commit(path):
    """The commit the gitlink at `path` records, descending into submodules."""
    repo, rel = ROOT, path
    while True:
        out = git("ls-tree", "HEAD", rel, cwd=repo)
        if out:
            _mode, objtype, sha, _name = out.split(None, 3)
            if objtype != "commit":
                sys.exit(f"sync-pins: {path} is not a gitlink (found {objtype})")
            return sha
        # The path crosses a submodule boundary; find the submodule and
        # continue the lookup inside its checkout.
        parts = rel.split("/")
        for i in range(len(parts) - 1, 0, -1):
            prefix = "/".join(parts[:i])
            out = git("ls-tree", "HEAD", prefix, cwd=repo)
            if out and out.split(None, 3)[1] == "commit":
                sub = repo / prefix
                if not (sub / ".git").exists():
                    sys.exit(
                        f"sync-pins: {path} needs submodule {prefix} checked out "
                        f"(git submodule update --init {prefix})"
                    )
                repo, rel = sub, "/".join(parts[i:])
                break
        else:
            sys.exit(f"sync-pins: {path} not found in the git tree")


def parse_blocks(lines):
    """Git source blocks as dicts of key -> line index. A block is a
    `- type: git` item plus its following 8-space-indented key lines."""
    blocks = []
    current = None
    for i, line in enumerate(lines):
        if re.match(r"^\s{6}- type: git\s*$", line):
            current = {}
            blocks.append(current)
        elif current is not None and re.match(r"^\s{8}\S", line):
            m = re.match(r"^\s{8}([\w-]+):\s*(.*)$", line)
            if m:
                current[m.group(1)] = i
        elif line.strip():
            current = None
    return blocks


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--url", help="app source URL (default: keep)")
    parser.add_argument("--ref", help="tag or branch name for the app source")
    parser.add_argument(
        "--ref-type",
        choices=["tag", "branch"],
        help="whether --ref is a tag or a branch",
    )
    parser.add_argument("--commit", help="app commit (default: HEAD)")
    args = parser.parse_args()

    commit = args.commit or git("rev-parse", "HEAD")
    ref, ref_type = args.ref, args.ref_type
    if not ref:
        try:
            ref, ref_type = git("describe", "--exact-match", "--tags"), "tag"
        except subprocess.CalledProcessError:
            ref, ref_type = git("rev-parse", "--abbrev-ref", "HEAD"), "branch"
    if not ref_type:
        ref_type = "tag" if re.fullmatch(r"v\d+(\.\d+)*", ref) else "branch"

    lines = MANIFEST.read_text().splitlines(keepends=True)
    changes = []

    def set_value(idx, key, value):
        old = lines[idx]
        indent = re.match(r"^\s*", old).group(0)
        new = f"{indent}{key}: {value}\n"
        if new != old:
            lines[idx] = new
            changes.append(f"  {old.strip()}  ->  {key}: {value}")

    for block in parse_blocks(lines):
        if "dest" not in block:
            # The app source: the only git source without a dest.
            if args.url:
                set_value(block["url"], "url", args.url)
            # The ref line is `tag:` or `branch:`, whichever the manifest
            # currently has; rewrite it to the right kind for this ref.
            ref_idx = block.get("tag", block.get("branch"))
            set_value(ref_idx, ref_type, ref)
            set_value(block["commit"], "commit", commit)
        else:
            dest = re.match(r"^\s{8}dest:\s*(\S+)", lines[block["dest"]]).group(1)
            if dest.startswith("Submodules/"):
                set_value(block["commit"], "commit", gitlink_commit(dest))

    if changes:
        MANIFEST.write_text("".join(lines))
        print(f"sync-pins: updated {MANIFEST.name}:")
        print("\n".join(changes))
    else:
        print("sync-pins: manifest already in sync")


if __name__ == "__main__":
    main()
