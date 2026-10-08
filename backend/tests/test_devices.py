def test_register_device_returns_token_once(client, admin_headers):
    r = client.post("/api/v1/devices", json={"id": "DOOR-01", "name": "Depan"}, headers=admin_headers)
    assert r.status_code == 201
    body = r.json()
    assert body["id"] == "DOOR-01"
    assert len(body["api_token"]) == 48  # 24 byte hex
    assert len(body["hmac_secret"]) == 64  # 32 byte hex


def test_register_duplicate_device_conflicts(client, admin_headers):
    client.post("/api/v1/devices", json={"id": "DOOR-01"}, headers=admin_headers)
    r = client.post("/api/v1/devices", json={"id": "DOOR-01"}, headers=admin_headers)
    assert r.status_code == 409


def test_devices_require_admin_key(client):
    r = client.post("/api/v1/devices", json={"id": "DOOR-01"})
    assert r.status_code in (401, 422)  # 422 jika header wajib hilang total di FastAPI


def test_devices_reject_wrong_admin_key(client):
    r = client.get("/api/v1/devices", headers={"X-Admin-Key": "salah"})
    assert r.status_code == 401


def test_list_devices_shows_offline_before_first_contact(client, admin_headers, registered_device):
    r = client.get("/api/v1/devices", headers=admin_headers)
    assert r.status_code == 200
    devices = r.json()
    assert len(devices) == 1
    assert devices[0]["status"] == "offline"
    assert devices[0]["last_seen"] is None


def test_device_status_online_after_event(client, admin_headers, registered_device):
    payload = {"device_id": "DOOR-01", "log_epoch": 1, "events": []}
    r = client.post("/api/v1/events", json=payload, headers=registered_device["bearer"])
    assert r.status_code == 201

    r = client.get("/api/v1/devices/DOOR-01", headers=admin_headers)
    assert r.json()["status"] == "online"
