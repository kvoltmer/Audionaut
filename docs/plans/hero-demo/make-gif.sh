#!/usr/bin/env bash
# Turn a screen recording into the README hero GIF (+ a trimmed MP4 for linking).
#
#   docs/plans/hero-demo/make-gif.sh recording.mov [start] [duration]
#
# start/duration select the GIF window (beats 2-6 of the storyboard); defaults
# cover the whole recording. Output lands next to the input as hero.gif / hero.mp4.
# Requires ffmpeg; uses gifski for the GIF when installed (brew install gifski),
# otherwise ffmpeg's two-pass palette encode.

set -euo pipefail

# /usr/local/bin may carry an ancient ffmpeg without libx264; prefer Homebrew's.
FFMPEG="$(command -v /opt/homebrew/bin/ffmpeg || command -v ffmpeg)"

in="${1:?usage: make-gif.sh recording.mov [start] [duration]}"
start="${2:-0}"
duration="${3:-}"
width=1000
fps=12
outdir="$(dirname "$in")"

trim=(-ss "$start")
[[ -n "$duration" ]] && trim+=(-t "$duration")

# Trimmed, downscaled MP4 (also the source for gifski so both share one cut).
"$FFMPEG" -y "${trim[@]}" -i "$in" -vf "scale=${width}:-2" -c:v libx264 -crf 23 -pix_fmt yuv420p -movflags +faststart -an "$outdir/hero.mp4"

if command -v gifski >/dev/null; then
    tmp="$(mktemp -d)"
    "$FFMPEG" -y -i "$outdir/hero.mp4" -vf "fps=${fps}" "$tmp/frame%04d.png"
    gifski --fps "$fps" --width "$width" --quality 80 -o "$outdir/hero.gif" "$tmp"/frame*.png
    rm -rf "$tmp"
else
    filters="fps=${fps},scale=${width}:-1:flags=lanczos"
    "$FFMPEG" -y -i "$outdir/hero.mp4" -vf "${filters},palettegen=max_colors=128:stats_mode=diff" "$outdir/palette.png"
    "$FFMPEG" -y -i "$outdir/hero.mp4" -i "$outdir/palette.png" -lavfi "${filters}[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" "$outdir/hero.gif"
    rm -f "$outdir/palette.png"
fi

ls -lh "$outdir/hero.gif" "$outdir/hero.mp4"
echo "GIF budget: <= 8 MB. If over, shorten the window, lower fps to 10, or drop width to 900."
