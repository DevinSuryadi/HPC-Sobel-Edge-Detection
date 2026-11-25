# Narasi Presentasi: Program Deteksi Tepi Video (V0, V1, V2)

Panduan pemakaian dokumen ini:

- **Narasi** = kalimat yang diucapkan (boleh disesuaikan dengan gaya bicara sendiri).
- **Tunjukkan** = file atau bagian kode yang dibuka di layar. Link di bawah bisa diklik di VS Code.
- Total sekitar 15 menit. Kalau waktunya hanya 5 menit, cukup bawakan bagian **2, 5, 7, dan 8**.

---

## 0. Pembuka (±30 detik)

**Narasi:**
> "Pada bagian ini kami menjelaskan program yang kami buat: file apa saja yang ada, apa
> gunanya, dan bagaimana alurnya dari video mentah sampai menjadi grafik hasil. Programnya
> ditulis dalam bahasa C dan terdiri dari tiga versi: V0 sekuensial, V1 OpenMP pada satu
> node, dan V2 hybrid MPI–OpenMP yang sudah dioptimasi. Ketiganya mengerjakan hal yang sama,
> yaitu deteksi tepi Sobel pada setiap frame video. Yang berbeda adalah cara membagi kerjanya."

---

## 1. Peta file (±1,5 menit)

**Tunjukkan:** panel Explorer VS Code.

```
Project/
├── 24fps_4096x2160.mp4      video sumber (isi 3840x2160, 23,976 fps, 456 frame)
├── docker/                  LINGKUNGAN: cluster tiruan di satu laptop
│   ├── Dockerfile           resep image: gcc, Open MPI, FFmpeg, Python, SSH
│   ├── compose.yml          daftar "node": single, node1, node2, node3, node4
│   └── hostfile             daftar node untuk mpirun
├── src/                     PROGRAM INTI (bahasa C)
│   ├── common.h / common.c  fungsi bersama: baca/tulis frame, 4 tahap pipeline, checksum, laporan
│   ├── v0_seq.c             Versi 0: sekuensial
│   ├── v1_omp.c             Versi 1: OpenMP
│   └── v2_hybrid.c          Versi 2: MPI + OpenMP + Sobel separable
├── tests/mpi_hello.c        uji komunikasi antar-node
├── scripts/                 ALAT BANTU (otomatisasi)
│   ├── extract_dataset.sh   video -> frame 720p/1080p/4K
│   ├── verify.sh            cek kebenaran V0/V1/V2 + frame contoh
│   ├── compare.py           bandingkan keluaran sebuah versi dengan V0
│   ├── run_experiments.sh   jalankan semua konfigurasi -> CSV
│   ├── analyze.py           hitung speedup/efisiensi + buat grafik
│   └── make_video.sh        rangkai frame peta tepi -> video
├── results/                 HASIL: results.csv, summary.md, fig_*.png, samples/, video/
├── Makefile                 resep kompilasi ketiga versi
└── README.md                cara menjalankan semuanya
```

Dataset frame (sekitar 2,9 GB) dan keluaran PGM tidak disimpan di folder proyek, melainkan di
disk WSL `~/hpc-data`. Di dalam container, folder ini terlihat sebagai `/data`.

**Narasi:**
> "Isi proyek kami bagi menjadi empat kelompok. Pertama, folder `docker` untuk lingkungan,
> yaitu cluster tiruan. Kedua, folder `src` berisi program intinya. Ketiga, folder `scripts`
> berisi alat bantu supaya seluruh langkah bisa diulang dengan satu perintah. Keempat, folder
> `results` berisi semua hasil. Dataset framenya kami simpan terpisah di disk Linux karena
> ukurannya hampir 3 GB."

---

## 2. Alur besar (±1,5 menit)

**Tunjukkan:** diagram ini (bisa disalin ke slide).

