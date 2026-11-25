/*
 * Versi 1 - OpenMP single-node.
 * Paralelisasi antar frame dengan #pragma omp parallel for. Setiap thread
 * memproses frame secara utuh dan menulis ke penampung keluaran milik frame
 * tersebut, sehingga tidak ada dua thread yang menulis ke lokasi yang sama.
 * Perhitungan di dalam frame sama persis dengan V0.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
    opts_t o;
    parse_opts(argc, argv, &o);
    if (o.threads > 0) omp_set_num_threads(o.threads);
    const int nthreads = omp_get_max_threads();

    dataset_t ds;
    if (dataset_load(&ds, o.dir, 0, o.frames)) return 1;
    const int w = ds.w, h = ds.h, n = ds.n;
    const size_t npx = (size_t)w * h;

    /* Buffer kerja (gray, blur) disediakan per thread sebelum pewaktu.
     * Jika dipakai bersama, thread akan saling menimpa (race condition). */
    uint8_t *out = malloc(npx * n);
    uint8_t **gray = malloc(nthreads * sizeof(uint8_t *));
    uint8_t **blur = malloc(nthreads * sizeof(uint8_t *));
    if (!out || !gray || !blur) {
        fprintf(stderr, "memori tidak cukup\n");
        return 1;
    }
    for (int t = 0; t < nthreads; t++) {
        gray[t] = malloc(npx);
        blur[t] = malloc(npx);
        if (!gray[t] || !blur[t]) {
            fprintf(stderr, "memori tidak cukup\n");
            return 1;
        }
    }

    double times[MAX_RUNS];
    for (int r = -o.warmup; r < o.runs; r++) {
        double t0 = omp_get_wtime();                       /* MULAI PEWAKTU */

        #pragma omp parallel for
        for (int i = 0; i < n; i++) {
            int t = omp_get_thread_num();
            process_frame_v0(ds.rgb[i], out + (size_t)i * npx, gray[t], blur[t], w, h);
        }

        double t = omp_get_wtime() - t0;                   /* PEWAKTU DIMATIKAN */
        if (r >= 0) times[r] = t;
        printf("  %s %d: %.4f s\n", r < 0 ? "warm-up" : "run", r < 0 ? -r : r + 1, t);
    }

    uint64_t sum = checksum_frames(out, npx, n);
    report(&o, "V1", 1, nthreads, n, w, h, times, sum);

    if (o.outdir && write_outputs(o.outdir, out, w, h, 0, n)) return 1;

    for (int t = 0; t < nthreads; t++) {
        free(gray[t]);
        free(blur[t]);
    }
    free(gray);
    free(blur);
    free(out);
    dataset_free(&ds);
    return 0;
}
