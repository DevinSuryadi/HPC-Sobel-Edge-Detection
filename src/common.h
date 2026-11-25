#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

#define MAX_RUNS 32

/* Kumpulan frame RGB yang sudah dimuat ke RAM. */
typedef struct {
    int w, h;       /* ukuran frame (piksel) */
    int n;          /* jumlah frame yang dimuat */
    uint8_t **rgb;  /* rgb[i]: frame ke-i, w*h*3 byte (R,G,B) */
} dataset_t;

/* Opsi baris perintah (sama untuk semua versi). */
typedef struct {
    const char *dir;     /* -d folder frame, mis. /data/frames/1080p      */
    const char *label;   /* -l label resolusi untuk CSV, mis. 1080p        */
    int frames;          /* -n jumlah frame                                */
    int runs;            /* -r jumlah pengukuran (default 5)               */
    int warmup;          /* -w jumlah pemanasan (default 1)                */
    int threads;         /* -t jumlah thread OpenMP (0 = OMP_NUM_THREADS)  */
    const char *outdir;  /* -o folder keluaran PGM (opsional)              */
    const char *csv;     /* -c berkas CSV hasil (opsional)                 */
} opts_t;

/* ---- I/O ---- */
int  dataset_load(dataset_t *ds, const char *dir, int first, int count);
void dataset_free(dataset_t *ds);
int  write_outputs(const char *dir, const uint8_t *out, int w, int h,
                   int first, int count);

/* ---- Tahap pipeline (satu frame) ---- */
void to_grayscale(const uint8_t *rgb, uint8_t *gray, int w, int h);
void box_blur3(const uint8_t *src, uint8_t *dst, int w, int h);
void sobel_naive(const uint8_t *src, uint8_t *dst, int w, int h);

/* Pipeline lengkap V0/V1: grayscale -> box blur -> Sobel 2D -> sqrt(Gx^2+Gy^2).
 * gray dan blur adalah buffer kerja berukuran w*h milik pemanggil. */
void process_frame_v0(const uint8_t *rgb, uint8_t *out,
                      uint8_t *gray, uint8_t *blur, int w, int h);

/* ---- Utilitas ---- */
/* Checksum keluaran = jumlah (mod 2^64) checksum FNV-1a tiap frame.
 * Tidak bergantung urutan, sehingga hasil dari beberapa MPI rank dapat
 * dijumlahkan dengan MPI_Reduce dan tetap sama dengan hasil satu proses. */
uint64_t checksum_frames(const uint8_t *out, size_t npx, int n);
double median(const double *v, int n);
void   parse_opts(int argc, char **argv, opts_t *o);
void   report(const opts_t *o, const char *version, int nodes, int threads,
              int frames, int w, int h, const double *times, uint64_t checksum);

#endif
