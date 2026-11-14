# DOGAMET — Pinout dan Panduan Wiring

**Target:** Arduino Nano ATmega328P 5 V / 16 MHz  
**Konfigurasi:** AS5600 + LCD I²C + 3 tombol + microSD SPI.

## 1. Tabel koneksi Nano

| Perangkat | Pin perangkat | Arduino Nano | Catatan |
|---|---|---|---|
| AS5600 | SDA | A4 (SDA) | I²C melalui level shifter jika diperlukan |
| AS5600 | SCL | A5 (SCL) | I²C melalui level shifter jika diperlukan |
| AS5600 | VCC | 3V3 sesuai breakout | Pastikan spesifikasi breakout |
| AS5600 | GND | GND | Ground bersama |
| AS5600 | DIR | GND | Konfigurasi arah tetap; firmware tetap melakukan signed unwrap |
| LCD I²C | SDA | A4 (SDA) | Bus I²C yang sama, alamat berbeda |
| LCD I²C | SCL | A5 (SCL) | Bus I²C yang sama |
| LCD I²C | VCC | 5V | Sesuai backpack LCD |
| LCD I²C | GND | GND | Ground bersama |
| Tombol NOL | satu kaki | D2 | `INPUT_PULLUP` |
| Tombol UKUR | satu kaki | D3 | `INPUT_PULLUP` |
| Tombol SIMPAN | satu kaki | D4 | `INPUT_PULLUP` |
| Semua tombol | kaki lainnya | GND | Aktif-LOW |
| microSD | CS | D10 | D10 diset OUTPUT untuk SPI master |
| microSD | MOSI / DI | D11 | SPI MOSI |
| microSD | MISO / DO | D12 | SPI MISO |
| microSD | SCK / CLK | D13 | SPI clock |
| microSD | VCC | sesuai modul | Cek apakah modul 3,3 V-only atau punya regulator |
| microSD | GND | GND | Ground bersama |

## 2. Skema koneksi konseptual

```text
                  +----------------------+
                  |     Arduino Nano     |
                  |                      |
AS5600 SDA --[I2C level shift]-- A4/SDA   |
AS5600 SCL --[I2C level shift]-- A5/SCL   |
LCD SDA ----------------------- A4/SDA   |
LCD SCL ----------------------- A5/SCL   |
NOL button -------------------- D2       |
UKUR button ------------------- D3       |
SIMPAN button ----------------- D4       |
SD CS ------------------------- D10      |
SD MOSI ----------------------- D11      |
SD MISO ----------------------- D12      |
SD SCK ------------------------ D13      |
                  | GND                  |
                  +--+---+---+---+-------+
                     |   |   |   |
                   GND AS  LCD SD
                       5600
```

Catatan: diagram ini menunjukkan hubungan sinyal, bukan skema regulator/charger lengkap. Semua GND harus common.

## 3. Catatan level tegangan — wajib diperiksa

- IC AS5600 bekerja pada domain tegangan rendah. Berkas konsep menyebut AS5600 3,3 V dan BSS138 sebagai level shifter I²C.
- Jangan menyimpulkan breakout AS5600 memiliki level shifter hanya karena ada regulator. Regulator catu dan level shifter sinyal adalah dua fungsi berbeda.
- Jika Nano 5 V berkomunikasi dengan AS5600 pada domain 3,3 V, gunakan level shifter I²C yang sesuai (misalnya rangkaian BSS138) kecuali breakout secara eksplisit menyatakan kompatibel dengan pull-up 5 V.
- LCD backpack biasanya menarik SDA/SCL ke 5 V. Pastikan sisi 5 V dan 3,3 V dipisahkan oleh level shifter; jangan menyatukan pull-up 5 V langsung ke bus AS5600 3,3 V.
- Modul microSD bervariasi. Sebagian modul punya regulator dan level shifter; sebagian breakout sederhana tidak. Pastikan pin input modul aman untuk sinyal 5 V dari Nano. Jika modul 3,3 V-only, gunakan level shifting pada SPI dan catu 3,3 V yang stabil.
- Jangan memberi 5 V ke pin VCC modul yang hanya menerima 3,3 V.

## 4. Tombol

Konfigurasi tiap tombol:

```text
D2 / D3 / D4 ---- tombol ---- GND
```

Firmware mengatur pin sebagai `INPUT_PULLUP`. Kondisi normal = HIGH; ditekan = LOW. Gunakan debounce perangkat lunak (sekitar 25–50 ms sebagai nilai awal) dan deteksi transisi HIGH→LOW. Jangan memakai satu tombol untuk beberapa aksi sekaligus.

## 5. SPI dan SD

Pada Nano ATmega328P, jalur hardware SPI adalah D11 MOSI, D12 MISO, D13 SCK. D10 menjadi chip-select. Pastikan tidak ada perangkat lain yang menahan jalur SPI. Gunakan kartu microSD yang diformat FAT16/FAT32 dan uji dengan library SD pada modul aktual.

## 6. I²C

AS5600 menggunakan alamat 7-bit 0x36. Alamat backpack LCD perlu diperiksa (sering 0x27 atau 0x3F, tetapi jangan diasumsikan). Jalankan I²C scanner pada tahap bring-up dan catat alamat aktual.

## 7. Daya — rancangan konsep dari HTML

Konsep HTML menyebut satu sel Li-ion 18650 3,7 V, modul TP4056, boost 5 V, USB-C, PTC 0,5 A, dan sakelar ON/OFF. Nilai/jenis modul, proteksi sel, kemampuan arus, dan jalur charger harus diverifikasi berdasarkan modul yang benar-benar dibeli. Jangan menghubungkan charger dan beban dengan asumsi modul memiliki power-path management jika datasheet tidak menyatakannya.

## 8. Checklist sebelum menyalakan

- [ ] Pastikan polaritas VCC/GND setiap modul.
- [ ] Pastikan batas tegangan AS5600 dan microSD dari datasheet/breakout.
- [ ] Pastikan level shifter I²C dan SPI benar-benar diperlukan/dipasang sesuai modul.
- [ ] Pastikan semua ground terhubung.
- [ ] Pastikan D10 tidak dipakai untuk fungsi lain dan diset OUTPUT.
- [ ] Ukur rail 5 V dan 3,3 V dengan multimeter sebelum memasang modul sensitif.
- [ ] Periksa tidak ada short antar-rail.
- [ ] Uji I²C scanner sebelum menjalankan firmware penuh.
- [ ] Uji microSD dengan sketch terpisah.