```
 [1] DATASET      24fps_4096x2160.mp4
                        │  scripts/extract_dataset.sh (FFmpeg)
                        ▼
                  frame PPM: 720p (300) · 1080p (150) · 4K (50)          -> /data/frames
 [2] LINGKUNGAN   docker/Dockerfile + compose.yml -> node: single, node1–node4
                  tests/mpi_hello.c -> uji komunikasi antar-node
 [3] PROGRAM      make -> bin/v0, bin/v1, bin/v2
                  ┌────────────────────────────────────────────────────────────┐
                  │ muat frame ke RAM                                          │
                  │ PEWAKTU MULAI                                              │
                  │   setiap frame: grayscale -> blur -> Sobel -> magnitudo    │
                  │ PEWAKTU BERHENTI    (diulang: 1 warm-up + 5 run)           │
                  │ checksum + laporan (waktu, FPS) + 1 baris CSV              │
                  └────────────────────────────────────────────────────────────┘
 [4] VERIFIKASI   scripts/verify.sh + compare.py -> V1 = V0? selisih V2? frame contoh
 [5] EKSPERIMEN   scripts/run_experiments.sh -> results/results.csv (24 konfigurasi)
 [6] ANALISIS     scripts/analyze.py -> results/summary.md + 4 grafik
 [7] DEMO         scripts/make_video.sh -> results/video/edge_*.mp4
```

**Narasi:**
> "Alurnya tujuh langkah. Video dipecah dulu menjadi frame mentah dalam tiga resolusi.
> Lalu kami menyiapkan cluster tiruan dengan Docker dan menguji komunikasinya. Setelah itu
> ketiga versi program dikompilasi. Setiap program bekerja dengan pola yang sama: memuat
> frame ke RAM, menyalakan pewaktu, memproses semua frame, mematikan pewaktu, lalu mencatat
> hasilnya. Hasil setiap versi diverifikasi terhadap V0, kemudian semua konfigurasi
> dijalankan dan dicatat di CSV. Terakhir, CSV diolah menjadi tabel dan grafik, dan frame
> hasilnya dirangkai menjadi video."

---

## 3. Lingkungan: cluster dalam satu laptop (±1,5 menit)

**Tunjukkan:** [docker/compose.yml](docker/compose.yml), lalu [docker/Dockerfile](docker/Dockerfile).

| Container | Peran | Core |
|---|---|---|
| `single` | node untuk V0 dan V1 | 8 core (CPU 0,2,4,...,14) |
| `node1` – `node4` | node untuk V2 | 2 core per node |

**Narasi:**
> "Kami tidak punya akses ke cluster fisik, jadi cluster kami tiru dengan Docker. Setiap
> container berperan sebagai satu compute node: punya hostname, IP, dan proses sendiri, dan
> berkomunikasi lewat jaringan virtual Docker, seperti komputer-komputer di cluster sungguhan.
> Setiap node diberi core sendiri lewat `cpuset`. Di WSL, CPU 0 dan 1 adalah dua hyperthread
> dari core yang sama, jadi kami hanya memakai CPU bernomor genap agar setiap thread mendapat
> core sendiri.
>
> Di Dockerfile kami memasang gcc, Open MPI, dan FFmpeg, serta membuat kunci SSH yang sama di
> semua node. Kunci ini dibutuhkan karena `mpirun` memakai SSH untuk menjalankan proses di
> node lain. File `hostfile` berisi daftar node untuk `mpirun`, satu rank MPI per node.
>
> Keterbatasannya: semua node sebenarnya berbagi CPU dan memori laptop yang sama, dan jaringan
> virtualnya jauh lebih cepat daripada jaringan cluster sungguhan."

**Tunjukkan (opsional):** hasil uji [tests/mpi_hello.c](tests/mpi_hello.c): empat rank berjalan di
node1–node4, dan token yang dikirim berkeliling kembali dengan nilai 4 (OK).

---

## 4. Dataset (±1 menit)

