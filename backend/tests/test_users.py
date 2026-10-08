def test_enroll_credential_auto_creates_allow_slot_command(client, admin_headers, registered_device):
    user = client.post("/api/v1/users", json={"name": "Budi"}, headers=admin_headers).json()

    r = client.post(
        f"/api/v1/users/{user['id']}/credentials",
        json={"device_id": "DOOR-01", "sensor_slot_id": 3},
        headers=admin_headers,
    )
    assert r.status_code == 201
    assert r.json()["status"] == "active"

    cmds = client.get("/api/v1/commands", params={"device_id": "DOOR-01"}, headers=admin_headers).json()
    assert len(cmds) == 1
    assert cmds[0]["command_type"] == "ALLOW_SLOT"
    assert cmds[0]["payload"] == {"slot": 3}


def test_duplicate_slot_on_same_device_conflicts(client, admin_headers, registered_device):
    user1 = client.post("/api/v1/users", json={"name": "Budi"}, headers=admin_headers).json()
    user2 = client.post("/api/v1/users", json={"name": "Siti"}, headers=admin_headers).json()

    client.post(
        f"/api/v1/users/{user1['id']}/credentials",
        json={"device_id": "DOOR-01", "sensor_slot_id": 3},
        headers=admin_headers,
    )
    r = client.post(
        f"/api/v1/users/{user2['id']}/credentials",
        json={"device_id": "DOOR-01", "sensor_slot_id": 3},
        headers=admin_headers,
    )
    assert r.status_code == 409


def test_revoke_credential_auto_creates_revoke_slot_command(client, admin_headers, registered_device):
    user = client.post("/api/v1/users", json={"name": "Budi"}, headers=admin_headers).json()
    cred = client.post(
        f"/api/v1/users/{user['id']}/credentials",
        json={"device_id": "DOOR-01", "sensor_slot_id": 5},
        headers=admin_headers,
    ).json()

    r = client.delete(f"/api/v1/users/{user['id']}/credentials/{cred['id']}", headers=admin_headers)
    assert r.status_code == 200
    assert r.json()["status"] == "revoked"

    cmds = client.get("/api/v1/commands", params={"device_id": "DOOR-01"}, headers=admin_headers).json()
    types = sorted(c["command_type"] for c in cmds)
    assert types == ["ALLOW_SLOT", "REVOKE_SLOT"]


def test_revoke_is_idempotent_no_duplicate_command(client, admin_headers, registered_device):
    user = client.post("/api/v1/users", json={"name": "Budi"}, headers=admin_headers).json()
    cred = client.post(
        f"/api/v1/users/{user['id']}/credentials",
        json={"device_id": "DOOR-01", "sensor_slot_id": 5},
        headers=admin_headers,
    ).json()
    client.delete(f"/api/v1/users/{user['id']}/credentials/{cred['id']}", headers=admin_headers)
    client.delete(f"/api/v1/users/{user['id']}/credentials/{cred['id']}", headers=admin_headers)

    cmds = client.get("/api/v1/commands", params={"device_id": "DOOR-01"}, headers=admin_headers).json()
    revoke_cmds = [c for c in cmds if c["command_type"] == "REVOKE_SLOT"]
    assert len(revoke_cmds) == 1
