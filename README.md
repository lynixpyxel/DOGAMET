# DOGAMET Firmware

Firmware untuk alat ukur lingkar dada (LD) domba berbasis Arduino Nano (ATmega328P, 5 V, 16 MHz), sensor sudut magnetik AS5600, LCD 16x2 I2C, tiga tombol tekan, dan penyimpanan microSD SPI.

Titik pengukuran alat: lingkar dada tepat di belakang kaki depan dengan pita melingkar tegak lurus terhadap sumbu tubuh. Estimasi bobot (BB) dihitung menggunakan model Schoorl saja dan bersifat estimasi, bukan pengganti timbangan.

## 1. Struktur Proyek

- `firmware/main/main.ino`: inisialisasi sistem dan loop non-blocking kooperatif.
- `firmware/main/Config.h`: pemetaan pin, alamat I2C, diameter roller 24.0 mm, faktor kalibrasi, nama file CSV, dan interval polling.
- `firmware/main/AngleSensor.h`, `firmware/main/AngleSensor.cpp`: driver I2C register AS5600 (`0x0B` status magnet, `0x0C..0x0D` raw angle 12-bit), validasi status, unwrap signed delta, dan re-baseline setelah error.
- `firmware/main/Measurement.h`, `firmware/main/Measurement.cpp`: akumulasi `signedCount` dengan proteksi overflow `int32_t`, konversi cm, reset nol, validasi LD negatif, dan perhitungan estimasi Schoorl.
- `firmware/main/Button.h`, `firmware/main/Button.cpp`: debounce non-blocking dan deteksi satu tepi tekan (`HIGH -> LOW`).
- `firmware/main/UiController.h`, `firmware/main/UiController.cpp`: rendering LCD 16x2 bebas-flicker (hanya menulis baris yang berubah), indikator mode `L`/`H`, peringatan SD `!`, dan pesan feedback sementara.
- `firmware/main/Storage.h`, `firmware/main/Storage.cpp`: inisialisasi microSD SPI, pemindaian ID terakhir dari `DOGAMET.CSV` saat boot, penulisan header hanya saat file kosong/baru, append satu baris per simpan, serta `flush()` dan `close()`.
- `firmware/main/SerialInput.h`, `firmware/main/SerialInput.cpp`: parser serial non-blocking untuk metadata opsional `PB`.
- `tools/i2c_scanner/i2c_scanner.ino`: sketch utilitas untuk memindai alamat I2C AS5600 dan backpack LCD saat bring-up hardware.
- `test/`: unit test dan mock Arduino untuk verifikasi logika firmware di PC menggunakan `g++`.

## 2. Kebutuhan Library Arduino

Kompilasi target Arduino Nano (ATmega328P) membutuhkan library berikut:

| Library | Sumber / Versi Acuan | Keterangan |
|---|---|---|
| `Wire` | Bawaan Arduino AVR Core (>= 1.8.3 disarankan) | Komunikasi I2C AS5600 dan LCD (`WIRE_HAS_TIMEOUT` aktif pada core >= 1.8.3) |
| `SPI` | Bawaan Arduino AVR Core | Antarmuka SPI untuk microSD |
| `SD` | Arduino `SD` built-in (v1.2.4 atau kompatibel) | Akses FAT16/FAT32 dan `DOGAMET.CSV` |
| `LiquidCrystal_I2C` | Frank de Brabander `LiquidCrystal I2C` (v1.1.2 melalui Library Manager) | Kontrol backpack PCF8574T LCD 16x2 I2C |

Tidak ada library eksternal khusus untuk AS5600; firmware membaca langsung register `0x0B`, `0x0C`, dan `0x0D` melalui `Wire` untuk menghemat RAM dan menjaga kontrol penuh atas status I2C serta status magnet.

## 3. Pemetaan Pin dan Wiring

