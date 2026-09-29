#!/usr/bin/env bash
# Restore Demo-Project.audium to its pristine state between recording takes.
#
# Uses ditto so file mtimes are preserved — the analysis cache is keyed on
# mtime + size, and a plain `cp -R` would invalidate it, making Create Segments
# fail with "Still analysing ... (sbic, beat_degara)".
#
# Close the project in Audionaut (or quit the app) before running, so the GUI
# does not autosave its in-memory state back over the restored files.

set -euo pipefail

# Override with DEMO_PROJECT=... to record from a different package.
P="${DEMO_PROJECT:-$HOME/Music/Audionaut/Demo-Project.audium}"
B="$(dirname "$P")/.$(basename "$P" .audium)-pristine.audium"

[[ -d "$B" ]] || { echo "No pristine backup at $B" >&2; exit 1; }

rm -rf "$P"
ditto "$B" "$P"

echo "Restored $P"
python3 - "$P" <<'PY' 2>/dev/null || true
import json,sys
raw=open(sys.argv[1]+'/Project.json').read()
d=json.loads(raw[:raw.rfind('}')+1])
pl=d['audium']['audio_tracks'][0]['play_list_vector']
print(f"  clips: {len(pl)}  (pristine = 1)")
PY
