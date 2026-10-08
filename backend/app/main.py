"""main.py — FastAPI application factory (Phase 4 backend).

Menjalankan (pengembangan lokal):
    uvicorn app.main:app --reload --port 8000
(dari dalam folder backend/, lihat backend/README.md / wiki/Backend-Setup.md)

Dipakai application FACTORY (create_app()), BUKAN `app = FastAPI()` langsung
di top-level modul. Alasannya bukan gaya semata: PHASE4_ENABLED (lihat
app/config.py) menentukan router mana yang terdaftar, dan keputusan itu
HARUS dibaca ulang setiap kali aplikasi dibangun — bukan dibekukan sekali
saat modul pertama kali di-import. Tanpa factory, test yang mengganti env
var PHASE4_ENABLED antar-kasus-uji tidak akan pernah terlihat efeknya
(modul Python hanya di-eksekusi sekali per proses, `import` berikutnya
mengambil dari cache) — inilah yang membuat pola factory ini WAJIB, bukan
sekadar best practice, begitu ada konfigurasi yang menentukan BENTUK
aplikasi (bukan cuma nilai di dalamnya).
"""
from __future__ import annotations

from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from .config import Settings, get_settings
from .database import Base, init_engine
from .routers.phase3 import devices, events, heartbeat


def create_app(settings: Settings | None = None) -> FastAPI:
    """Bangun instance FastAPI baru dari AWAL berdasarkan `settings` (atau
    hasil get_settings() bila tidak diberikan). Setiap pemanggilan membaca
    konfigurasi secara eksplisit -> tidak ada state beku lintas pemanggilan."""
    settings = settings or get_settings()

    @asynccontextmanager
    async def lifespan(_app: FastAPI):
        engine = init_engine(settings.database_url)
        Base.metadata.create_all(bind=engine)
        yield

    app = FastAPI(
        title="Biometric Smart Key & Management System — Backend",
        description="Ingestion, device management, remote user sync, dan command queue (SRS Bab 8-12, Phase 4).",
        version="0.4.0",
        lifespan=lifespan,
    )

    # ---------------- PHASE 3 CORE (selalu aktif) ----------------
    app.include_router(events.router)
    app.include_router(devices.router)
    app.include_router(heartbeat.router)

    # ---------------- PHASE 4 (remote user sync & command queue) ----------------
    # Dipasang KONDISIONAL berdasarkan settings.phase4_enabled (default true —
    # proyek ini sedang mengerjakan Phase 4). Dengan PHASE4_ENABLED=false,
    # backend berjalan sebagai "Phase 3 murni": /api/v1/users dan
    # /api/v1/commands* TIDAK TERDAFTAR SAMA SEKALI (404, bukan 401/403) —
    # berguna saat mem-branch `phase-3` terpisah dari `phase-4` tanpa
    # menghapus/menulis ulang kode (lihat wiki/Architecture.md "Batas Fase").
    if settings.phase4_enabled:
        from .routers.phase4 import commands as phase4_commands
        from .routers.phase4 import users as phase4_users

        app.include_router(phase4_users.router)
        app.include_router(phase4_commands.router)

    @app.get("/health", tags=["meta"])
    def health():
        return {"status": "ok", "phase4_enabled": settings.phase4_enabled}

    static_dir = Path(__file__).parent / "static"
    if static_dir.exists():
        app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")

        @app.get("/", include_in_schema=False)
        def dashboard():
            return FileResponse(str(static_dir / "dashboard.html"))

    return app


# Entry point default untuk `uvicorn app.main:app` — membaca environment
# APA ADANYA saat proses dimulai (perilaku wajar untuk server sungguhan,
# yang memang hanya start sekali per deployment).
app = create_app()
