"""
database.py — Setup SQLAlchemy engine/session (SRS Bab 9, Phase 4).

Sinkron (bukan async) dengan sengaja: FastAPI menjalankan endpoint `def`
biasa di threadpool, jadi tidak perlu kerumitan async SQLAlchemy untuk
skala Phase 4 (single edge device s/d puluhan unit, sesuai NFR-003).
"""
from __future__ import annotations

from sqlalchemy import create_engine
from sqlalchemy.orm import DeclarativeBase, sessionmaker

from .config import get_settings


class Base(DeclarativeBase):
    pass


def make_engine(database_url: str):
    connect_args = {"check_same_thread": False} if database_url.startswith("sqlite") else {}
    return create_engine(database_url, connect_args=connect_args)


_settings = None
engine = None
SessionLocal = None


def init_engine(database_url: str | None = None):
    """Dipanggil sekali saat startup app (atau oleh test fixture dengan URL
    berbeda, mis. sqlite:///:memory: atau file temp) untuk (re)konfigurasi
    engine global. Terpisah dari import-time agar test bisa memakai DB
    temporer tanpa menyentuh file DB pengembangan.
    """
    global engine, SessionLocal
    url = database_url or get_settings().database_url
    engine = make_engine(url)
    SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
    return engine


def get_db():
    """Dependency FastAPI: satu session per request, selalu ditutup."""
    if SessionLocal is None:
        init_engine()
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
