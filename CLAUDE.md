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