**Tunjukkan:** [scripts/extract_dataset.sh:36-38](scripts/extract_dataset.sh#L36-L38).

```bash
extract 720p  1280  720 300
extract 1080p 1920 1080 150
extract 4k    3840 2160  50
```

**Narasi:**
> "Video MP4 itu terkompresi, jadi FFmpeg membukanya lalu menyimpan setiap frame sebagai
> gambar mentah PPM, yaitu tiga angka R, G, B per piksel. Untuk 1080p, setiap kotak 2×2
> piksel 4K dirata-rata menjadi satu piksel. Untuk 720p, kotaknya 3×3. Semua tingkatan
> diambil dari awal klip yang sama, sehingga isinya sama dan yang berbeda hanya ukuran
> bebannya. Skrip ini juga memeriksa jumlah dan ukuran setiap file. Langkah ini dikerjakan
> sekali saja dan tidak ikut diukur."

---

## 5. Kode bersama: `common.c` (±2,5 menit)

**Tunjukkan:** [src/common.c](src/common.c). File ini terbagi menjadi tiga bagian.

| Bagian | Fungsi | Guna |
|---|---|---|
| **I/O** (di luar pewaktu) | `ppm_read`, [`dataset_load`](src/common.c#L67), [`write_outputs`](src/common.c#L115) | membaca frame PPM ke RAM, menulis peta tepi PGM |
| **Pipeline** (yang diukur) | [`to_grayscale`](src/common.c#L144), [`box_blur3`](src/common.c#L155), [`sobel_naive`](src/common.c#L181), [`process_frame_v0`](src/common.c#L204) | empat tahap deteksi tepi |
| **Utilitas** | [`checksum_frames`](src/common.c#L226), [`median`](src/common.c#L239), [`parse_opts`](src/common.c#L260), [`report`](src/common.c#L291) | checksum, median 5 run, opsi `-d -n -t ...`, laporan + CSV |

**Tahap 1, grayscale.** Rumus 0,299R + 0,587G + 0,114B ditulis dengan bilangan bulat:

```c
gray[i] = (uint8_t)((77 * p[0] + 150 * p[1] + 29 * p[2] + 128) >> 8);
```

**Tahap 2, box blur 3×3:** jumlah 9 piksel tetangga dibagi 9.

**Tahap 3–4, Sobel 2D + magnitudo** (yang dipakai V0 dan V1):

```c
for (int ky = -1; ky <= 1; ky++) {
    for (int kx = -1; kx <= 1; kx++) {
        int p = src[(size_t)(y + ky) * w + (x + kx)];
        gx += KX[ky + 1][kx + 1] * p;     /* kernel Gx */
        gy += KY[ky + 1][kx + 1] * p;     /* kernel Gy */
    }
}
int m = (int)(sqrtf((float)(gx * gx + gy * gy)) + 0.5f);   /* |G| = sqrt(Gx² + Gy²) */
d[x] = (uint8_t)(m > 255 ? 255 : m);                       /* clamp ke 0..255 */
```

**Narasi:**
> "Semua fungsi yang dipakai lebih dari satu versi kami taruh di `common.c`, supaya
> perbandingannya adil. Isinya tiga bagian. Bagian I/O membaca dan menulis frame; bagian ini
> dipanggil sebelum dan sesudah pewaktu, jadi kecepatan disk tidak ikut terukur. Bagian
> pipeline berisi empat tahap: grayscale dengan aritmetika bilangan bulat, box blur 3×3 untuk
> mengurangi noise, lalu Sobel. Sobel di sini adalah versi langsung: untuk setiap piksel
> program melihat 9 tetangga, mengalikannya dengan kernel Gx dan Gy, totalnya 18 perkalian,
> lalu menghitung magnitudo dengan akar kuadrat. Nilai di atas 255 dipotong menjadi 255.
>
> Bagian utilitas berisi checksum untuk membuktikan dua keluaran identik, median dari lima
> pengukuran, dan fungsi `report` yang mencetak waktu, FPS, serta status real-time, lalu
> menambahkan satu baris ke CSV."

---

## 6. Versi 0, sekuensial (±1 menit)

**Tunjukkan:** [src/v0_seq.c:33-38](src/v0_seq.c#L33-L38).

```c
double t0 = omp_get_wtime();                       /* MULAI PEWAKTU */

for (int i = 0; i < n; i++)
    process_frame_v0(ds.rgb[i], out + (size_t)i * npx, gray, blur, w, h);

double t = omp_get_wtime() - t0;                   /* PEWAKTU DIMATIKAN */
```

**Narasi:**
> "V0 adalah patokan. Semua frame dimuat ke RAM dulu, baru pewaktu dinyalakan. Setelah itu
> satu thread memproses frame satu per satu. Pengukuran diulang: satu kali pemanasan yang
> tidak dihitung, lalu lima kali pengukuran, dan yang dilaporkan median. Waktu V0 ini menjadi
> t_s untuk menghitung speedup, dan keluarannya menjadi acuan kebenaran untuk versi lain."

---

## 7. Versi 1, OpenMP single-node (±1,5 menit)

**Tunjukkan:** [src/v1_omp.c:46-54](src/v1_omp.c#L46-L54).

```c
#pragma omp parallel for
for (int i = 0; i < n; i++) {
    int t = omp_get_thread_num();
    process_frame_v0(ds.rgb[i], out + (size_t)i * npx, gray[t], blur[t], w, h);
}
```

**Narasi:**
> "V1 hanya menambahkan satu baris: `#pragma omp parallel for`. Loop frame dibagi ke beberapa
> thread, dan setiap thread memproses frame secara utuh. Perhitungan di dalam frame sama persis
> dengan V0, karena fungsi yang dipanggil sama, yaitu `process_frame_v0`.
>
> Ada dua hal penting untuk mencegah race condition. Pertama, setiap frame menulis ke bagian
> keluarannya sendiri, `out + i * npx`, jadi tidak ada dua thread yang menulis ke tempat yang
> sama. Kedua, buffer kerja `gray` dan `blur` disediakan per thread, ditandai indeks `[t]`.
> Kalau buffer ini dipakai bersama, satu thread bisa menimpa hasil grayscale thread lain di
> tengah jalan. Karena perhitungannya sama, keluaran V1 harus identik dengan V0, dan itu kami
> buktikan dengan checksum."

---

## 8. Versi 2, hybrid MPI–OpenMP teroptimasi (±3 menit)

**Tunjukkan:** [src/v2_hybrid.c](src/v2_hybrid.c). Bawakan dalam lima langkah.

### 8a. Pembagian frame antar node ([baris 96-99](src/v2_hybrid.c#L96-L99))

```c
const int base = N / size, rem = N % size;
const int my_count = base + (rank < rem ? 1 : 0);
const int my_first = rank * base + (rank < rem ? rank : rem);
```

> "Setiap proses MPI, atau rank, berjalan di satu node. Frame dibagi per blok berurutan
> dengan selisih paling banyak satu frame. Contohnya 150 frame untuk 4 node: node1 mendapat
> frame 1–38, node2 39–76, node3 77–113, dan node4 114–150."

### 8b. Setiap node memuat bagiannya sendiri ([baris 103](src/v2_hybrid.c#L103))

> "Setiap node hanya memuat frame miliknya ke memori lokal, sebelum pewaktu. Lalu
> `MPI_Allreduce` memastikan semua node berhasil memuat datanya."

### 8c. Pengukuran waktu ([baris 139-152](src/v2_hybrid.c#L139-L152))

```c
MPI_Barrier(MPI_COMM_WORLD);                       /* seluruh rank siap */
double t0 = MPI_Wtime();                           /* MULAI PEWAKTU */

#pragma omp parallel for                           /* OpenMP di dalam node */
for (int i = 0; i < my_count; i++) {
    int t = omp_get_thread_num();
    process_frame_v2(ds.rgb[i], out + (size_t)i * npx, gray[t], blur[t], ring[t], w, h);
    done[my_first + i]++;                          /* catat: frame ini sudah diproses */
}

double t_local = MPI_Wtime() - t0;                 /* PEWAKTU DIMATIKAN */
MPI_Reduce(&t_local, &t_global, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
```

> "Di sinilah 'hybrid' terlihat. MPI membagi frame antar node, lalu di dalam setiap node
> OpenMP membagi frame tersebut ke dua thread. Sebelum pewaktu, `MPI_Barrier` menunggu semua
> node siap, supaya semuanya mulai bersamaan. Setiap node mengukur waktunya sendiri, lalu
> `MPI_Reduce` dengan operasi MAX mengambil waktu node yang paling lama, karena pekerjaan baru
> selesai kalau node terakhir selesai."

### 8d. Optimasi: Sobel separable + |Gx|+|Gy| ([baris 30-71](src/v2_hybrid.c#L30-L71))

```c
/* disederhanakan dari kode asli agar mudah dibaca di slide */
/* lintasan horizontal (sekali per piksel) */
dx[x] = row[x + 1] - row[x - 1];                   /* turunan  [-1, 0, 1] */
sx[x] = row[x - 1] + 2 * row[x] + row[x + 1];      /* penghalus [1, 2, 1] */

/* lintasan vertikal: memakai ulang 3 baris hasil horizontal */
int gx = dx[up][x] + 2 * dx[mid][x] + dx[dn][x];   /* penghalus vertikal [1, 2, 1] */
int gy = sx[dn][x] - sx[up][x];                    /* turunan vertikal [-1, 0, 1] */
int m  = |gx| + |gy|;                              /* tanpa kuadrat dan akar */
```

> "Kernel Sobel 3×3 bisa dipecah menjadi dua kernel 1D: [1, 2, 1] untuk menghaluskan dan
> [-1, 0, 1] untuk turunan. Jadi V2 menghitung lintasan horizontal sekali per baris, lalu
> hasilnya dipakai ulang oleh tiga baris keluaran pada lintasan vertikal. Hasil antaranya
> disimpan di buffer kecil berisi tiga baris (`ring`), sehingga muat di cache. Magnitudonya
> juga diganti |Gx| + |Gy|, tanpa kuadrat dan akar. Kami sudah menguji bahwa Gx dan Gy versi
> separable sama persis dengan versi 2D. Yang membuat hasil V2 sedikit berbeda dari V0
> hanyalah rumus magnitudonya."

### 8e. Verifikasi di dalam program ([baris 163-165](src/v2_hybrid.c#L163-L165))

> "Setiap kali sebuah frame selesai, hitungan `done` untuk frame itu ditambah satu. Di akhir,
> semua hitungan dijumlahkan ke rank 0 dengan `MPI_Reduce`. Setiap frame harus bernilai tepat
> 1, artinya tidak ada frame yang terlewat dan tidak ada yang diproses dua kali. Checksum dari
> semua node juga dijumlahkan, sehingga hasil V2 dengan 1, 2, maupun 4 node bisa dicek sama."

---

## 9. Verifikasi kebenaran (±1 menit)

**Tunjukkan:** [scripts/verify.sh](scripts/verify.sh), [scripts/compare.py](scripts/compare.py),
dan gambar di `results/samples/` (kiri ke kanan: asli | V0 | V2).

| Pemeriksaan | Hasil |
|---|---|
| V1 identik dengan V0 di setiap frame | ✅ 300/300, 150/150, 50/50 frame identik |
| Selisih rata-rata magnitudo V2 terhadap V0 (skala 0–255) | 6,2 (720p) · 5,2 (1080p) · 3,6 (4K) |
| Setiap frame V2 diproses tepat satu kali | ✅ OK di semua resolusi |

**Narasi:**
> "Skrip verifikasi menjalankan ketiga versi, menyimpan peta tepinya, lalu `compare.py`
> membandingkannya byte per byte dengan V0. V1 identik di semua frame. V2 berbeda rata-rata
> hanya 3 sampai 6 dari skala 255, dan selalu sedikit lebih terang, karena |Gx| + |Gy| selalu
> lebih besar atau sama dengan akar Gx² + Gy². Secara visual peta tepinya sama, seperti
> terlihat di frame contoh ini."

---

## 10. Eksperimen (±1 menit)

**Tunjukkan:** [scripts/run_experiments.sh](scripts/run_experiments.sh), lalu `results/results.csv`.

| Versi | Konfigurasi | × 3 resolusi |
|---|---|---|
| V0 | 1 thread | 3 |
| V1 | 1, 2, 4, 8 thread | 12 |
| V2 | 1, 2, 4 node × 2 thread per node | 9 |
| **Total** | | **24 konfigurasi × (1 warm-up + 5 run)** |

**Narasi:**
> "Skrip ini menjalankan 24 konfigurasi sesuai proposal. Setiap program menambahkan satu baris
> ke CSV: versi, resolusi, jumlah node dan thread, median waktu, waktu per frame, FPS,
> checksum, dan kelima waktu mentahnya. Pengukuran kami lakukan dengan laptop tersambung
> charger dan power mode Best performance. Di mode Balanced, Windows sempat memindahkan proses
> ke core lambat, sehingga waktunya tidak stabil."

---

## 11. Analisis dan grafik (±1 menit)

**Tunjukkan:** [scripts/analyze.py](scripts/analyze.py), lalu [results/summary.md](results/summary.md) dan grafik.

```
Speedup           S = t_s / t_p          t_s = median waktu V0
Efisiensi         E = S / p x 100%       p = jumlah thread (V1) atau node x thread (V2)
Computation cost  = t_p x p
FPS               = jumlah frame / waktu
```

| Grafik | Isi |
|---|---|
| `fig_v1_scaling.png` | V1: waktu, speedup, efisiensi terhadap jumlah thread |
| `fig_v2_scaling.png` | V2: waktu dan speedup terhadap jumlah node, dengan garis ideal |
| `fig_fps.png` | FPS per resolusi dengan garis target 30 FPS |
| `fig_final.png` | Perbandingan akhir V0, V1, V2 pada konfigurasi terbaik |

**Narasi:**
> "Program C hanya mencatat waktu. Semua metrik dihitung di `analyze.py` dari CSV, dengan rumus
> sesuai materi kuliah. Hasilnya tabel `summary.md` dan empat grafik."

---

## 12. Video hasil (±30 detik)

**Tunjukkan:** putar `results/video/edge_1080p.mp4`.

**Narasi:**
> "Terakhir, frame peta tepi hasil V2 dirangkai kembali menjadi video dengan FFmpeg. Langkah
> ini hanya untuk demonstrasi dan tidak ikut diukur."

---

## 13. Demo langsung (opsional, ±2 menit)

Persiapan sebelum presentasi: charger tersambung, power mode **Best performance**, terminal
**Ubuntu (WSL)** terbuka, dan cluster menyala.

```bash
cd "/mnt/d/T-1/Smstr 7/High Programming Computing (HPC)/Project"
DC="docker compose -f docker/compose.yml"
$DC up -d

# V0: 1 thread
$DC exec -u mpi single bin/v0 -d /data/frames/4k -n 50
# V1: 8 thread
$DC exec -u mpi single bin/v1 -d /data/frames/4k -n 50 -t 8
# V2: 4 node x 2 thread
$DC exec -u mpi node1 mpirun -np 4 --hostfile docker/hostfile --bind-to none \
    bin/v2 -d /data/frames/4k -n 50 -t 2
```

**Narasi:**
> "Kami jalankan langsung untuk 4K. V0 sekitar 3 detik, atau 16,8 FPS, jadi belum real-time.
> V1 dengan 8 thread sekitar 0,66 detik, 76 FPS. V2 dengan 4 node sekitar 0,36 detik,
> 141 FPS. Perhatikan juga checksum V1 yang sama dengan V0, dan baris verifikasi pembagian
> frame V2 yang bernilai OK."

Angka saat demo bisa sedikit berbeda dari tabel, karena setiap run punya variasi kecil.

---

## 14. Antisipasi pertanyaan

| Pertanyaan | Jawaban singkat |
|---|---|
| Kenapa buffer `gray`/`blur` dibuat per thread? | Kalau dipakai bersama, thread saling menimpa hasil antara (race condition), sehingga keluarannya salah. |
| Kenapa ada `MPI_Barrier` sebelum pewaktu? | Supaya semua node mulai bersamaan setelah datanya siap, sehingga waktu pemuatan tidak ikut terukur. |
| Kenapa waktu V2 diambil MAX? | Pekerjaan baru selesai ketika node paling lambat selesai. |
| Kenapa V2 lebih cepat dari V1 dengan jumlah thread sama? | Karena algoritmanya, bukan MPI. Sobel separable sekitar 4× lebih cepat dari Sobel 2D (12,9 → 3,3 ms per frame 1080p), sehingga total per frame turun sekitar 2,3×. |
| Kenapa efisiensi V2 lebih dari 100%? | Speedup V2 dihitung terhadap V0, dan V2 juga memakai algoritma yang lebih cepat. Itu bukan speedup superlinear sungguhan. |
| Kenapa V1 dengan 8 thread tidak 8× lebih cepat? | Laptop hanya punya 6 core cepat (P-core), clock CPU turun ketika banyak core aktif, dan semua thread berbagi bandwidth memori. |
| Apakah Docker sama dengan cluster sungguhan? | Kode dan cara komunikasinya sama (MPI lewat TCP), tetapi CPU dan memorinya satu laptop. Ini kami catat sebagai keterbatasan. |
| Apakah komunikasi MPI jadi penghambat? | Di pengukuran tidak, karena data sudah dimuat sebelum pewaktu. Di jaringan nyata (misalnya LAN 1 Gbps), mengirim frame bisa lebih lama daripada memprosesnya. |
| Kenapa memakai median? | Median tidak terpengaruh satu atau dua run yang kebetulan lambat karena gangguan sistem operasi. |

---

## Penutup (±20 detik)

**Narasi:**
> "Jadi, program kami terdiri dari tiga versi yang memakai fungsi bersama yang sama. V1
> menambahkan paralelisme antar-frame dengan OpenMP, sedangkan V2 menambahkan pembagian kerja
> antar-node dengan MPI dan optimasi algoritma Sobel separable. Seluruh langkah, dari
> ekstraksi dataset sampai grafik, bisa diulang dengan skrip yang ada di folder `scripts`."
