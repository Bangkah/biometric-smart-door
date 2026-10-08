def test_heartbeat_updates_last_seen_and_firmware_version(client, admin_headers, registered_device):
    r = client.post(
        "/api/v1/devices/heartbeat",
        json={"firmware_version": "0.4.0-phase4", "uptime_ms": 123456},
        headers=registered_device["bearer"],
    )
    assert r.status_code == 200
    assert r.json()["status"] == "ok"
    assert isinstance(r.json()["server_time"], int)

    d = client.get("/api/v1/devices/DOOR-01", headers=admin_headers).json()
    assert d["status"] == "online"
    assert d["firmware_version"] == "0.4.0-phase4"


def test_heartbeat_does_not_require_any_events(client, admin_headers, registered_device):
    """Inti dari 'heartbeat terpisah dari event upload': device online HANYA
    lewat heartbeat, walau /events tidak pernah dipanggil sama sekali."""
    r = client.post(
        "/api/v1/devices/heartbeat", json={}, headers=registered_device["bearer"]
    )
    assert r.status_code == 200
    assert client.get("/api/v1/devices/DOOR-01", headers=admin_headers).json()["status"] == "online"


def test_heartbeat_rejects_invalid_token(client, registered_device):
    r = client.post(
        "/api/v1/devices/heartbeat",
        json={"uptime_ms": 1},
        headers={"Authorization": "Bearer salah", "X-Device-Id": "DOOR-01"},
    )
    assert r.status_code == 401


def test_heartbeat_rejects_invalid_payload_types(client, registered_device):
    """uptime_ms harus integer >= 0 — string/negatif ditolak (422), bukan
    diam-diam diterima lalu merusak data."""
    r = client.post(
        "/api/v1/devices/heartbeat",
        json={"uptime_ms": "bukan-angka"},
        headers=registered_device["bearer"],
    )
    assert r.status_code == 422

    r2 = client.post(
        "/api/v1/devices/heartbeat",
        json={"uptime_ms": -5},
        headers=registered_device["bearer"],
    )
    assert r2.status_code == 422
