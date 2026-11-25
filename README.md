# Deteksi Tepi Video dengan Operator Sobel: V0, V1 (OpenMP), V2 (Hybrid MPI–OpenMP)

Implementasi proposal UTS High Performance Computing: *Optimasi Sistem Parallel pada Deteksi
Tepi Video Menggunakan Operator Sobel dengan Hybrid MPI–OpenMP*.

| Versi | Berkas | Isi |
|---|---|---|
| V0 | [src/v0_seq.c](src/v0_seq.c) | Sekuensial, Sobel 2D langsung (18 perkalian/piksel), magnitudo √(Gx²+Gy²) |
| V1 | [src/v1_omp.c](src/v1_omp.c) | OpenMP single-node, paralel antar frame (`#pragma omp parallel for`), perhitungan sama dengan V0 |
| V2 | [src/v2_hybrid.c](src/v2_hybrid.c) | MPI membagi frame antar node, OpenMP membagi frame ke thread; Sobel separable + magnitudo \|Gx\|+\|Gy\| |
| – | [src/common.c](src/common.c) | Baca/tulis frame, grayscale, box blur 3×3, Sobel 2D, checksum, laporan CSV |

## Struktur folder

```
24fps_4096x2160.mp4      video sumber (isi: 3840x2160, H.264, 23,976 fps, 456 frame)
docker/                  image dan cluster teremulasi (Dockerfile, compose.yml, hostfile)
src/                     kode sumber C (V0, V1, V2)
tests/mpi_hello.c        uji komunikasi antar-node (Tahap 2)
scripts/                 ekstraksi dataset, verifikasi, eksperimen, analisis, video
results/                 results.csv, summary.md/csv, grafik, frame contoh, video
Makefile                 build semua versi (gcc/mpicc -O2 -fopenmp)
```

Dataset frame dan keluaran PGM disimpan di disk WSL (`~/hpc-data`, dipasang sebagai `/data`
di container), bukan di folder proyek, karena ukurannya beberapa GB.

## Lingkungan: cluster teremulasi dengan Docker

Semua container memakai satu image (Ubuntu 24.04, GCC, Open MPI, FFmpeg, Python) dan berjalan di
satu komputer. Setiap container berperan sebagai satu compute node dan berkomunikasi lewat
jaringan virtual Docker (TCP). Setiap node diberi core sendiri dengan `cpuset`:

| Container | Peran | CPU (cpuset) |
|---|---|---|
| `single` | node untuk V0 dan V1 (1, 2, 4, 8 thread) | 0,2,4,6,8,10,12,14 (8 core) |
| `node1`–`node4` | node untuk V2 (2 thread OpenMP per node) | 2 core per node |

Di WSL, CPU (0,1), (2,3), … adalah pasangan hyperthread dari core yang sama, sehingga hanya CPU
bernomor genap yang dipakai agar setiap thread mendapat core sendiri.


## Cara menjalankan

Prasyarat: WSL2 Ubuntu dengan Docker, user termasuk grup `docker`. Disk WSL sebaiknya berada di
drive dengan ruang kosong cukup (proyek ini memindahkannya ke `D:\WSL\Ubuntu`).

Buka terminal **Ubuntu (WSL)** dan biarkan tetap terbuka selama bekerja (WSL mematikan Ubuntu
beserta container sekitar 15 detik setelah tidak ada terminal yang terhubung).

```bash
cd "/mnt/d/T-1/Smstr 7/High Programming Computing (HPC)/Project"
DC="docker compose -f docker/compose.yml"

# Build image (sekali) dan nyalakan cluster
mkdir -p ~/hpc-data
$DC build
$DC up -d

# Tahap 1: ekstraksi frame PPM (720p 300, 1080p 150, 4K 50 frame)
$DC exec -u mpi single bash scripts/extract_dataset.sh

# Tahap 2: uji komunikasi antar-node
$DC exec -u mpi single make
$DC exec -u mpi node1 mpirun -np 4 --hostfile docker/hostfile --bind-to none bin/mpi_hello

# Tahap 3-5: verifikasi kebenaran + frame contoh (results/samples/)
bash scripts/verify.sh

# Tahap 6: seluruh eksperimen -> results/results.csv
bash scripts/run_experiments.sh

# Tahap 7: tabel metrik dan grafik -> results/summary.md, results/fig_*.png
$DC exec -u mpi single python3 scripts/analyze.py

# Video peta tepi untuk presentasi -> results/video/
bash scripts/make_video.sh
```

Menjalankan satu konfigurasi secara manual:

```bash
# V0 / V1 (di node "single")
$DC exec -u mpi single bin/v0 -d /data/frames/1080p -n 150
$DC exec -u mpi single bin/v1 -d /data/frames/1080p -n 150 -t 8
# V2: 4 node x 2 thread
$DC exec -u mpi node1 mpirun -np 4 --hostfile docker/hostfile --bind-to none \
    bin/v2 -d /data/frames/1080p -n 150 -t 2
```

Opsi program: `-d` folder frame, `-n` jumlah frame, `-t` thread OpenMP, `-r` jumlah pengukuran
(default 5), `-w` pemanasan (default 1), `-o` folder keluaran PGM, `-c` berkas CSV.

## Prosedur pengukuran

- Seluruh frame dimuat ke RAM sebelum pewaktu dinyalakan (`omp_get_wtime` untuk V0/V1,
  `MPI_Wtime` untuk V2). Baca/tulis berkas berada di luar pengukuran.
- V2: setiap node memuat subset frame miliknya, semua rank disinkronkan dengan `MPI_Barrier`,
  lalu pewaktu dimulai. Waktu yang dicatat adalah waktu rank yang paling lama (`MPI_Reduce` MAX).
- Setiap konfigurasi: 1 pemanasan + 5 pengukuran, nilai yang dilaporkan median.
- Opsi kompiler sama untuk semua versi: `-O2 -fopenmp`.

## Detail implementasi

| Hal | Keputusan |
|---|---|
| Grayscale integer | `Y = (77R + 150G + 29B + 128) >> 8` (77 + 150 + 29 = 256) |
| Tepi gambar | box blur: piksel tepi disalin; Sobel: piksel tepi bernilai 0 |
| Box blur | jumlah 9 piksel dibagi 9 (pembagian integer) |
| Normalisasi ke 0–255 | clamp: nilai di atas 255 menjadi 255 |
| Pembagian frame V2 | blok berurutan, selisih jumlah frame antar rank paling banyak 1 |
| Checksum | jumlah (mod 2^64) checksum FNV-1a setiap frame; dapat dijumlahkan antar rank MPI |

## Verifikasi kebenaran

- V1 harus identik dengan V0: checksum sama (kolom `checksum` di CSV) dan perbandingan byte per
  frame di `scripts/compare.py`.
- V2: selisih rata-rata magnitudo terhadap V0 dilaporkan oleh `scripts/compare.py`, frame contoh
  (asli | V0 | V2) di `results/samples/`, dan program V2 memeriksa setiap indeks frame diproses
  tepat satu kali.
