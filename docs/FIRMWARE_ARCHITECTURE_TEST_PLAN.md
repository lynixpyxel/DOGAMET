# DOGAMET — Arsitektur Firmware dan Test Plan

## 1. Arsitektur logis

```text
AS5600 (I²C)
    │ raw angle 12-bit
    ▼
AngleSensor ── unwrap + signed delta
    │ signedCount
    ▼
Measurement ── cm conversion ── Schoorl estimate
    │ live measurement
    ├──────────────► UiController ──► LCD 16×2
    │
    └─ UKUR snapshot ─► Hold record
                           │
NOL / UKUR / SIMPAN ─► Button handler
                           │ SIMPAN
                           ▼
                       Storage ──► microSD CSV

USB Serial ── optional PB metadata (not used in Schoorl calculation)
```

## 2. Pembagian modul

| Modul | Tanggung jawab |
|---|---|
| Config | Pin, alamat I²C, diameter roller 24 mm, faktor kalibrasi, nama file |
| AngleSensor | Init AS5600, baca raw, validasi, unwrap wrap-around |
| Measurement | signedCount, konversi cm, zero, formula Schoorl |
| Button | Debounce, press-edge, satu event per tekan |
| UiController | LIVE/HOLD/error, format LCD |
| Storage | Mount SD, header CSV, append, ID rekaman |
| SerialInput | PB metadata opsional lewat USB |
| main | Setup dan loop kooperatif tanpa delay panjang |

## 3. State machine

```text
             boot
              │
              ▼
          INIT_HARDWARE
          /           \
 sensor gagal       sensor valid
      │                  │
      ▼                  ▼
 SENSOR_ERROR       READY_LIVE ◄──────────────┐
      │                  │                   │
 recovery + baseline     │ UKUR              │ UKUR
      └─────────────────►▼                   │
                         HOLD ────────────────┘
                          │
                          │ SIMPAN + SD OK
                          ▼
                     HOLD (record saved)

SD error adalah warning terpisah: pengukuran tetap dapat berjalan,
tetapi aksi SIMPAN gagal dan harus memberi pesan error.
NOL dari READY_LIVE/HOLD menetapkan ulang count relatif dan kembali LIVE.
```

## 4. Rumus dan angka acuan

- Roller diameter = 24 mm.
- Keliling = 75,3982237 mm = 7,53982237 cm.
- AS5600 = 4096 count/rev.
- cm/count = 7,53982237 / 4096 = sekitar 0,00184078 cm.
- `LD_cm = signedCount * 7.53982237 / 4096 * calibrationFactor`.
- `BB_kg = ((LD_cm + 22) * (LD_cm + 22)) / 100`.

Arah signed: count positif menambah LD, count negatif mengurangi LD. Nilai LD negatif setelah mundur melewati titik nol harus ditangani sebagai kondisi tidak valid/di bawah nol untuk UI dan penyimpanan; jangan mengubahnya menjadi nilai absolut. Pilihan paling aman untuk versi awal adalah clamp tampilan/prediksi ke “belum ada pengukuran” bila LD < 0, sambil mempertahankan signedCount internal agar arah gerak tetap benar. NOL dapat menetapkan ulang titik acuan.

## 5. Rencana pengujian unit dan integrasi

| ID | Pengujian | Input/aksi | Hasil yang diharapkan |
|---|---|---|---|
| T-01 | AS5600 init | Power on, sensor terpasang | Sensor terdeteksi pada 0x36 |
| T-02 | I²C LCD | Scan bus | LCD pada alamat konfigurasi |
| T-03 | Forward wrap | raw 4090 lalu 5 | delta +11 |
| T-04 | Reverse wrap | raw 5 lalu 4090 | delta −11 |
| T-05 | Full revolution | +4096 count | LD ≈ 7,53982 cm sebelum kalibrasi |
| T-06 | Reverse correction | +1000 lalu −1000 count | LD kembali mendekati nilai awal |
| T-07 | NOL | Tekan NOL | LD relatif = 0 dan HOLD dibatalkan |
| T-08 | UKUR | Tekan saat LIVE | Snapshot dibuat dan UI HOLD |
| T-09 | Hold freeze | Gerakkan roller saat HOLD | Nilai snapshot di LCD tidak berubah |
| T-10 | Resume | Tekan UKUR saat HOLD | UI kembali LIVE |
| T-11 | Save guard | Tekan SIMPAN saat LIVE | Tidak ada record ditulis |
| T-12 | CSV append | HOLD lalu SIMPAN | Tepat satu baris baru |
| T-13 | Debounce | Tahan/tekan tombol dengan bounce | Satu aksi per tekan |
| T-14 | SD unavailable | Boot tanpa SD | Pengukuran tetap bisa; SIMPAN memberi error |
| T-15 | SD write fail | Media penuh/removed | Tidak ada pesan sukses palsu |
| T-16 | Persistence | Simpan, reboot, simpan lagi | Baris lama tetap ada, ID unik |
| T-17 | Schoorl arithmetic | LD 90 cm | BB 125,44 kg menurut persamaan yang dipilih |
| T-18 | PB metadata | Kirim `PB=78.5` | Kolom PB terisi; BB tidak berubah |
| T-19 | PB clear | Kirim `PB=CLEAR` | Kolom PB berikutnya kosong |
| T-20 | Sensor disconnect | Cabut/putus I²C sensor | Sensor error, count tidak melonjak |

## 6. Kalibrasi fisik

1. Pastikan diameter roller aktual terukur dan permukaan kontak bersih.
2. Beri tanda posisi awal pita dan roller.
3. Ukur panjang referensi dengan alat pembanding yang layak.
4. Jalankan DOGAMET pada beberapa titik panjang (mis. 30, 50, 100 cm) dengan tegangan pita yang konsisten.
5. Bandingkan hasil; catat error absolut dan persentase.
6. Ulangi untuk arah maju dan mundur guna mengidentifikasi slip/backlash.
7. Jika perlu, tentukan calibration factor berdasarkan beberapa pengujian, bukan satu titik saja.
8. Dokumentasikan nilai diameter efektif dan faktor kalibrasi yang digunakan.

## 7. Uji penggunaan lapangan

- Pastikan pita berada tepat di belakang kaki depan.
- Jaga pita tegak lurus terhadap sumbu tubuh.
- Hindari pita terlalu longgar atau menekan tubuh berlebihan.
- Lakukan pengukuran berulang pada subjek yang sama untuk melihat repeatability.
- Bandingkan estimasi bobot dengan timbangan aktual sebelum menilai kegunaan model.
- Jangan menggunakan estimasi sebagai pengganti timbangan atau dasar keputusan kesehatan ternak tanpa validasi.

## 8. Batasan diketahui

- Ø24 mm menggantikan Ø20 mm pada HTML sumber berdasarkan keputusan terbaru pengguna.
- Model Schoorl menggantikan regresi linear yang tertulis pada HTML. Kesesuaian formula pada populasi domba sasaran perlu diverifikasi dari sumber ilmiah primer.
- PB manual tidak punya kontrol input fisik pada tiga tombol saat ini. USB serial hanya opsi metadata ketika terhubung ke komputer.
- Tidak ada RTC; waktu CSV hanya `millis()` sejak boot.
- Keberadaan level shifting/regulator pada modul AS5600 dan SD harus diperiksa pada modul aktual.
- Hasil simulasi HTML tidak membuktikan akurasi mekanik prototipe.
