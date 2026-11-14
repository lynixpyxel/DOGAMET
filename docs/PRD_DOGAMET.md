# Product Requirements Document (PRD) — DOGAMET

**Versi:** 1.0 — finalisasi spesifikasi firmware awal  
**Basis desain:** `DOGAMET.html` + keputusan konfigurasi terbaru pengguna  
**Status:** siap menjadi acuan implementasi dan prototyping; belum merupakan bukti akurasi alat fisik.

## 1. Ringkasan produk

DOGAMET adalah alat ukur lingkar dada domba berbasis pita ukur dan roller. Saat pita ditarik atau dikembalikan, roller memutar magnet diametral yang dibaca oleh sensor sudut AS5600. Arduino Nano mengakumulasi perubahan sudut menjadi panjang pita, menampilkan lingkar dada (LD) pada LCD 16×2, menghitung estimasi bobot (BB) menggunakan persamaan Schoorl, lalu menyimpan hasil yang ditahan ke microSD.

Titik ukur mengikuti konsep pada berkas sumber: lingkar dada tepat di belakang kaki depan, dengan pita melingkar tegak lurus terhadap sumbu tubuh.

## 2. Keputusan konfigurasi yang dikunci

| Parameter | Keputusan |
|---|---|
| Mikrokontroler | Arduino Nano ATmega328P, 5 V / 16 MHz |
| Sensor putaran | AS5600, 12-bit, I²C 0x36 |
| Resolusi sudut | 4096 kode per putaran |
| Diameter roller | **24 mm** (nilai terbaru pengguna; menggantikan Ø20 mm pada model HTML) |
| Keliling nominal roller | π × 24 mm = 75,3982 mm = 7,53982 cm |
| Model estimasi bobot | **Schoorl:** BB (kg) = ((LD (cm) + 22)²) / 100 |
| PB | Dipilih manual, tetapi tidak digunakan oleh model Schoorl; lihat batasan di §8 |
| Arah putaran | **Signed**: putaran maju menambah LD, putaran mundur mengurangi LD |
| LCD | 16×2 I²C |
| Tombol | NOL, UKUR, SIMPAN |
| Penyimpanan | microSD, CSV |

## 3. Tujuan dan bukan tujuan

### Tujuan
- Mengukur perpindahan pita berdasarkan perubahan sudut roller.
- Menghasilkan nilai LD dalam cm dengan pembacaan yang stabil.
- Menampilkan estimasi bobot Schoorl sebagai estimasi, bukan bobot aktual.
- Memungkinkan operator menetapkan nol, menahan hasil, dan menyimpan hasil.
- Menyimpan rekaman terstruktur pada microSD.
- Menangani kondisi sensor atau SD yang gagal tanpa menyimpan data palsu.

### Bukan tujuan versi awal
- Menggantikan timbangan ternak.
- Mengklaim akurasi klinis atau validasi pada populasi domba tertentu.
- Mengukur PB secara otomatis.
- Menyediakan kalender/timestamp absolut tanpa modul RTC.
- Menyinkronkan data ke cloud atau aplikasi ponsel.

## 4. Pengguna dan skenario penggunaan

Operator menempatkan pita pada lingkar dada domba, tepat di belakang kaki depan. Pita ditarik hingga melingkari dada dengan posisi tegak lurus terhadap sumbu tubuh dan ketegangan yang konsisten. Operator menekan NOL pada posisi awal pita, melakukan pengukuran, menekan UKUR untuk membekukan nilai, lalu SIMPAN untuk menulis satu rekaman ke SD.

## 5. Kebutuhan fungsional

| ID | Kebutuhan | Kriteria penerimaan |
|---|---|---|
| FR-01 | Membaca sudut AS5600 melalui I²C | Pembacaan 0–4095 terbaca berulang tanpa blocking panjang |
| FR-02 | Mengurai wrap-around sudut | Transisi 4090→5 dianggap maju kecil; 5→4090 dianggap mundur kecil |
| FR-03 | Menghitung signed count | Arah maju menambah count dan arah mundur menguranginya |
| FR-04 | Mengonversi count menjadi panjang | LD = signed_count / 4096 × keliling roller |
| FR-05 | Menetapkan nol | Tombol NOL menetapkan posisi pita saat itu sebagai 0,00 cm dan menghapus status hold |
| FR-06 | Menghitung estimasi bobot | BB = ((LD+22)²)/100, LD dalam cm, hasil kg |
| FR-07 | Menampilkan hasil | LCD menampilkan LD, BB, serta indikator LIVE/HOLD atau status singkat |
| FR-08 | Menahan hasil | UKUR menyimpan snapshot LD dan BB; perubahan pita sesudahnya tidak mengubah snapshot |
| FR-09 | Melanjutkan live | Tekan UKUR saat HOLD untuk kembali ke LIVE |
| FR-10 | Menyimpan rekaman | SIMPAN hanya aktif/diterima pada status HOLD dan menulis tepat satu baris per tekan |
| FR-11 | Menangani SD gagal | LCD/status menunjukkan SD ERROR; jangan mengklaim data tersimpan |
| FR-12 | Menangani sensor gagal | Tampilkan SENSOR ERROR; jangan izinkan HOLD/SIMPAN atas nilai invalid |
| FR-13 | Menyediakan PB manual opsional | Firmware menerima PB melalui serial USB sebagai metadata opsional; PB tidak memengaruhi Schoorl |
| FR-14 | Menjaga rekaman | Nomor urut bertambah dan file CSV tidak ditimpa saat boot berikutnya |

