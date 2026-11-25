#!/usr/bin/env bash
# bash scripts/verify.sh

set -euo pipefail
cd "$(dirname "$0")/.."

DC="docker compose -f docker/compose.yml"
DATASETS=("720p 300" "1080p 150" "4k 50")

$DC up -d
$DC exec -T -u mpi single make -s
mkdir -p results/samples

for item in "${DATASETS[@]}"; do
    read -r res n <<< "$item"
    dir=/data/frames/$res
    echo "==================== $res ($n frame) ===================="

    $DC exec -T -u mpi single bin/v0 -d "$dir" -n "$n" -r 1 -w 0 -o "/data/out/v0/$res"
    $DC exec -T -u mpi single bin/v1 -d "$dir" -n "$n" -r 1 -w 0 -t 8 -o "/data/out/v1/$res"
    $DC exec -T -u mpi node1 mpirun -np 4 --hostfile docker/hostfile --bind-to none \
        bin/v2 -d "$dir" -n "$n" -r 1 -w 0 -t 2 -o "/data/out/v2/$res"

    $DC exec -T -u mpi single python3 scripts/compare.py "/data/out/v0/$res" "/data/out/v1/$res" "$n" V1
    $DC exec -T -u mpi single python3 scripts/compare.py "/data/out/v0/$res" "/data/out/v2/$res" "$n" V2
done

# Frame contoh 1080p: awal, tengah, akhir. Susunan kiri-ke-kanan: asli | V0 | V2.
for f in 1 75 150; do
    id=$(printf "%04d" "$f")
    $DC exec -T -u mpi single ffmpeg -hide_banner -loglevel error -y \
        -i "/data/frames/1080p/frame_$id.ppm" \
        -i "/data/out/v0/1080p/edge_$id.pgm" \
        -i "/data/out/v2/1080p/edge_$id.pgm" \
        -filter_complex "[1]format=rgb24[a];[2]format=rgb24[b];[0][a][b]hstack=inputs=3,scale=2880:-2" \
        "results/samples/1080p_frame_$id.png"
done
echo "Frame contoh: results/samples/"
