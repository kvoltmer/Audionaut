#!/usr/bin/env bash
# Turn a long screen recording into the README hero GIF with per-beat pacing.
#
#   docs/plans/hero-demo/make-paced-gif.sh recording.mov segments.txt [fps] [colors]
#
# segments.txt holds one beat per line: "<start> <end> <speed> <comment>", in
# seconds of the raw recording. Each window is cut and sped up by its factor,
# the windows are concatenated in order, and anything between them is dropped.
# Typing reads well at 2-5x, agent waits at 12-40x, visible changes at 5-8x.
#
# Writes hero.gif (fps, default 8, and a palette of colors, default 64) and
# hero.mp4 (same cut, 24 fps) next to the recording. Take 2 at 12/128 was
# 12 MB; 8/64 brought it to 7.4 MB with no visible loss on the UI.

set -euo pipefail

FFMPEG="$(command -v /opt/homebrew/bin/ffmpeg || command -v ffmpeg)"

in="${1:?usage: make-paced-gif.sh recording.mov segments.txt [fps] [colors]}"
segments="${2:?usage: make-paced-gif.sh recording.mov segments.txt [fps] [colors]}"
fps="${3:-8}"
colors="${4:-64}"
width=1000
outdir="$(dirname "$in")"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Downscale once so the per-segment trims stay fast on a retina recording.
"$FFMPEG" -v error -y -i "$in" -vf "fps=24,scale=${width}:-2:flags=lanczos" \
    -c:v libx264 -crf 16 -preset veryfast -an "$tmp/work.mp4"

graph=""; inputs=""; n=0
while read -r start end speed _; do
    [[ -z "$start" || "$start" == \#* ]] && continue
    graph+="[0:v]trim=${start}:${end},setpts=(PTS-STARTPTS)/${speed}[s$n];"
    inputs+="[s$n]"
    n=$((n + 1))
done < "$segments"
printf '%s%sconcat=n=%d:v=1:a=0,fps=24[out]' "$graph" "$inputs" "$n" > "$tmp/graph.txt"

"$FFMPEG" -v error -y -i "$tmp/work.mp4" -filter_complex_script "$tmp/graph.txt" -map "[out]" \
    -c:v libx264 -crf 23 -pix_fmt yuv420p -movflags +faststart "$outdir/hero.mp4"

# The GIF is cut from a 12 fps pass, not from hero.mp4: sampling 8 fps out of 24
# picks busier frames and made take 2's GIF ~5 % larger.
sed 's/fps=24\[out\]$/fps=12[out]/' "$tmp/graph.txt" > "$tmp/graph12.txt"
"$FFMPEG" -v error -y -i "$tmp/work.mp4" -filter_complex_script "$tmp/graph12.txt" -map "[out]" \
    -c:v libx264 -crf 23 -pix_fmt yuv420p "$tmp/cut12.mp4"

"$FFMPEG" -v error -y -i "$tmp/cut12.mp4" \
    -vf "fps=${fps},palettegen=max_colors=${colors}:stats_mode=diff" "$tmp/palette.png"
"$FFMPEG" -v error -y -i "$tmp/cut12.mp4" -i "$tmp/palette.png" \
    -lavfi "[0:v]fps=${fps}[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle" \
    "$outdir/hero.gif"

ls -lh "$outdir/hero.gif" "$outdir/hero.mp4"
echo "GIF budget: <= 8 MB. If over, raise the speeds of the busy segments or drop fps/colors."