## 6. Kebutuhan nonfungsional

- Firmware harus dapat dikompilasi untuk Arduino Nano ATmega328P.
- Hindari `delay()` panjang pada loop utama; tombol dan sensor harus tetap responsif.
- Gunakan tipe integer yang cukup lebar untuk akumulasi putaran (`int32_t` atau `int64_t`).
- Gunakan debounce tombol dan deteksi tepi tekan agar satu tekan hanya satu aksi.
- Hindari alokasi heap dinamis berulang; RAM Nano terbatas.
- Penulisan SD dilakukan saat SIMPAN, bukan setiap iterasi sensor.
- Tampilkan status kegagalan dengan jelas.
- Gunakan format angka yang ringkas agar muat pada LCD 16×2.

## 7. Pengukuran dan kalibrasi

### 7.1 Rumus nominal

Diameter roller terbaru = 24 mm = 2,4 cm.

- Keliling = π × 2,4 cm = **7,539822 cm/putaran**.
- Resolusi teoritis = 7,539822 / 4096 = **0,001841 cm/kode** = **0,01841 mm/kode**.
- Panjang = signed_count × 7,539822 / 4096 (cm).

Resolusi teoritis bukan akurasi. Akurasi aktual dipengaruhi diameter efektif roller, kompresi/keausan, slip pita, tekanan roller penekan, alignment magnet, dan cara operator menarik pita.

### 7.2 Unwrap sudut

Baca raw angle 12-bit. Hitung delta terhadap pembacaan sebelumnya:

```cpp
int16_t delta = (int16_t)rawNow - (int16_t)rawPrev;
if (delta > 2048)  delta -= 4096;
if (delta < -2048) delta += 4096;
signedCount += delta;
```

Asumsi: perubahan sudut sebenarnya antara dua sampel kurang dari setengah putaran. Sampling harus cukup sering sehingga asumsi ini tidak dilanggar. Saat sensor baru terdeteksi atau setelah error, inisialisasi ulang `rawPrev` agar tidak terjadi lonjakan count.

### 7.3 Kalibrasi diameter efektif

Ø24 mm adalah nilai awal dari pengguna, bukan nilai yang telah dikalibrasi. Uji beberapa panjang referensi (mis. 50 cm dan 100 cm) dengan pita lurus dan tegangan konsisten. Jika hasil sistematis meleset, hitung faktor kalibrasi:

`factor = panjang_referensi / panjang_terbaca`

Simpan faktor sebagai konfigurasi firmware/EEPROM setelah proses kalibrasi terverifikasi. Jangan mengubah diameter nominal diam-diam; dokumentasikan nilai kalibrasi terpisah.

## 8. Model estimasi bobot dan PB

### Model yang dipilih

`BB (kg) = ((LD (cm) + 22) × (LD (cm) + 22)) / 100`

Contoh: LD 90 cm → BB = (112²)/100 = **125,44 kg**. Contoh ini hanya demonstrasi aritmetika formula, bukan klaim kewajaran biologis. Periksa sumber dan kecocokan persamaan Schoorl untuk spesies, populasi, satuan, dan konteks penggunaan sebelum dipakai untuk keputusan peternakan.

### PB manual

Pilihan “PB manual” tidak diperlukan oleh persamaan Schoorl. Selain itu, rangkaian yang ditetapkan hanya memiliki tiga tombol tanpa keypad/encoder untuk memasukkan angka PB. Maka spesifikasi firmware awal adalah:

- PB menjadi **metadata opsional**, bukan parameter hitung bobot.
- Jika operator menghubungkan Nano ke komputer, PB dapat dimasukkan melalui Serial Monitor dengan perintah yang didokumentasikan (contoh: `PB=78.5`).
- PB terakhir dapat dilampirkan ke rekaman berikutnya; firmware harus menyediakan perintah untuk menghapusnya (`PB=CLEAR`).
- Jika tidak ada input PB, kolom PB pada CSV dibiarkan kosong.
- Untuk input PB langsung pada alat tanpa komputer, diperlukan hardware/UI tambahan (mis. rotary encoder + tombol, keypad, atau aplikasi eksternal). Itu di luar BOM saat ini.

**Penting:** formula Schoorl yang dipilih berbeda dari formula regresi pada HTML sumber (`BB = −79,640 + 1,369 × LD`). Firmware harus menggunakan Schoorl saja, dan dokumentasi/label perangkat harus menyebut bahwa ini estimasi.

## 9. Antarmuka pengguna

Tampilan yang disarankan:

- Baris 1: `LD:  90.0 cm H` (H=HOLD, L=LIVE jika muat)
- Baris 2: `BB: 125.4 kg`

