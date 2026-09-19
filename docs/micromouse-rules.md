# Micromouse Competition Rules — Reference

Ringkasan aturan umum kompetisi micromouse. Aturan detail (waktu, jumlah run,
skor) sedikit berbeda antar penyelenggara (IEEE, UKMARS, All Japan, APEC,
dll) — dokumen ini mencatat aturan yang paling umum dipakai plus perbedaan
antar sumber saat relevan.

## 1. Apa itu micromouse

Robot otonom yang harus menjelajahi labirin (maze) tak dikenal dari titik
start ke sel goal secepat mungkin, sepenuhnya mandiri (tanpa remote control
atau bantuan dari luar setelah run dimulai).

## 2. Jenis kompetisi

| | Classic | Half-size |
|---|---|---|
| Ukuran sel | 18cm × 18cm | 9cm × 9cm |
| Grid maze | 16 × 16 | hingga 32 × 32 |
| Tinggi dinding | 5cm | 2.5cm |
| Tebal dinding | 1.2cm | 0.6cm |
| Lebar lorong (passage) | 16.8cm | 8.4cm |
| Lokasi goal | tetap di 4 sel tengah | ditentukan panitia per kompetisi, bisa di mana saja, ukurannya diumumkan sebelum lomba |
| Ukuran maksimal robot | 25cm × 25cm (tinggi bebas) | 25cm × 25cm (tinggi bebas) — beberapa sumber/edisi lama mencatat batas lebih kecil (12.5cm), cek aturan spesifik kompetisi yang diikuti |

Half-size mulai diperkenalkan di All Japan Micromouse Competition ke-30
(2009). Selain classic & half-size, ada juga variasi tambahan di beberapa
kompetisi (mis. kategori "wall-following only" untuk pemula), tapi classic
dan half-size adalah dua kategori utama yang paling umum.

## 3. Konstruksi maze

- **Lantai**: kayu, dicat/vernis hitam doff (matte) — permukaan ini menyerap
  cahaya inframerah.
- **Dinding**: sisi putih, bagian atas merah — permukaan ini memantulkan
  cahaya inframerah. Inilah alasan sensor IR reflektif (seperti
  TEFT4300+SFH4545 yang dipakai di board kita) bisa membedakan "ada
  dinding" vs "lorong kosong" berdasarkan pantulan.
- **Post/tiang sudut**: 1.2cm × 1.2cm × 5cm (classic), berdiri di tiap
  perpotongan sudut sel, terlepas dari ada/tidaknya dinding di sel itu.
- **Start**: di salah satu dari 4 sudut maze, dinding menutup 3 sisi
  (biasanya sisi barat & selatan adalah dinding luar maze, sisi yang
  terbuka menghadap utara).
- **Goal**: classic = 4 sel tengah persis (2×2), tanpa dinding/post di
  dalam area goal. Half-size = area ditentukan panitia, diumumkan sebelum
  lomba, tidak harus di tengah.
- Maze dijamin dapat diselesaikan (ada jalur dari start ke goal), tapi
  aturan tidak selalu menjamin maze "simply-connected" (bebas loop
  tertutup) — ini relevan untuk pemilihan algoritma (lihat catatan di
  README project soal kelemahan wall-following terhadap maze berloop).

## 4. Batasan robot

- **Ukuran**: harus selalu muat dalam bujur sangkar 25cm × 25cm dilihat
  dari atas (kalau robot berubah bentuk saat jalan, tetap tidak boleh
  melebihi ini kapan pun). Tidak ada batas tinggi.
- **Sumber daya**: baterai (self-contained), dilarang pakai motor
  pembakaran/combustion.
- **Otonomi penuh**: tidak boleh menerima bantuan/kontrol dari luar
  setelah run dimulai (tidak remote control).
- **Dilarang merusak maze**: tidak boleh melompat, memanjat, menggores,
  atau merusak dinding/lantai maze. Metode sensor yang berisiko merusak
  permukaan maze juga dilarang.
