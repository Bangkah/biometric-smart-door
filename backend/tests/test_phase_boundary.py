"""Membuktikan PHASE4_ENABLED=false benar-benar MENIADAKAN router Phase 4
(users, commands) dari aplikasi — bukan sekadar menyembunyikannya di balik
auth. Endpoint Phase 3 (events, devices, heartbeat) tetap berfungsi penuh.

Ini mekanisme teknis di balik opsi 'pisahkan tapi tidak dihapus': branch
`phase-3` cukup set env var ini, tanpa menghapus/menulis ulang kode
(lihat app/main.py:create_app() dan wiki/Architecture.md, bagian Batas Fase).
"""


def test_phase4_routes_absent_when_disabled(client_phase4_disabled, admin_headers):
    c = client_phase4_disabled
    # 404 (route tidak terdaftar sama sekali), BUKAN 401/403 (yang berarti
    # route ADA tapi ditolak auth) — buktinya router benar-benar tidak di-mount.
    assert c.get("/api/v1/users", headers=admin_headers).status_code == 404
    assert c.post("/api/v1/users", json={"name": "x"}, headers=admin_headers).status_code == 404
    assert c.get("/api/v1/commands", headers=admin_headers).status_code == 404
    assert (
        c.post(
            "/api/v1/commands",
            json={"device_id": "X", "command_type": "REMOTE_UNLOCK"},
            headers=admin_headers,
        ).status_code
        == 404
    )


def test_phase3_routes_still_work_when_phase4_disabled(client_phase4_disabled, admin_headers):
    c = client_phase4_disabled
    r = c.post("/api/v1/devices", json={"id": "DOOR-01"}, headers=admin_headers)
    assert r.status_code == 201
    token = r.json()["api_token"]
    bearer = {"Authorization": f"Bearer {token}", "X-Device-Id": "DOOR-01"}

    assert (
        c.post(
            "/api/v1/events", json={"device_id": "DOOR-01", "log_epoch": 1, "events": []}, headers=bearer
        ).status_code
        == 201
    )
    assert c.post("/api/v1/devices/heartbeat", json={}, headers=bearer).status_code == 200
    assert c.get("/api/v1/devices", headers=admin_headers).status_code == 200


def test_health_reports_phase4_flag(client, client_phase4_disabled):
    assert client.get("/health").json() == {"status": "ok", "phase4_enabled": True}
    assert client_phase4_disabled.get("/health").json() == {"status": "ok", "phase4_enabled": False}


def test_phase4_enabled_by_default(client, admin_headers):
    """Fixture `client` biasa (tanpa override) HARUS tetap punya Phase 4
    aktif — default proyek ini saat ini memang sedang mengerjakan Phase 4."""
    r = client.get("/api/v1/users", headers=admin_headers)
    assert r.status_code == 200  # route ADA (list kosong), bukan 404


def test_dashboard_served_at_root(client):
    r = client.get("/")
    assert r.status_code == 200
    assert "text/html" in r.headers["content-type"]
    assert "Dashboard" in r.text
