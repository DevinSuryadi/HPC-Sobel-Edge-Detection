/*
 * Versi 2 - hybrid MPI-OpenMP teroptimasi.
 *
 * Paralelisasi:
 *   - MPI  : kumpulan frame dibagi seimbang antar compute node (blok berurutan,
 *            selisih jumlah frame antar rank paling banyak 1). Setiap rank memuat
 *            subset frame miliknya ke memori lokal sebelum pewaktu.
 *   - OpenMP: subset frame pada setiap node dibagi ke beberapa thread.
 *
 * Optimasi di dalam frame:
 *   1. Sobel separable: Gx = penghalus vertikal [1,2,1] x turunan horizontal [-1,0,1]
 *                       Gy = turunan vertikal [-1,0,1] x penghalus horizontal [1,2,1]
 *      sehingga konvolusi 2D diganti dua lintasan 1D.
 *   2. Magnitudo pendekatan |G| = |Gx| + |Gy| (tanpa kuadrat dan akar).
 *
 * Pengukuran: semua rank disinkronkan (MPI_Barrier), pewaktu dimulai, dan waktu
 * yang dicatat adalah waktu rank yang paling lama selesai (MPI_Reduce MAX).
 */
#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* Lintasan horizontal 1D pada satu baris:
 *   dx = turunan horizontal  [-1, 0, 1]
 *   sx = penghalus horizontal [1, 2, 1]                                   */
static void sobel_pass_horizontal(const uint8_t *row, int16_t *dx, int16_t *sx, int w)
{
    for (int x = 1; x < w - 1; x++) {
        dx[x] = (int16_t)(row[x + 1] - row[x - 1]);
        sx[x] = (int16_t)(row[x - 1] + 2 * row[x] + row[x + 1]);
    }
}

/* Sobel separable + magnitudo |Gx| + |Gy|.
 * Hasil lintasan horizontal disimpan dalam buffer 3 baris (cincin); lintasan
 * vertikal untuk baris y memakai baris y-1, y, y+1 dari buffer tersebut:
 *   Gx = dx[y-1] + 2*dx[y] + dx[y+1]   (penghalus vertikal [1, 2, 1])
 *   Gy = sx[y+1] - sx[y-1]             (turunan vertikal  [-1, 0, 1])
 * ring: buffer kerja 6*w int16. Piksel di tepi gambar bernilai 0 (sama dengan V0). */
static void sobel_separable_abs(const uint8_t *src, uint8_t *dst, int w, int h,
                                int16_t *ring)
{
    int16_t *dx[3], *sx[3];
    for (int k = 0; k < 3; k++) {
        dx[k] = ring + (size_t)k * w;
        sx[k] = ring + (size_t)(3 + k) * w;
    }

    memset(dst, 0, (size_t)w);
    memset(dst + (size_t)(h - 1) * w, 0, (size_t)w);
    sobel_pass_horizontal(src, dx[0], sx[0], w);
    sobel_pass_horizontal(src + w, dx[1], sx[1], w);

    for (int y = 1; y < h - 1; y++) {
        int up = (y - 1) % 3, mid = y % 3, dn = (y + 1) % 3;
        sobel_pass_horizontal(src + (size_t)(y + 1) * w, dx[dn], sx[dn], w);

        uint8_t *d = dst + (size_t)y * w;
        d[0] = 0;
        d[w - 1] = 0;
        for (int x = 1; x < w - 1; x++) {
            int gx = dx[up][x] + 2 * dx[mid][x] + dx[dn][x];
            int gy = sx[dn][x] - sx[up][x];
            int m = (gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy);
            d[x] = (uint8_t)(m > 255 ? 255 : m);
        }
    }
}

/* Pipeline V2: grayscale -> box blur (sama dengan V0) -> Sobel separable |Gx|+|Gy|. */
static void process_frame_v2(const uint8_t *rgb, uint8_t *out, uint8_t *gray,
                             uint8_t *blur, int16_t *ring, int w, int h)
{
    to_grayscale(rgb, gray, w, h);
    box_blur3(gray, blur, w, h);
    sobel_separable_abs(blur, out, w, h, ring);
}

