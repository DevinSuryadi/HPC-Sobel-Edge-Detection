#!/usr/bin/env bash
# ekstraksi frame mentah PPM (P6) dari video sumber untuk tiga tingkatan dataset.
#   docker compose -f docker/compose.yml exec -u mpi single bash scripts/extract_dataset.sh

set -euo pipefail

SRC=${1:-/work/24fps_4096x2160.mp4}
OUT=${2:-/data/frames}

extract() {
    local name=$1 w=$2 h=$3 n=$4
    mkdir -p "$OUT/$name"
    rm -f "$OUT/$name"/*.ppm
    ffmpeg -hide_banner -loglevel error -y -i "$SRC" \
        -vf "scale=${w}:${h}:flags=area" \
        -frames:v "$n" -pix_fmt rgb24 "$OUT/$name/frame_%04d.ppm"
    sync   

    # Periksa jumlah frame dan ukuran setiap berkas (header PPM + w*h*3 byte).
    local header expect got bad
    header=$(printf 'P6\n%d %d\n255\n' "$w" "$h" | wc -c)
    expect=$((header + w * h * 3))
    got=$(find "$OUT/$name" -name '*.ppm' | wc -l)
    bad=$(find "$OUT/$name" -name '*.ppm' ! -size "${expect}c" | wc -l)
    echo "$name (${w}x${h}): $got frame, $(du -sh "$OUT/$name" | cut -f1), ukuran salah: $bad"
    if [ "$got" -ne "$n" ] || [ "$bad" -ne 0 ]; then
        echo "GAGAL: diharapkan $n frame berukuran $expect byte" >&2
        exit 1
    fi
}

extract 720p  1280  720 300
extract 1080p 1920 1080 150
extract 4k    3840 2160  50
