#define _GNU_SOURCE
#include "common.h"

#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* I/O frame (dipanggil di luar pewaktu) */
static int read_header_int(FILE *f, int *val)
{
    int c;
    for (;;) {
        c = fgetc(f);
        if (c == '#') {
            while ((c = fgetc(f)) != '\n' && c != EOF) {}
        } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
    }
    if (c < '0' || c > '9') return -1;
    int v = 0;
    while (c >= '0' && c <= '9') {
        v = v * 10 + (c - '0');
        c = fgetc(f);
    }
    *val = v;  /* satu karakter spasi setelah angka sudah terbaca */
    return 0;
}

static uint8_t *ppm_read(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "gagal membuka %s: %s\n", path, strerror(errno));
        return NULL;
    }
    char magic[2];
    int maxval;
    if (fread(magic, 1, 2, f) != 2 || magic[0] != 'P' || magic[1] != '6' ||
        read_header_int(f, w) || read_header_int(f, h) ||
        read_header_int(f, &maxval) || maxval != 255) {
        fprintf(stderr, "%s: bukan PPM P6 8-bit\n", path);
        fclose(f);
        return NULL;
    }
    size_t size = (size_t)(*w) * (*h) * 3;
    uint8_t *buf = malloc(size);
    if (!buf || fread(buf, 1, size, f) != size) {
        fprintf(stderr, "%s: data piksel tidak lengkap\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return buf;
}

/* Muat frame dengan indeks first .. first+count-1 (berkas frame_0001.ppm, ...). */
int dataset_load(dataset_t *ds, const char *dir, int first, int count)
{
    char path[4096];
    ds->n = count;
    ds->w = ds->h = 0;
    ds->rgb = calloc(count > 0 ? count : 1, sizeof(uint8_t *));
    if (!ds->rgb) return -1;

    for (int i = 0; i < count; i++) {
        int w, h;
        snprintf(path, sizeof path, "%s/frame_%04d.ppm", dir, first + i + 1);
        ds->rgb[i] = ppm_read(path, &w, &h);
        if (!ds->rgb[i]) return -1;
        if (i == 0) {
            ds->w = w;
            ds->h = h;
        } else if (w != ds->w || h != ds->h) {
            fprintf(stderr, "%s: ukuran berbeda dari frame pertama\n", path);
            return -1;
        }
    }
    return 0;
}

void dataset_free(dataset_t *ds)
{
    for (int i = 0; i < ds->n; i++) free(ds->rgb[i]);
    free(ds->rgb);
    ds->rgb = NULL;
    ds->n = 0;
}

static int mkdir_p(const char *dir)
{
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", dir);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
    return 0;
}

/* Tulis frame peta tepi sebagai PGM P5: edge_0001.pgm, ... */
int write_outputs(const char *dir, const uint8_t *out, int w, int h,
                  int first, int count)
{
    char path[4096];
    size_t npx = (size_t)w * h;
    if (mkdir_p(dir)) {
        fprintf(stderr, "gagal membuat folder %s: %s\n", dir, strerror(errno));
        return -1;
    }
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof path, "%s/edge_%04d.pgm", dir, first + i + 1);
        FILE *f = fopen(path, "wb");
        if (!f) {
            fprintf(stderr, "gagal menulis %s: %s\n", path, strerror(errno));
            return -1;
        }
        fprintf(f, "P5\n%d %d\n255\n", w, h);
        fwrite(out + (size_t)i * npx, 1, npx, f);
        fclose(f);
    }
    return 0;
}

/* 1. Grayscale: Y = 0,299R + 0,587G + 0,114B dalam aritmetika bilangan bulat
 *    (77 + 150 + 29 = 256, lalu dibagi 256 dengan pergeseran bit). */
void to_grayscale(const uint8_t *rgb, uint8_t *gray, int w, int h)
{
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; i++) {
        const uint8_t *p = rgb + 3 * i;
        gray[i] = (uint8_t)((77 * p[0] + 150 * p[1] + 29 * p[2] + 128) >> 8);
    }
}

/* 2. Reduksi noise: filter rata-rata (box blur) 3x3. */
void box_blur3(const uint8_t *src, uint8_t *dst, int w, int h) 
{
    memcpy(dst, src, (size_t)w);
    memcpy(dst + (size_t)(h - 1) * w, src + (size_t)(h - 1) * w, (size_t)w);
    for (int y = 1; y < h - 1; y++) {
        const uint8_t *r0 = src + (size_t)(y - 1) * w;
        const uint8_t *r1 = r0 + w;
        const uint8_t *r2 = r1 + w;
        uint8_t *d = dst + (size_t)y * w;
        d[0] = r1[0];
        d[w - 1] = r1[w - 1];
        for (int x = 1; x < w - 1; x++) {
            int s = r0[x - 1] + r0[x] + r0[x + 1]
                  + r1[x - 1] + r1[x] + r1[x + 1]
                  + r2[x - 1] + r2[x] + r2[x + 1];
            d[x] = (uint8_t)(s / 9);
        }
    }
}

/* 3. Sobel 2D langsung (V0/V1): dua kernel 3x3 = 18 perkalian per piksel,
 *      magnitudo |G| = sqrt(Gx^2 + Gy^2), dinormalisasi ke 0..255 dengan clamp. */
