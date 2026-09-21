# micromouse

Robot micromouse (maze-solving robot). Target hardware: **STM32F401CCU6**
(WeAct Blackpill). Repo git sendiri, terpisah dari `PROJEK/` (yang bukan git
repo) — remote: https://github.com/zrhnd/simulator-micromouse (branch `main`).

## Struktur

```
micromouse/
  README.md          ringkasan project untuk GitHub
  CLAUDE.md           file ini
  docs/
    micromouse-rules.md   aturan kompetisi (maze, robot, misi, skor) — referensi
  hardware/
    BOM_Board_MICROMOUSE.csv
    SCH_MICROMOUSE.png     skematik EasyEDA, board "Board1"
  sim/
    algorithms/
      flood-fill-c/   ALGORITMA AKTIF — flood fill (BFS) di C, sudah dikomentari
                       lengkap untuk belajar. Ini yang dipakai, bukan mms-cpp.
      mms-cpp/        template C++ kosong, disimpan sebagai referensi kalau
                       nanti porting firmware pakai C++ (belum dipakai)
      q-learning/     EKSPERIMEN — Q-learning di Python, hanya untuk dicoba di
                       simulator (bukan kandidat firmware/robot nyata, jadi
                       sengaja Python bukan C). Lihat catatan di bawah.
    mms-app/          simulator mms siap pakai (mms.exe + DLL, Windows)
    mms-src/          source code simulator mms, untuk dibaca/referensi
    mazes/mazefiles/  522 maze kompetisi asli (All Japan, APEC, AAMC, dst),
                       classic (16x16) / halfsize (32x32) / training
  firmware/           BELUM ADA — rencana kode STM32F401CCU6 asli,
                       masih di device lain milik user
```

Semua folder di atas (termasuk `sim/mms-app`, `sim/mms-src`,
`sim/mazes/mazefiles`) **di-track di git**, bukan di-gitignore — user minta
repo GitHub-nya self-contained. `.gitignore` cuma exclude build artifact
(`sim/algorithms/**/*.exe`, `.o`, `.obj`).

## Toolchain

- **Compiler C/C++**: MinGW-w64 (WinLibs, `gcc`/`g++`) ter-install
  system-wide via winget (`BrechtSanders.WinLibs.POSIX.UCRT`). PATH sistem
  sudah update, tapi shell yang sudah lama terbuka perlu direstart/PATH
  di-refresh manual dulu sebelum `gcc`/`g++` kekenali.
- **Git identity**: pakai global config user (`zurinhnd` / `zurinhnd@gmail.com`),
  tidak ada override lokal di repo ini.

## Cara pakai simulator

1. Jalankan `sim/mms-app/mms/mms.exe`.
2. Tambah algoritma baru (tombol **+**), isi:
   - Directory: `sim/algorithms/flood-fill-c`
   - Build Command: `gcc -std=c11 -O2 -o algo.exe main.c solver.c API.c queue.c`
   - Run Command: **path lengkap/absolut** ke `algo.exe` (di Windows, mms
     tidak bisa resolve nama file relatif untuk Run Command — harus full path)
3. Pilih maze (default, atau buka file dari `sim/mazes/mazefiles/classic/`),
   klik Run.

`mms` sendiri **hardcode goal = tengah geometris maze** (lihat
`Maze::getCenterPositions()` di `mms-src`) — tidak ada cara mengatur target
custom lewat file maze atau UI, walau maze half-size aslinya boleh goal di
mana saja. Jadi simulator ini cuma cocok buat latihan gaya "classic".

## flood-fill-c — catatan implementasi

- `mazeWidth`/`mazeHeight` diambil otomatis dari `API_mazeWidth()`/
  `API_mazeHeight()` saat `initialize()` — bukan hardcode 16. Array kapasitas
  tetap `MAZE_MAX_SIZE=32` (cukup untuk classic & halfsize).
- Aturan goal (`resetDistances()`) mengikuti persis logika `mms` sendiri:
  4 sel kalau width & height genap, 2 sel kalau salah satu genap, 1 sel
  kalau ganjil-ganjil.
- Run-output: satu baris log per langkah lewat `debug_log()` (muncul di
  panel log simulator), format:
  `F:<wall/open> R:<wall/open> L:<wall/open>\tACTION: <label>\tMOVE-TO: [x,y]\tREROUTE: <kosong atau "lama->baru">`
  REROUTE terisi kalau dinding baru bikin jarak-ke-goal dari sel sekarang
  naik dibanding sebelumnya (rencana lama kena blokir).
- `showPath()` mewarnai (kuning) jalur terpendek yang sedang diketahui dari
  posisi robot ke goal, update tiap langkah.
- **Known issue (belum dibenahi)**: `queue_create()` di `updateDistances()`
  tidak pernah di-`queue_destroy()` — memory leak kecil tiap langkah. Tidak
  masalah di simulator PC, tapi WAJIB dibenahi sebelum porting ke firmware
  STM32 (RAM jauh lebih terbatas).