int main(int argc, char **argv)
{
    int provided, rank, size;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    opts_t o;
    parse_opts(argc, argv, &o);
    if (o.threads > 0) omp_set_num_threads(o.threads);
    const int nthreads = omp_get_max_threads();
    const int N = o.frames;

    /* Pembagian frame seimbang antar rank. */
    const int base = N / size, rem = N % size;
    const int my_count = base + (rank < rem ? 1 : 0);
    const int my_first = rank * base + (rank < rem ? rank : rem);

    /* Setiap node memuat subset frame miliknya (di luar pewaktu). */
    dataset_t ds;
    int ok = dataset_load(&ds, o.dir, my_first, my_count) == 0;
    int all_ok;
    MPI_Allreduce(&ok, &all_ok, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!all_ok) {
        if (rank == 0) fprintf(stderr, "pemuatan frame gagal pada salah satu rank\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    int wh[2] = {ds.w, ds.h};   /* rank tanpa frame tetap perlu ukuran frame */
    MPI_Allreduce(MPI_IN_PLACE, wh, 2, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    const int w = wh[0], h = wh[1];
    const size_t npx = (size_t)w * h;

    char host[MPI_MAX_PROCESSOR_NAME];
    int hlen;
    MPI_Get_processor_name(host, &hlen);
    printf("  rank %d (%s): frame %d..%d (%d frame), %d thread\n",
           rank, host, my_first + 1, my_first + my_count, my_count, nthreads);
    fflush(stdout);

    /* Buffer keluaran dan buffer kerja per thread, disiapkan sebelum pewaktu. */
    uint8_t *out = malloc(npx * (my_count > 0 ? my_count : 1));
    uint8_t **gray = malloc(nthreads * sizeof(uint8_t *));
    uint8_t **blur = malloc(nthreads * sizeof(uint8_t *));
    int16_t **ring = malloc(nthreads * sizeof(int16_t *));
    int *done = calloc(N, sizeof(int));    /* berapa kali tiap frame diproses */
    if (!out || !gray || !blur || !ring || !done) MPI_Abort(MPI_COMM_WORLD, 1);
    for (int t = 0; t < nthreads; t++) {
        gray[t] = malloc(npx);
        blur[t] = malloc(npx);
        ring[t] = malloc(6 * (size_t)w * sizeof(int16_t));
        if (!gray[t] || !blur[t] || !ring[t]) MPI_Abort(MPI_COMM_WORLD, 1);
    }

    double times[MAX_RUNS];
    for (int r = -o.warmup; r < o.runs; r++) {
        memset(done, 0, N * sizeof(int));
        MPI_Barrier(MPI_COMM_WORLD);                       /* seluruh rank siap */
        double t0 = MPI_Wtime();                           /* MULAI PEWAKTU */

        #pragma omp parallel for
        for (int i = 0; i < my_count; i++) {
            int t = omp_get_thread_num();
            process_frame_v2(ds.rgb[i], out + (size_t)i * npx,
                             gray[t], blur[t], ring[t], w, h);
            done[my_first + i]++;
        }

        double t_local = MPI_Wtime() - t0;                 /* PEWAKTU DIMATIKAN */
        double t_global;
        MPI_Reduce(&t_local, &t_global, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            if (r >= 0) times[r] = t_global;
            printf("  %s %d: %.4f s\n", r < 0 ? "warm-up" : "run",
                   r < 0 ? -r : r + 1, t_global);
        }
    }

    /* Verifikasi: setiap indeks frame diproses tepat satu kali dan
     * jumlah frame hasil seluruh rank sama dengan jumlah frame masukan. */
    int *done_all = rank == 0 ? calloc(N, sizeof(int)) : NULL;
    MPI_Reduce(done, done_all, N, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    uint64_t sum_local = checksum_frames(out, npx, my_count), sum_all = 0;
    MPI_Reduce(&sum_local, &sum_all, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        int total = 0, bad = 0;
        for (int i = 0; i < N; i++) {
            total += done_all[i];
            if (done_all[i] != 1) bad++;
        }
        printf("  verifikasi pembagian frame: %d/%d frame diproses, %d frame tidak tepat 1 kali -> %s\n",
               total, N, bad, (total == N && bad == 0) ? "OK" : "GAGAL");
        report(&o, "V2", size, nthreads, N, w, h, times, sum_all);
        free(done_all);
    }

    /* Setiap rank menulis frame peta tepi miliknya (di luar pewaktu). */
    if (o.outdir && my_count > 0 && write_outputs(o.outdir, out, w, h, my_first, my_count))
        MPI_Abort(MPI_COMM_WORLD, 1);

    for (int t = 0; t < nthreads; t++) {
        free(gray[t]);
        free(blur[t]);
        free(ring[t]);
    }
    free(gray);
    free(blur);
    free(ring);
    free(done);
    free(out);
    dataset_free(&ds);
    MPI_Finalize();
    return 0;
}