static const int KX[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
static const int KY[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

void sobel_naive(const uint8_t *src, uint8_t *dst, int w, int h)
{
    memset(dst, 0, (size_t)w);
    memset(dst + (size_t)(h - 1) * w, 0, (size_t)w);
    for (int y = 1; y < h - 1; y++) {
        uint8_t *d = dst + (size_t)y * w;
        d[0] = 0;
        d[w - 1] = 0;
        for (int x = 1; x < w - 1; x++) {
            int gx = 0, gy = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int p = src[(size_t)(y + ky) * w + (x + kx)];
                    gx += KX[ky + 1][kx + 1] * p;
                    gy += KY[ky + 1][kx + 1] * p;
                }
            }
            int m = (int)(sqrtf((float)(gx * gx + gy * gy)) + 0.5f);
            d[x] = (uint8_t)(m > 255 ? 255 : m);
        }
    }
}

void process_frame_v0(const uint8_t *rgb, uint8_t *out,
                      uint8_t *gray, uint8_t *blur, int w, int h)
{
    to_grayscale(rgb, gray, w, h);
    box_blur3(gray, blur, w, h);
    sobel_naive(blur, out, w, h);
}


static uint64_t fnv1a(const uint8_t *data, size_t len)
{
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t checksum_frames(const uint8_t *out, size_t npx, int n)
{
    uint64_t sum = 0;
    for (int i = 0; i < n; i++) sum += fnv1a(out + (size_t)i * npx, npx);
    return sum;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

double median(const double *v, int n)
{
    double tmp[MAX_RUNS];
    memcpy(tmp, v, (size_t)n * sizeof(double));
    qsort(tmp, (size_t)n, sizeof(double), cmp_double);
    return (n % 2) ? tmp[n / 2] : 0.5 * (tmp[n / 2 - 1] + tmp[n / 2]);
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Pemakaian: %s -d <folder_frame> -n <jumlah_frame> [opsi]\n"
        "  -l <label>   label resolusi untuk CSV (default: nama folder)\n"
        "  -r <runs>    jumlah pengukuran, median dilaporkan (default 5)\n"
        "  -w <warmup>  jumlah pemanasan yang tidak dihitung (default 1)\n"
        "  -t <thread>  jumlah thread OpenMP (default: OMP_NUM_THREADS)\n"
        "  -o <folder>  tulis frame peta tepi (PGM) ke folder ini\n"
        "  -c <csv>     tambahkan baris hasil ke berkas CSV\n", prog);
    exit(2);
}

void parse_opts(int argc, char **argv, opts_t *o)
{
    memset(o, 0, sizeof *o);
    o->runs = 5;
    o->warmup = 1;
    int c;
    while ((c = getopt(argc, argv, "d:l:n:r:w:t:o:c:h")) != -1) {
        switch (c) {
        case 'd': o->dir = optarg; break;
        case 'l': o->label = optarg; break;
        case 'n': o->frames = atoi(optarg); break;
        case 'r': o->runs = atoi(optarg); break;
        case 'w': o->warmup = atoi(optarg); break;
        case 't': o->threads = atoi(optarg); break;
        case 'o': o->outdir = optarg; break;
        case 'c': o->csv = optarg; break;
        default: usage(argv[0]);
        }
    }
    if (!o->dir || o->frames <= 0 || o->runs < 1 || o->runs > MAX_RUNS ||
        o->warmup < 0)
        usage(argv[0]);
    if (!o->label) {
        const char *s = strrchr(o->dir, '/');
        o->label = (s && s[1]) ? s + 1 : o->dir;
    }
}


void report(const opts_t *o, const char *version, int nodes, int threads,
            int frames, int w, int h, const double *times, uint64_t checksum)
{
    double med = median(times, o->runs);
    double mn = times[0], mx = times[0];
    for (int i = 1; i < o->runs; i++) {
        if (times[i] < mn) mn = times[i];
        if (times[i] > mx) mx = times[i];
    }
    double ms_per_frame = med * 1000.0 / frames;
    double fps = frames / med;

    printf("%s | %s %dx%d, %d frame | %d node x %d thread\n",
           version, o->label, w, h, frames, nodes, threads);
    printf("  median %.4f s (min %.4f, maks %.4f, %d run)\n", med, mn, mx, o->runs);
    printf("  waktu/frame %.2f ms | %.2f FPS | real-time 30 FPS: %s\n",
           ms_per_frame, fps, ms_per_frame <= 1000.0 / 30.0 ? "YA" : "TIDAK");
    printf("  checksum %016llx\n", (unsigned long long)checksum);

    if (!o->csv) return;
    FILE *f = fopen(o->csv, "a+");
    if (!f) {
        fprintf(stderr, "gagal membuka %s: %s\n", o->csv, strerror(errno));
        return;
    }
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0)
        fprintf(f, "version,resolution,width,height,frames,nodes,threads_per_node,"
                   "runs,median_s,min_s,max_s,ms_per_frame,fps,checksum,times_s\n");
    fprintf(f, "%s,%s,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.4f,%.4f,%016llx,",
            version, o->label, w, h, frames, nodes, threads, o->runs,
            med, mn, mx, ms_per_frame, fps, (unsigned long long)checksum);
    for (int i = 0; i < o->runs; i++)
        fprintf(f, "%s%.6f", i ? ";" : "", times[i]);
    fprintf(f, "\n");
    fclose(f);
}