| Perangkat | Pin Modul | Pin Arduino Nano | Catatan |
|---|---|---|---|
| AS5600 | SDA | A4 (SDA) | Melalui I2C level shifter 3.3 V <-> 5 V jika dibutuhkan |
| AS5600 | SCL | A5 (SCL) | Melalui I2C level shifter 3.3 V <-> 5 V jika dibutuhkan |
| AS5600 | VCC / GND | 3V3 / GND | Pastikan spesifikasi modul dan ground bersama |
| AS5600 | DIR | GND | Arah hardware tetap; arah signed dihitung firmware dari delta |
| LCD 16x2 I2C | SDA / SCL | A4 / A5 | Bus I2C yang sama, alamat berbeda (default `0x27`) |
| LCD 16x2 I2C | VCC / GND | 5V / GND | Ground bersama |
| Tombol NOL | Satu kaki | D2 | Kaki lain ke GND (`INPUT_PULLUP`, aktif-LOW) |
| Tombol UKUR | Satu kaki | D3 | Kaki lain ke GND (`INPUT_PULLUP`, aktif-LOW) |
| Tombol SIMPAN | Satu kaki | D4 | Kaki lain ke GND (`INPUT_PULLUP`, aktif-LOW) |
| microSD SPI | CS | D10 | Dikonfigurasi `OUTPUT` untuk mode SPI master |
| microSD SPI | MOSI / DI | D11 | SPI MOSI |
| microSD SPI | MISO / DO | D12 | SPI MISO |
| microSD SPI | SCK / CLK | D13 | SPI SCK |
| microSD SPI | VCC / GND | Sesuai modul / GND | Verifikasi apakah modul menerima 5 V atau hanya 3.3 V |

### Peringatan Hardware Penting

1. **Level shifting I2C (AS5600):** Adanya regulator pada breakout AS5600 tidak berarti modul tersebut memiliki level shifter I2C. Jika backpack LCD menarik SDA/SCL ke 5 V, pisahkan domain 5 V (Nano + LCD) dan 3.3 V (AS5600) menggunakan level shifter I2C bi-directional (misalnya BSS138) kecuali datasheet breakout menyatakan kompatibel 5 V.
2. **Level shifting SPI (microSD):** Jangan mengasumsikan semua modul microSD toleran logika 5 V. Modul mentah 3.3 V memerlukan level shifter pada CS, MOSI, dan SCK.
3. **Common GND:** Semua GND (Nano, AS5600, LCD, microSD, dan catu daya) harus terhubung bersama.
4. **Alamat LCD I2C:** Jalankan `tools/i2c_scanner/i2c_scanner.ino` terlebih dahulu. Jika alamat LCD adalah `0x3F` (bukan `0x27`), ubah `LCD_I2C_ADDR` di `firmware/main/Config.h`.
5. **Pin D10:** Jangan memakai D10 untuk fungsi lain karena hardware SPI master AVR membutuhkan pin SS (`D10`) berstatus `OUTPUT`.

## 4. Konfigurasi (`firmware/main/Config.h`)

Parameter utama yang dapat disesuaikan sebelum kompilasi:

- `ROLLER_DIAMETER_MM = 24.0f` (keputusan final; menggantikan Ø20 mm pada model HTML awal).
- `CALIBRATION_FACTOR = 1.0f` (sesuaikan setelah kalibrasi fisik terhadap pita referensi).
- `LCD_I2C_ADDR = 0x27` (ubah sesuai hasil I2C scanner).
- `SENSOR_POLL_INTERVAL_MS = 5` (200 Hz).
- `CHECK_AS5600_MAGNET_STATUS = 1` dan `CHECK_AS5600_MAGNET_GAIN = 0`: bit `MD` (magnet terdeteksi) menjadi syarat wajib agar pengukuran valid (`MD=0` memicu `AS5600 ERROR / MAGNET HILANG`). Bit `ML`/`MH` (batas AGC lemah/kuat) dicatat sebagai peringatan non-blocking di Serial Monitor beserta nilai `AGC` dan `MAG` tanpa memblokir pengukuran selama `MD=1`. Set `CHECK_AS5600_MAGNET_GAIN` ke `1` hanya jika ingin menjadikan `ML`/`MH` sebagai error keras.

## 5. Kompilasi dan Upload (Flashing)

### Menggunakan Arduino IDE