Status alternatif saat error:
- `AS5600 ERROR`
- `SD ERROR`
- `TEKAN NOL`

Tombol:
- **NOL:** menetapkan count saat ini sebagai nol relatif; `signedCount=0`; keluar dari HOLD.
- **UKUR:** LIVE→HOLD mengambil snapshot; HOLD→LIVE melanjutkan pembacaan.
- **SIMPAN:** hanya menyimpan snapshot saat HOLD. Satu tekan menghasilkan satu rekaman.

## 10. Format CSV

Nama file: `DOGAMET.CSV` (append; header ditulis hanya jika file belum ada/kosong).

Header yang disarankan:

```csv
id,ld_cm,bb_kg,model_bb,pb_cm,signed_count,roller_diameter_mm,calibration_factor,elapsed_ms,source
```

- `id`: nomor urut rekaman.
- `ld_cm`: snapshot LD dalam cm.
- `bb_kg`: snapshot estimasi bobot kg.
- `model_bb`: `SCHOORL`.
- `pb_cm`: metadata opsional; kosong jika tidak diisi.
- `signed_count`: akumulasi count relatif terhadap nol.
- `roller_diameter_mm`: 24.0 atau nilai konfigurasi aktif.
- `calibration_factor`: faktor koreksi aktif.
- `elapsed_ms`: `millis()` saat simpan; bukan tanggal/waktu kalender.
- `source`: `DEVICE`.

Gunakan titik desimal untuk CSV dan jangan menulis koma desimal. Pastikan penulisan baris ditutup dengan newline dan file di-flush/close setelah simpan.

## 11. Pemetaan pin (rancangan awal)

| Fungsi | Pin Nano |
|---|---|
| I²C SDA (AS5600 + LCD) | A4 |
| I²C SCL (AS5600 + LCD) | A5 |
| NOL | D2 |
| UKUR | D3 |
| SIMPAN | D4 |
| microSD CS | D10 |
| microSD MOSI | D11 |
| microSD MISO | D12 |
| microSD SCK | D13 |

Tombol diasumsikan aktif-LOW: satu sisi ke pin, sisi lain ke GND, gunakan `INPUT_PULLUP`. D10 harus menjadi OUTPUT saat Nano bertindak sebagai SPI master. Pinout SD harus diverifikasi terhadap modul aktual.

## 12. Risiko dan hal yang perlu diverifikasi pada prototipe

1. **Level tegangan:** AS5600 IC menggunakan logika 3,3 V; berkas HTML mengusulkan BSS138 dan AMS1117. Pastikan breakout AS5600 tidak diasumsikan memiliki level shifter. Pastikan modul microSD memiliki regulator/level shifting yang sesuai; modul SD mentah 3,3 V tidak boleh langsung menerima logika 5 V.
2. **Diameter roller:** model HTML menyebut Ø20 mm, sedangkan konfigurasi terbaru Ø24 mm. Implementasi firmware harus memakai Ø24 mm sesuai keputusan terbaru dan beri komentar bahwa ini override.
3. **Sampling AS5600:** jika roller berputar terlalu cepat di antara dua pembacaan, unwrap bisa salah arah/jumlah.
4. **Slip:** gesekan pita–roller tidak ideal; butuh uji kalibrasi fisik.
5. **PB:** input manual belum memiliki kontrol fisik pada BOM saat ini.
6. **RTC:** tidak ada RTC pada daftar komponen; `millis()` bukan timestamp kalender dan akan reset saat daya mati.
7. **Formula:** verifikasi sumber primer dan validitas model Schoorl pada domba yang akan diukur; jangan menganggap estimasi sama dengan hasil timbangan.
8. **AS5600 magnet status:** jika memakai status magnet, implementasikan pemeriksaan magnet detected/too weak/too strong sesuai kemampuan library/register.

## 13. Kriteria penerimaan akhir

- Pembacaan sudut dapat melewati batas 4095↔0 tanpa lonjakan panjang.
- Gerakan maju meningkatkan LD; gerakan mundur mengurangi LD.
- Setelah NOL, LD mendekati 0,00 cm.
- Dengan keliling nominal, 4096 count bersih menghasilkan sekitar 7,53982 cm sebelum faktor kalibrasi.
- UKUR membekukan snapshot meski pita bergerak.
- SIMPAN hanya berfungsi saat HOLD dan menghasilkan satu baris CSV.
- Reboot tidak menghapus data CSV sebelumnya.
- SD/sensor error ditampilkan dan tidak menghasilkan klaim penyimpanan/pengukuran valid.
- Formula Schoorl diterapkan persis dan unit cm→kg sesuai definisi yang dipilih.
- PB kosong jika tidak ada input serial, dan tidak memengaruhi BB.

## 14. Catatan sumber

Rancangan casing, lokasi komponen, AS5600 12-bit, pin I²C Nano, tombol, titik ukur domba, dan konsep mekanik berasal dari `DOGAMET.html`. Nilai roller Ø20 mm dan model regresi yang tertulis pada HTML **tidak** dipakai pada konfigurasi firmware final ini karena telah diganti oleh keputusan pengguna: Ø24 mm dan model Schoorl.