- Ada sisa dead code di `updateMaze()` (variabel `north` tidak dipakai,
  komentar `// REMOVE LATER`) — aman diabaikan/dibersihkan kapan saja.

## q-learning — catatan implementasi

- Eksperimen, bukan algoritma aktif kompetisi (itu tetap flood-fill-c).
  Python dipilih (bukan C) karena tujuannya khusus dicoba di simulator `mms`,
  bukan untuk di-porting ke firmware STM32.
- State = posisi sel (x,y) saja; heading TIDAK masuk Q-table (aksi = arah
  mutlak N/E/S/W, robot berbelok otomatis sebelum maju) — state space jadi
  W×H×4 nilai-Q saja, bukan W×H×4×4.
- Training jalan lewat gerakan fisik sungguhan di `mms` (bukan simulasi
  virtual di memori): tiap episode robot benar-benar jalan dari start, lalu
  dipandu BFS (di atas peta yang sudah diketahui) untuk kembali ke start —
  jadi tidak perlu klik tombol Reset simulator berulang-ulang untuk setiap
  episode training (`wasReset()`/`ackReset()` cuma dipakai di akhir, untuk
  menunggu user reset manual kalau mau mengulang seluruh proses).
- Q-table + peta dinding yang sudah diketahui disimpan ke `qtable_state.pkl`
  (gitignored) supaya training lanjut dari sesi sebelumnya, bukan mulai dari
  nol tiap kali simulator dibuka ulang.
- **Bug nyata yang sudah diperbaiki (ditemukan dari crash log user, run
  pertama, maze 16x16, TANPA `qtable_state.pkl` lama sama sekali — jadi
  bukan soal state lama)**: `navigate_home()` dulu menghitung SATU rencana
  BFS di awal lalu mengeksekusi semua langkahnya membuta, tanpa sensor ulang
  di tiap langkah — beda dari `run_episode()` (dan dari `flood-fill-c`) yang
  selalu sensor+replan tiap langkah. Itu satu-satunya tempat yang melanggar
  invariant "hanya bergerak ke arah yang baru saja dikonfirmasi terbuka",
  dan bisa memicu `MouseCrashedError` yang sebelumnya tidak pernah ditangani
  (proses mati dengan traceback mentah). Sekarang `navigate_home()`
  sense+BFS ulang di SETIAP langkah, sama seperti `flood-fill-c`.
- **Bug terkait yang juga diperbaiki**: `run_episode()` dulu selalu asumsi
  mouse mulai menghadap `"N"`, padahal tidak ada reset sungguhan di `mms`
  antar episode (cuma jalan fisik pulang lewat `navigate_home()`) — kalau
  robot berhenti menghadap arah lain, asumsi `"N"` di episode berikutnya
  bikin heading yang dilacak Python desync dari heading asli robot, merusak
  semua pemetaan sensor depan/kanan/kiri/belakang→arah mutlak sesudahnya.
  Sekarang heading asli diteruskan (threaded) lewat seluruh loop di `main()`.
- **Proteksi tambahan (defense in depth, bukan fix utama)**: `qtable_state.pkl`
  dari maze lain yang KEBETULAN beda ukuran (lebar/tinggi) otomatis dibuang
  saat dimuat (`load_state()`), dan seluruh training/run akhir dibungkus
  `try/except MouseCrashedError` supaya kalau masih ada crash (mis. maze
  lain yang ukurannya SAMA tapi tata letaknya beda), programnya berhenti
  dengan pesan jelas alih-alih traceback mentah.
- Belum menggambar dinding yang baru disensor ke tampilan `mms` (tidak
  panggil `setWall`, beda dari flood-fill-c yang menggambar tiap step) —
  kosmetik saja, tidak memengaruhi korektnes algoritma.

## Hardware — catatan dari review skematik

- MCU: STM32F401CCU6 Blackpill. Footprint EasyEDA yang dipakai bernama
  `Blackpill_STM32F401CEU6` (beda varian chip) — **ini aman**, PCB modul
  Blackpill CCU6 dan CEU6 identik secara fisik (dimensi, pin header, USB-C),
  cuma beda kapasitas flash/RAM chip di dalamnya.
- Buzzer di BOM (`SEA-1295Y-0520-42Ω`, LCSC C2687681) itu elektromagnetik,
  **50mA rated** — melebihi batas arus GPIO STM32F401 (~25mA/pin). Kalau
  dipakai langsung tanpa transistor driver, berisiko rusak pin. User sedang
  test di breadboard dulu.
- Motor driver TB6612FNG: designator beda antara skematik (`U2`) dan BOM
  (`U14`) — belum direkonsiliasi, cuma masalah penomoran/paperwork.

## Konvensi kerja

- Selalu test-build (`gcc ...`) sebelum bilang perubahan kode selesai.
- Commit + push ke GitHub dilakukan atas persetujuan eksplisit user tiap kali
  (bukan otomatis tiap edit).
- Attribusi commit pakai identitas git yang sudah di-set user sendiri.