1. Instal library `LiquidCrystal I2C` (Frank de Brabander) melalui Library Manager.
2. Buka `tools/i2c_scanner/i2c_scanner.ino`, pilih Board **Arduino Nano** dan Processor **ATmega328P** (atau **ATmega328P (Old Bootloader)** sesuai papan Anda), upload, dan cek alamat I2C di Serial Monitor pada baud `115200`.
3. Sesuaikan `LCD_I2C_ADDR` di `firmware/main/Config.h` bila perlu.
4. Buka `firmware/main/main.ino`, verifikasi kompilasi, lalu upload ke Arduino Nano.

### Menggunakan `arduino-cli`

```powershell
arduino-cli core install arduino:avr
arduino-cli lib install "LiquidCrystal I2C"
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 firmware/main
arduino-cli upload -p COMx --fqbn arduino:avr:nano:cpu=atmega328 firmware/main
```

Ganti `COMx` dengan port serial Arduino Nano Anda (atau gunakan `atmega328old` jika Nano menggunakan bootloader lama).

## 6. Rumus dan State Machine

### Rumus Pengukuran dan Estimasi Bobot

- Diameter roller: $24.0\text{ mm}$
- Keliling nominal: $\pi \times 24.0\text{ mm} = 75.3982237\text{ mm} = 7.53982237\text{ cm}$
- Resolusi sudut AS5600: $4096\text{ count/putaran}$ ($\approx 0.00184078\text{ cm/count}$)
- Unwrap sudut per sampel:
  - $\Delta = \text{rawNow} - \text{rawPrev}$
  - Jika $\Delta > 2048$, maka $\Delta \mathrel{-}= 4096$; jika $\Delta < -2048$, maka $\Delta \mathrel{+}= 4096$.
  - **Batasan kecepatan:** rotasi sebenarnya di antara dua sampel (interval 5 ms) harus kurang dari 180 derajat ($\pm 2048$ count $\approx \pm 3.77\text{ cm}$). Pada interval 5 ms, batas teoritis ini setara sekitar $7.54\text{ m/s}$.
- Panjang pita (signed):
  $$\text{LD (cm)} = \text{signedCount} \times \frac{7.53982237}{4096} \times \text{calibrationFactor}$$
- Estimasi bobot (hanya Schoorl):
  $$\text{BB (kg)} = \frac{(\text{LD (cm)} + 22.0)^2}{100.0}$$
  - Contoh uji aritmetika: untuk $\text{LD} = 90.0\text{ cm}$, $\text{BB} = (112.0)^2 / 100.0 = 125.44\text{ kg}$.
  - Nilai LD negatif (`LD < 0`) tetap dipertahankan pada `signedCount` agar arah balik tetap akurat, namun estimasi BB dan snapshot `HOLD`/`SIMPAN` ditolak dengan pesan `TEKAN NOL`.

### State Machine

1. `BOOT`: inisialisasi Serial, I2C (`100 kHz` + timeout bus bila didukung core), tombol `INPUT_PULLUP`, LCD (jika terdeteksi), AS5600, dan microSD. Kegagalan LCD dicatat lewat Serial dan tidak menghentikan sensor/storage. Kegagalan SD menampilkan peringatan namun tetap masuk mode ukur.
2. `SENSOR_ERROR`: aktif bila transaksi I2C AS5600 gagal atau magnet tidak terdeteksi (`MD=0`; atau `ML=1`/`MH=1` jika `CHECK_AS5600_MAGNET_GAIN` diaktifkan). Snapshot `HOLD` dibersihkan, `NOL`/`UKUR`/`SIMPAN` ditolak agar tidak menghasilkan nol atau rekaman palsu. Saat sensor pulih, `AngleSensor` mengambil baseline `rawPrev` baru tanpa mengakumulasi delta lompatan, lalu berpindah ke `READY_LIVE`.
3. `READY_LIVE` (indikator `L`): sensor dibaca tiap 5 ms, LCD diperbarui maks tiap 100 ms hanya ketika teks berubah.
   - `NOL`: `signedCount = 0`, tetap/kembali ke `READY_LIVE`.
   - `UKUR`: membekukan snapshot LD, BB, `signedCount`, dan PB opsional lalu masuk ke `HOLD`. Ditolak jika `LD < 0`.
   - `SIMPAN`: ditolak; menampilkan pesan `TEKAN UKUR DULU / HARUS MODE HOLD`.
