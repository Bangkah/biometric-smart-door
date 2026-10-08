import os
import tempfile

import pytest

os.environ.setdefault("ADMIN_API_KEY", "test-admin-key")


def _build_client(monkeypatch, *, phase4_enabled: bool = True):
    """Helper bersama: siapkan DB SQLite temporer BARU + bangun instance
    FastAPI BARU lewat create_app() (BUKAN `from app.main import app`).

    create_app() membaca Settings saat itu juga setiap dipanggil (lihat
    app/main.py), jadi test yang perlu konfigurasi berbeda (mis.
    PHASE4_ENABLED=false) CUKUP memanggil create_app() lagi dengan Settings
    baru -- tidak perlu importlib.reload()/manipulasi sys.modules, yang
    berisiko merusak registry class SQLAlchemy (declarative Base) saat
    modul yang sama dieksekusi ulang lebih dari sekali dalam satu proses.
    """
    fd, path = tempfile.mkstemp(suffix=".db")
    os.close(fd)
    db_url = f"sqlite:///{path}"
    monkeypatch.setenv("DATABASE_URL", db_url)
    monkeypatch.setenv("ADMIN_API_KEY", "test-admin-key")
    monkeypatch.setenv("PHASE4_ENABLED", "true" if phase4_enabled else "false")

    from fastapi.testclient import TestClient

    import app.database as database
    from app.config import get_settings
    from app.main import create_app

    database.init_engine(db_url)
    database.Base.metadata.create_all(bind=database.engine)

    fastapi_app = create_app(get_settings())
    return TestClient(fastapi_app), path


@pytest.fixture()
def client(monkeypatch):
    """TestClient dengan database SQLite file temporer BARU per test (bukan
    :memory:, supaya perilaku identik dengan produksi termasuk lintas-koneksi),
    dan Phase 4 AKTIF (default proyek saat ini)."""
    c, path = _build_client(monkeypatch, phase4_enabled=True)
    with c:
        yield c
    os.remove(path)


@pytest.fixture()
def client_phase4_disabled(monkeypatch):
    """Sama seperti `client`, tapi PHASE4_ENABLED=false — mensimulasikan
    deployment/branch "Phase 3 murni" (lihat wiki/Architecture.md, bagian
    Batas Fase). Router /api/v1/users dan /api/v1/commands* TIDAK TERDAFTAR
    sama sekali pada instance ini (404, bukan 401/403)."""
    c, path = _build_client(monkeypatch, phase4_enabled=False)
    with c:
        yield c
    os.remove(path)


@pytest.fixture()
def admin_headers():
    return {"X-Admin-Key": "test-admin-key"}


@pytest.fixture()
def registered_device(client, admin_headers):
    """Daftarkan satu device, kembalikan (device_id, bearer_headers, hmac_secret)."""
    resp = client.post("/api/v1/devices", json={"id": "DOOR-01", "name": "Pintu Depan"}, headers=admin_headers)
    assert resp.status_code == 201, resp.text
    body = resp.json()
    bearer = {"Authorization": f"Bearer {body['api_token']}", "X-Device-Id": "DOOR-01"}
    return {"device_id": "DOOR-01", "bearer": bearer, "hmac_secret": body["hmac_secret"], "token": body["api_token"]}
