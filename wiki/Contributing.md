# Contributing — Strategi Git & Quality Gate

## Satu Branch per Fase, PR Dibiarkan Terbuka

Setiap fase (Phase 1, 2, 3, 4, ...) dikembangkan di branch tersendiri
(`phase-1`, `phase-2`, ...), di-push dengan Pull Request ke `main` — **PR
tidak langsung di-merge**. PR dibiarkan terbuka sebagai *quality gate*
hingga seluruh fase proyek selesai dan siap dirilis bersama.

```bash
# Selalu mulai dari origin/main, BUKAN git init lokal — menghindari
# error "unrelated histories" (lihat Troubleshooting).
git fetch origin
git checkout -b phase-5 origin/main

# ...kerjakan Phase 5...

git add .
git commit -m "Phase 5: Enrollment & Credential Management"
git push -u origin phase-5

gh pr create --base main --head phase-5 \
  --title "Phase 5: Enrollment & Credential Management" \
  --draft   # draft = tidak bisa ter-merge tidak sengaja
```

## Setiap PR Wajib Lulus CI

`.github/workflows/firmware-ci.yml` berjalan otomatis pada setiap PR ke
`main` dan setiap push ke `phase-*`. Lihat [[Testing]] untuk rincian tiap
job. Ringkas: unit test firmware + backend (cepat, paralel) → baru build
ESP32 sungguhan (3 environment, termasuk pembuktian batas fase) → static
analysis. Semua harus hijau sebelum PR dianggap siap direview.

## Batas Antar-Fase (Phase Boundary)

Kode fase yang lebih baru **boleh** bergantung pada fase sebelumnya,
**tidak sebaliknya**. Ini ditegakkan secara teknis (bukan hanya aturan
sosial), lihat [[Architecture]] bagian "Batas Fase": folder terpisah
(`src/phase4/`, `app/routers/phase4/`), flag kompilasi/runtime
(`ENABLE_PHASE4_FEATURES`, `PHASE4_ENABLED`), dan build/test yang
membuktikannya secara otomatis di CI.

Konsekuensi praktis: branch `phase-3` yang dibuat **sebelum** folder
`src/phase4/`/`app/routers/phase4/` ada akan tetap compile & lulus test
tanpa perlu kode apa pun dihapus — karena dependensi memang hanya
mengalir satu arah.

## Commit Message

Tidak ada format ketat, tapi disarankan menyebut fase & komponen:

```
Phase 4: Add HMAC signing to command queue
Phase 3: Fix heartbeat timer not resetting after failure
```

## Sebelum Membuka PR

1. `pio test -e native` lulus (firmware).
2. `pio run -e esp32dev && pio run -e esp32dev-phase3only` berhasil compile.
3. `cd backend && pytest && ruff check .` lulus.
4. `include/secrets.h` dan `backend/.env` **tidak ikut ter-commit**
   (cek `.gitignore`).

## Pertanyaan Umum

**Q: Saya menambah fitur yang menyentuh Phase 3 maupun Phase 4, branch
mana?**
A: Buat branch dari fase yang sedang aktif dikerjakan. Pastikan perubahan
di sisi Phase 3 tetap lulus `esp32dev-phase3only` — jika tidak, berarti
perubahan itu sebenarnya membuat Phase 3 bergantung pada Phase 4, yang
melanggar arah dependensi.

**Q: Bagaimana cara merilis (akhirnya benar-benar merge)?**
A: Di luar cakupan dokumen ini saat ini — bergantung keputusan tim kapan
seluruh fase dianggap siap. Saat itu terjadi, PR-PR yang sudah
terkumpul (dan sudah lulus CI selama ini) di-review & di-merge berurutan.