4. `HOLD` (indikator `H`): nilai LCD dan snapshot tetap beku sementara polling sensor tetap berjalan di latar belakang sehingga `signedCount` tidak rusak saat pita digerakkan dan kembali ke `READY_LIVE` tetap akurat.
   - `UKUR`: membuang snapshot dan kembali ke `READY_LIVE`.
   - `NOL`: `signedCount = 0`, membuang snapshot, kembali ke `READY_LIVE`.
   - `SIMPAN`: menulis tepat 1 baris ke `DOGAMET.CSV` per tepi tekan tombol. Jika SD gagal/penuh/tidak ada, snapshot tetap dipertahankan di `HOLD` dan muncul `SD ERROR / GAGAL SIMPAN`.

## 7. Operasi Perangkat dan Serial PB Metadata

### Tampilan LCD 16x2

- Mode LIVE normal:
  - Baris 1: `LD:  90.0 cm   L`
  - Baris 2: `BB: 125.4 kg EST`
- Mode HOLD dengan peringatan SD tidak siap (tanda `!` di kolom 14):
  - Baris 1: `LD:  90.0 cm ! H`
  - Baris 2: `BB: 125.4 kg EST`
- Kondisi error: `AS5600 ERROR` + detail (`CEK KABEL I2C`, `MAGNET HILANG`, `MAGNET LEMAH`, `MAGNET KUAT`), atau `SD ERROR`, atau `TEKAN NOL (LD<0)`.

### Perintah Serial USB (`115200` baud, newline `\n` atau `\r\n`)

Hardware hanya memiliki tiga tombol tanpa keypad, sehingga Panjang Badan (`PB`) tidak dapat diinput langsung tanpa komputer. `PB` hanya disimpan sebagai metadata opsional CSV dan **tidak pernah** mengubah rumus Schoorl.

| Perintah | Aksi | Respons Serial |
|---|---|---|
| `PB=78.5` | Menyimpan metadata `pb_cm = 78.50` untuk snapshot berikutnya | `OK: PB=78.50 cm (metadata only, not in Schoorl)` |
| `PB=CLEAR` | Menghapus metadata PB sehingga kolom `pb_cm` CSV kosong | `OK: PB CLEARED` |
| `STATUS` | Menampilkan state, LD, BB estimasi, count, PB, dan status SD | `STATE=... LD=... BB_EST=... COUNT=... PB=... SD=...` |
| `HELP` | Menampilkan daftar perintah | `CMDS: PB=<cm> \| PB=CLEAR \| STATUS \| HELP` |

Input tidak valid (misalnya `PB=-5`, `PB=0`, `PB=78,5`, `PB=abc`) ditolak dan mengembalikan pesan error ringkas.

### Format CSV (`DOGAMET.CSV`)

File dibuka dengan mode append (tidak menimpa data lama). Header ditulis hanya bila file belum ada atau berukuran 0 byte:

```csv
id,ld_cm,bb_kg,model_bb,pb_cm,signed_count,roller_diameter_mm,calibration_factor,elapsed_ms,source
```

- `id`: nomor urut unik (saat boot firmware memindai ID terbesar di `DOGAMET.CSV` dan melanjutkan dari `max_id + 1`).
- `model_bb`: selalu `SCHOORL`.
- `pb_cm`: kosong (`,,`) jika PB belum diset atau telah di-clear.
- `elapsed_ms`: nilai `millis()` saat tombol `SIMPAN` berhasil menulis (bukan tanggal/waktu kalender karena tidak ada RTC).
- `source`: selalu `DEVICE`.

## 8. Rencana Pengujian dan Hasil yang Diharapkan

### Pengujian Unit/Host (`test/test_dogamet.cpp`)

Jalankan suite unit test terhadap modul produksi (`AngleSensor`, `Measurement`, `Button`, `SerialInput`, `Storage`, `UiController`) dengan compiler `g++`:

```powershell
g++ -std=c++17 -Wall -Wextra -Werror -I test/mocks -I firmware/main `
  firmware/main/AngleSensor.cpp firmware/main/Measurement.cpp `
  firmware/main/Button.cpp firmware/main/SerialInput.cpp `
  firmware/main/Storage.cpp firmware/main/UiController.cpp `
  test/test_dogamet.cpp -o test/test_dogamet.exe
.\test\test_dogamet.exe
```

