/*
 * Versi 0 - sekuensial.
 * Seluruh frame diproses satu per satu oleh satu thread dengan Sobel 2D langsung.
 * Menjadi patokan waktu satu prosesor (t_s) dan acuan kebenaran keluaran.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
    opts_t o;
    parse_opts(argc, argv, &o);

    /* Muat seluruh frame ke RAM sebelum pewaktu dinyalakan. */
    dataset_t ds;
    if (dataset_load(&ds, o.dir, 0, o.frames)) return 1;
    const int w = ds.w, h = ds.h, n = ds.n;
    const size_t npx = (size_t)w * h;

    uint8_t *out  = malloc(npx * n);
    uint8_t *gray = malloc(npx);
    uint8_t *blur = malloc(npx);
    if (!out || !gray || !blur) {
        fprintf(stderr, "memori tidak cukup\n");
        return 1;
    }

    double times[MAX_RUNS];
    for (int r = -o.warmup; r < o.runs; r++) {
        double t0 = omp_get_wtime();                      

        for (int i = 0; i < n; i++)
            process_frame_v0(ds.rgb[i], out + (size_t)i * npx, gray, blur, w, h);

        double t = omp_get_wtime() - t0;                 
        if (r >= 0) times[r] = t;
        printf("  %s %d: %.4f s\n", r < 0 ? "warm-up" : "run", r < 0 ? -r : r + 1, t);
    }

    uint64_t sum = checksum_frames(out, npx, n);
    report(&o, "V0", 1, 1, n, w, h, times, sum);

    if (o.outdir && write_outputs(o.outdir, out, w, h, 0, n)) return 1;

    free(out);
    free(gray);
    free(blur);
    dataset_free(&ds);
    return 0;
}
