"""Router Phase 4 — Remote User Sync & Command Queue (users, commands,
remote unlock). Inklusinya dikendalikan oleh env var PHASE4_ENABLED
(lihat app/config.py:Settings.phase4_enabled) agar deployment/branch
"Phase 3 murni" bisa menonaktifkannya tanpa menghapus kode — lihat
wiki/Architecture.md bagian "Batas Fase" untuk rasionalnya."""
