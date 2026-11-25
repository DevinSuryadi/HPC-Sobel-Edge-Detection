#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

DC="docker compose -f docker/compose.yml"
FPS=24   # laju frame video sumber
mkdir -p results/video

$DC up -d
for res in 720p 1080p 4k; do
    $DC exec -T -u mpi single ffmpeg -hide_banner -loglevel error -y \
        -framerate "$FPS" -i "/data/out/v2/$res/edge_%04d.pgm" \
        -c:v libx264 -pix_fmt yuv420p -crf 18 "results/video/edge_$res.mp4"
    echo "results/video/edge_$res.mp4"
done
