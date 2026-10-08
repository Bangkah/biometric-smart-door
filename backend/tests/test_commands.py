import hashlib
import hmac as hmac_lib


def _resign(hmac_secret, cmd_wire):
    """Hitung ulang signature persis seperti firmware akan lakukan, untuk
    memverifikasi backend mengirim tanda tangan yang benar."""
    canonical_payload = ""
    if cmd_wire["command_type"] in ("ALLOW_SLOT", "REVOKE_SLOT"):
        canonical_payload = f"slot={cmd_wire['payload']['slot']}"
    msg = f"{cmd_wire['id']}.{cmd_wire['command_type']}.{cmd_wire['issued_at']}.{cmd_wire['expires_at']}.{canonical_payload}"
    return hmac_lib.new(hmac_secret.encode(), msg.encode(), hashlib.sha256).hexdigest()


def test_admin_can_create_and_list_command(client, admin_headers, registered_device):
    r = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 60},
        headers=admin_headers,
    )
    assert r.status_code == 201
    cmd = r.json()
    assert cmd["status"] == "pending"
    assert cmd["command_type"] == "REMOTE_UNLOCK"

    r2 = client.get("/api/v1/commands", params={"device_id": "DOOR-01"}, headers=admin_headers)
    assert len(r2.json()) == 1


def test_device_poll_returns_signed_command_and_marks_acknowledged(client, admin_headers, registered_device):
    client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 60},
        headers=admin_headers,
    )

    r = client.get("/api/v1/commands/poll", headers=registered_device["bearer"])
    assert r.status_code == 200
    cmds = r.json()["commands"]
    assert len(cmds) == 1
    wire = cmds[0]
    assert wire["command_type"] == "REMOTE_UNLOCK"

    # Signature harus bisa direproduksi memakai hmac_secret milik device ini.
    expected = _resign(registered_device["hmac_secret"], wire)
    assert wire["signature"] == expected

    # Command yang sudah di-poll -> status acknowledged, TIDAK muncul lagi di poll berikutnya.
    r2 = client.get("/api/v1/commands/poll", headers=registered_device["bearer"])
    assert r2.json()["commands"] == []

    admin_list = client.get("/api/v1/commands", params={"device_id": "DOOR-01"}, headers=admin_headers).json()
    assert admin_list[0]["status"] == "acknowledged"


def test_allow_slot_and_revoke_slot_signature_covers_payload(client, admin_headers, registered_device):
    r = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "ALLOW_SLOT", "payload": {"slot": 7}, "ttl_seconds": 60},
        headers=admin_headers,
    )
    assert r.status_code == 201

    wire = client.get("/api/v1/commands/poll", headers=registered_device["bearer"]).json()["commands"][0]
    assert wire["payload"] == {"slot": 7}
    assert wire["signature"] == _resign(registered_device["hmac_secret"], wire)

    # Payload berbeda (mis. dimanipulasi di tengah jalan) HARUS membuat
    # signature tidak lagi cocok -> firmware wajib menolaknya.
    tampered = dict(wire)
    tampered["payload"] = {"slot": 99}
    assert _resign(registered_device["hmac_secret"], tampered) != wire["signature"]


def test_two_devices_have_independent_hmac_secrets(client, admin_headers):
    d1 = client.post("/api/v1/devices", json={"id": "DOOR-01"}, headers=admin_headers).json()
    d2 = client.post("/api/v1/devices", json={"id": "DOOR-02"}, headers=admin_headers).json()
    assert d1["hmac_secret"] != d2["hmac_secret"]
    assert d1["api_token"] != d2["api_token"]

    client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 60},
        headers=admin_headers,
    )
    wire = client.get(
        "/api/v1/commands/poll",
        headers={"Authorization": f"Bearer {d1['api_token']}", "X-Device-Id": "DOOR-01"},
    ).json()["commands"][0]

    # Signature yang sah untuk DOOR-01 TIDAK BOLEH tervalidasi dengan kunci DOOR-02.
    assert wire["signature"] != _resign(d2["hmac_secret"], wire)


def test_device_ack_updates_status(client, admin_headers, registered_device):
    created = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 60},
        headers=admin_headers,
    ).json()
    cmd_id = created["id"]
    client.get("/api/v1/commands/poll", headers=registered_device["bearer"])

    r = client.post(
        f"/api/v1/commands/{cmd_id}/ack",
        json={"status": "executed", "detail": "unlocked"},
        headers=registered_device["bearer"],
    )
    assert r.status_code == 200
    assert r.json()["status"] == "executed"


def test_ack_is_idempotent_after_terminal_status(client, admin_headers, registered_device):
    created = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 60},
        headers=admin_headers,
    ).json()
    cmd_id = created["id"]
    client.get("/api/v1/commands/poll", headers=registered_device["bearer"])
    client.post(f"/api/v1/commands/{cmd_id}/ack", json={"status": "executed"}, headers=registered_device["bearer"])

    # Ack kedua (mis. retry firmware) dengan status BERBEDA tidak boleh menimpa hasil final.
    r = client.post(f"/api/v1/commands/{cmd_id}/ack", json={"status": "failed"}, headers=registered_device["bearer"])
    assert r.json()["status"] == "executed"


def test_device_cannot_ack_other_devices_command(client, admin_headers, registered_device):
    d2 = client.post("/api/v1/devices", json={"id": "DOOR-02"}, headers=admin_headers).json()
    created = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 60},
        headers=admin_headers,
    ).json()

    other_bearer = {"Authorization": f"Bearer {d2['api_token']}", "X-Device-Id": "DOOR-02"}
    r = client.post(f"/api/v1/commands/{created['id']}/ack", json={"status": "executed"}, headers=other_bearer)
    assert r.status_code == 404


def test_expired_command_not_returned_by_poll(client, admin_headers, registered_device):
    r = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 1},
        headers=admin_headers,
    )
    cmd_id = r.json()["id"]

    import time

    time.sleep(1.2)
    polled = client.get("/api/v1/commands/poll", headers=registered_device["bearer"]).json()
    assert polled["commands"] == []

    admin_view = client.get("/api/v1/commands", params={"device_id": "DOOR-01"}, headers=admin_headers).json()
    assert [c for c in admin_view if c["id"] == cmd_id][0]["status"] == "expired"