- **Komponen tidak boleh diganti** selama kompetisi berlangsung (kecuali
  perbaikan minor/ganti baterai sesuai izin panitia).
- Robot butuh hook/loop yang aman untuk diangkat panitia dari tengah maze
  setelah selesai run.

## 5. Misi & alur satu sesi kompetisi

1. Robot ditempatkan di start cell.
2. Selama waktu yang dialokasikan (lihat §6), robot boleh melakukan
   beberapa **run** (percobaan) — biasanya mulai dengan eksplorasi (belum
   tahu peta maze), lalu run berikutnya makin cepat karena sudah punya
   peta dari run sebelumnya (inilah kenapa flood fill/algoritma pemetaan
   jauh lebih unggul dari sekadar wall-following: bisa "belajar" dan
   mengoptimalkan run lanjutan).
3. Satu run dianggap valid/dihitung waktunya dari saat robot meninggalkan
   start cell sampai masuk ke goal cell.
4. Kalau baterai/memori di-reset atau ada penyesuaian signifikan, peta
   maze yang tersimpan di robot **harus dihapus** sebelum mulai lagi.
5. Skor akhir biasanya diambil dari **run tercepat yang sah** selama sesi
   (bukan run pertama/terakhir).

## 6. Batas waktu & jumlah run

Bervariasi per penyelenggara — dua contoh (UKMARS, 2020-an):

| | Classic | Half-size |
|---|---|---|
| Total waktu di maze | 10 menit (bisa dipangkas ke 7 menit kalau peserta banyak) | 10 menit (bisa dipangkas ke 5 menit) |
| Jumlah run maksimal | 5 | 5 |
| Kondisi run selesai | robot kembali ke start & berhenti ≥2 detik | serupa |

Sumber-sumber lain (mis. rangkuman historis di Micromouse Online) mencatat
angka berbeda (15 menit/10 run, atau format "handicapped time" dengan
formula skor — lihat §7) — **selalu cek buku aturan resmi kompetisi yang
diikuti**, karena angka ini bukan standar tunggal yang seragam di semua
event.

## 7. Skor (bervariasi per penyelenggara)

**Format sederhana (umum di UKMARS/kompetisi modern):** skor = waktu run
tercepat yang sah. Tidak ada penalti tambahan di luar itu.

**Format "handicapped time" (dipakai beberapa kompetisi, termasuk
sebagian aturan historis IEEE):**

```
Handicapped Time = Run Time + Search Penalty + Touch Penalty

Search Penalty = (total waktu eksplorasi di maze, detik) / 30
Touch Penalty  = 3 detik + (run time / 10)   — hanya berlaku kalau robot
                 disentuh/diangkat manual sebelum run tersebut
```

Tujuan formula ini: memberi "bonus" ke robot yang bisa memetakan maze
lebih cepat (search time kecil), dan menghukum robot yang butuh bantuan
manual (disentuh panitia/operator) di tengah sesi.

## 8. Wasit & diskualifikasi

- Wasit boleh minta robot berhenti lebih awal kalau progressnya jelas
  tidak akan berhasil, atau perilakunya berisiko merusak maze.
- Robot tidak boleh diulang/restart cuma karena salah belok — kecuali
  atas izin wasit.
- Kalau tidak ada run yang berhasil sama sekali, penjurian bisa tetap
  menilai secara kualitatif: seberapa jauh progress, "purposefulness"
  (gerakan terarah vs acak), dan kualitas kontrol robot.

## Sumber

- [UKMARS — Micromouse Classic Rules](https://ukmars.org/contests/contest-rules/micromouse-classic/)
- [UKMARS — Micromouse Half Size Rules](https://ukmars.org/contests/contest-rules/micromouse-half-size/)
- [Micromouse Online — Rules (rangkuman historis/umum)](https://micromouseonline.com/micromouse-book/rules/)
- [IEEE R2 SAC MicroMouse Competition Rules 2023 (PDF)](https://www.marshall.edu/cecs/files/MicroMouse_Rules_2023.pdf)
- [Wikipedia — Micromouse](https://en.wikipedia.org/wiki/Micromouse)
