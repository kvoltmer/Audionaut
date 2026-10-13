#!/usr/bin/env bash
#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Publishes docs/manual/ to the website repo (../Audionaut-Web by default):
# copies the chapters, renames README.md to index.md (MkDocs convention),
# rewrites repo-relative links to GitHub URLs, and rebuilds the site.
#
# The app repo is the source of truth - edit the manual there, then run this.
#
# Usage: Tools/sync-manual.sh [--commit]
#   --commit    also commit and push the website repo when there are changes
#
# The website location can be overridden with AUDIONAUT_WEB_DIR.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WEB_DIR="${AUDIONAUT_WEB_DIR:-$REPO_ROOT/../Audionaut-Web}"
SOURCE_DIR="$REPO_ROOT/docs/manual"
TARGET_DIR="$WEB_DIR/docs/manual"
GITHUB_BLOB="https://github.com/kvoltmer/Audionaut/blob/main"

COMMIT=false
[[ "${1:-}" == "--commit" ]] && COMMIT=true

[[ -f "$SOURCE_DIR/README.md" ]] || { echo "error: no manual at $SOURCE_DIR" >&2; exit 1; }
[[ -f "$WEB_DIR/mkdocs.yml" ]]   || { echo "error: no MkDocs site at $WEB_DIR" >&2; exit 1; }

# Replace the target wholesale so deleted chapters and images disappear too.
rm -rf "$TARGET_DIR"
mkdir -p "$TARGET_DIR"
cp "$SOURCE_DIR"/*.md "$TARGET_DIR"/
[ -d "$SOURCE_DIR/img" ] && cp -R "$SOURCE_DIR/img" "$TARGET_DIR/img"
mv "$TARGET_DIR/README.md" "$TARGET_DIR/index.md"

# Repo-relative links point at files that only exist on GitHub.
for file in "$TARGET_DIR"/*.md; do
    sed -i.bak 's|](\.\./\.\./|]('"$GITHUB_BLOB"'/|g' "$file" && rm "$file.bak"
done

if command -v mkdocs >/dev/null 2>&1; then
    (cd "$WEB_DIR" && mkdocs build --quiet)
else
    echo "warning: mkdocs not found - site/ not rebuilt" >&2
fi

if [[ -z "$(git -C "$WEB_DIR" status --porcelain)" ]]; then
    echo "manual already in sync - nothing to do"
    exit 0
fi

echo "changed in $WEB_DIR:"
git -C "$WEB_DIR" status --short

if $COMMIT; then
    git -C "$WEB_DIR" add -A
    git -C "$WEB_DIR" commit -m "Sync the user manual from the app repo"
    git -C "$WEB_DIR" push
    echo "committed and pushed"
else
    echo "review and commit in $WEB_DIR (or rerun with --commit)"
fi
