"""
config.py — Konfigurasi terpusat backend (SRS Bab 8, Phase 4).

Semua nilai berasal dari environment variable, dengan default yang aman
untuk pengembangan lokal. Di produksi WAJIB di-override lewat file .env
atau environment nyata (lihat backend/.env.example).
"""
from __future__ import annotations

import os
from dataclasses import dataclass

try:
    from dotenv import load_dotenv

    load_dotenv()
except ImportError:  # pragma: no cover - python-dotenv opsional saat runtime
    pass


@dataclass(frozen=True)
class Settings:
    admin_api_key: str
    database_url: str
    command_freshness_window_sec: int

    # [PHASE 4] Toggle fitur remote user sync & command queue
    # (app/routers/phase4/*: users, commands, remote unlock). Default AKTIF
    # (proyek ini sudah mengerjakan Phase 4). Set PHASE4_ENABLED=false untuk
    # menjalankan backend sebagai "Phase 3 murni" -- router phase4 TIDAK IKUT
    # TERDAFTAR sama sekali (bukan cuma disembunyikan), lihat main.py dan
    # wiki/Architecture.md bagian Phase Boundary.
    phase4_enabled: bool


def get_settings() -> Settings:
    admin_key = os.getenv("ADMIN_API_KEY", "")
    if not admin_key:
        # Fail-closed: tanpa ADMIN_API_KEY, endpoint admin TIDAK BOLEH
        # diam-diam bisa diakses siapa pun. Lebih baik gagal saat startup
        # daripada bocor di produksi.
        raise RuntimeError(
            "ADMIN_API_KEY belum diset. Salin backend/.env.example -> backend/.env "
            "dan isi nilai acak (mis. `openssl rand -hex 32`)."
        )
    return Settings(
        admin_api_key=admin_key,
        database_url=os.getenv("DATABASE_URL", "sqlite:///./biometric_smart_key.db"),
        command_freshness_window_sec=int(os.getenv("COMMAND_FRESHNESS_WINDOW_SEC", "120")),
        phase4_enabled=os.getenv("PHASE4_ENABLED", "true").strip().lower() in ("1", "true", "yes"),
    )