### Tabel Kasus Uji Integrasi dan Hardware

| ID | Kasus Uji | Input / Aksi | Hasil yang Diharapkan | Status Verifikasi |
|---|---|---|---|---|
| T-01 | Scan AS5600 | Jalankan `i2c_scanner.ino` | Ditemukan `0x36` | Belum diuji di hardware fisik |
| T-02 | Scan LCD | Jalankan `i2c_scanner.ino` | Ditemukan alamat LCD (`0x27` atau `0x3F`) | Belum diuji di hardware fisik |
| T-03 | Wrap maju | Raw `4090 -> 5` | Delta `+11` | Lulus di host unit test |
| T-04 | Wrap mundur | Raw `5 -> 4090` | Delta `-11` | Lulus di host unit test |
| T-05 | 1 putaran penuh | `+4096` count | `LD = 7.53982 cm` (sebelum kalibrasi) | Lulus di host unit test |
| T-06 | Arah bolak-balik | `+1000` lalu `-1000` count | `signedCount == 0`, LD kembali ke `0.00 cm` | Lulus di host unit test |
| T-07 | Tombol NOL | Tekan NOL saat LIVE/HOLD | `signedCount = 0`, HOLD batal, mode LIVE | Lulus di unit logic (`Measurement`) & perlu uji pin fisik |
| T-08 | Tombol UKUR (LIVE->HOLD) | Tekan UKUR saat LIVE | Snapshot dibuat, LCD tampilkan `H` | Lulus di host unit test |
| T-09 | Hold freeze | Gerakkan roller saat HOLD | Nilai LCD snapshot tetap, latar belakang tetap tracking | Lulus di host unit test |
| T-10 | Resume (HOLD->LIVE) | Tekan UKUR saat HOLD | Kembali ke LIVE dengan posisi pita terbaru | Perlu verifikasi alur tombol di hardware fisik |
| T-11 | Guard SIMPAN saat LIVE | Tekan SIMPAN saat LIVE | Tidak ada baris ditulis; tampil `HARUS MODE HOLD` | Perlu verifikasi alur tombol di hardware fisik |
| T-12 | Append CSV | HOLD lalu SIMPAN | Tepat 1 baris ditulis ke `DOGAMET.CSV` | Lulus di host mock SD; perlu uji microSD fisik |
| T-13 | Debounce tombol | Bounce <30 ms dan tahan tombol lama | Bounce diabaikan; 1 tekan = 1 event | Lulus di host unit test |
| T-14 | Boot tanpa SD | Cabut microSD lalu nyalakan | Ukur & tampil tetap jalan; `!` tampil; SIMPAN error | Lulus di host mock SD; perlu uji hardware fisik |
| T-15 | SD gagal tulis | Penuh / dicabut saat tulis | `SD ERROR`, tidak ada klaim sukses, snapshot bertahan | Lulus di host mock SD; perlu uji hardware fisik |
| T-16 | Persistensi reboot | Simpan 2 baris, reboot, simpan lagi | Baris lama tetap ada, ID lanjut ke `#3` | Lulus di host mock SD; perlu uji hardware fisik |
| T-17 | Aritmetika Schoorl | `LD = 90.0 cm` | `BB = 125.44 kg` (uji aritmetika, bukan validasi biologis) | Lulus di host unit test |
| T-18 | PB set | Kirim `PB=78.5` | Kolom `pb_cm = 78.50`, `bb_kg` tidak berubah | Lulus di host unit test |
| T-19 | PB clear | Kirim `PB=CLEAR` | Kolom `pb_cm` berikutnya kosong | Lulus di host unit test |
| T-20 | Putus / pulih sensor | Putus I2C atau lepas magnet lalu pasang lagi | `AS5600 ERROR` muncul; saat pulih count tidak melonjak | Lulus di host mock I2C; perlu uji hardware fisik |

## 9. Asumsi, Ambiguitas, dan Pengecekan Hardware yang Belum Dapat Diverifikasi dari Kode

