#!/usr/bin/env bash
# bash scripts/run_experiments.sh
set -euo pipefail
cd "$(dirname "$0")/.."

DC="docker compose -f docker/compose.yml"
CSV=results/results.csv
THREADS_PER_NODE=2
DATASETS=("720p 300" "1080p 150" "4k 50")

mkdir -p results
if [ -f "$CSV" ]; then
    mv "$CSV" "results/results_$(date +%Y%m%d_%H%M%S).csv.bak"
fi

$DC up -d
$DC exec -T -u mpi single make -s

for item in "${DATASETS[@]}"; do
    read -r res n <<< "$item"
    dir=/data/frames/$res
    echo "==================== $res ($n frame) ===================="

    $DC exec -T -u mpi single bin/v0 -d "$dir" -n "$n" -c "$CSV"

    for t in 1 2 4 8; do
        $DC exec -T -u mpi single bin/v1 -d "$dir" -n "$n" -t "$t" -c "$CSV"
    done

    for nodes in 1 2 4; do
        $DC exec -T -u mpi node1 mpirun -np "$nodes" --hostfile docker/hostfile \
            --bind-to none bin/v2 -d "$dir" -n "$n" -t "$THREADS_PER_NODE" -c "$CSV"
    done
done

echo "Selesai. Hasil: $CSV"