1. **Belum dikompilasi dengan toolchain AVR (`avr-gcc` / `arduino-cli`) maupun diuji di hardware fisik:** Di lingkungan pengembangan saat ini hanya tersedia compiler host (`g++` MinGW), bukan core `arduino:avr` atau papan Nano fisik. Estimasi pemakaian SRAM/Flash aktual di ATmega328P harus diperiksa saat kompilasi di Arduino IDE / `arduino-cli`.
2. **Alamat I2C LCD:** Diasumsikan `0x27`. Beberapa modul menggunakan `0x3F` atau alamat PCF8574A lain; wajib dicek dengan `i2c_scanner.ino`.
3. **Level tegangan dan pull-up I2C/SPI:** Kode sumber tidak dapat memverifikasi apakah breakout AS5600 dan modul microSD yang dipakai benar-benar memiliki BSS138 / level shifter 3.3 V <-> 5 V.
4. **Orientasi mekanik roller dan polaritas AS5600:** Karena pin `DIR` AS5600 dihubungkan ke GND, diasumsikan tarikan pita arah maju menghasilkan kenaikan sudut raw (`0 -> 4095`). Jika mekanik roller terbalik arah putarnya, tanda `delta` di `AngleSensor::computeUnwrapDelta` dapat dibalik (`delta = -delta`) atau polaritas pemasangan roller disesuaikan.
5. **Status magnet (`ML` / `MH`):** Firmware menganggap magnet terlalu lemah (`ML`) atau terlalu kuat (`MH`) sebagai `SENSOR_ERROR`. Jika jarak magnet mekanik prototipe sering memicu batas gain meski pembacaan sudut stabil, pertimbangkan memperbaiki celah magnet terlebih dahulu sebelum menonaktifkan `CHECK_AS5600_MAGNET_GAIN`.
6. **Slip mekanik dan diameter efektif:** `CALIBRATION_FACTOR = 1.0f` dan diameter `24.0f mm` adalah parameter awal; keakuratan fisik terhadap pita ukur referensi harus diuji secara empiris.
7. **Kesesuaian biologis formula Schoorl:** Model Schoorl (`((LD + 22)^2)/100`) diterapkan sesuai spesifikasi pengguna dan belum divalidasi terhadap populasi domba target.

## 10. Troubleshooting Singkat

- **LCD tampil kotak penuh atau kosong:** Cek kontras potensiometer biru di belakang backpack I2C, cek alamat dengan `i2c_scanner.ino`, dan pastikan pin `SDA` ke `A4`, `SCL` ke `A5`.
- **`AS5600 ERROR / CEK KABEL I2C`:** Periksa kabel A4/A5, level shifter 3.3 V, dan pastikan sisi 3.3 V mendapat catu daya.
- **Status `ML=1` di Serial atau `MAGNET LEMAH` padahal magnet sudah dekat:**
  1. Pastikan magnet adalah tipe **diametral** (kutub N/S di sisi kiri-kanan), bukan **aksial** (kutub di permukaan atas-bawah). Magnet aksial yang dipasang tepat di atas AS5600 menghasilkan medan horizontal `Bx/By` sangat kecil sehingga `ML=1` atau `MD=0` meskipun jaraknya sangat rapat.
  2. Cek Serial Monitor (`115200` baud) dan ketik `STATUS` untuk melihat `AS5600(STAT=0x.. AGC=.. MAG=.. RAW=..)`. Jika `AGC=255` dan `MAG` sangat kecil (<100) atau `RAW` tidak berubah saat diputar, periksa tipe magnet (harus diametral) dan pemusatan posisi magnet tepat di tengah IC AS5600.
  3. Dengan konfigurasi default `CHECK_AS5600_MAGNET_GAIN = 0`, kondisi `MD=1` + `ML=1` tidak lagi memblokir pembacaan di LCD dan hanya mencetak peringatan ke Serial Monitor.
- **`SD ERROR / GAGAL SIMPAN`:** Pastikan kartu diformat FAT16/FAT32, CS terhubung ke D10, wiring SPI (D11/D12/D13) benar, dan modul microSD mendapat catu serta level tegangan yang sesuai.
- **Angka LD berkurang saat pita ditarik keluar:** Arah putaran magnet berlawanan dengan asumsi `DIR=GND`; periksa orientasi sensor/roller.

