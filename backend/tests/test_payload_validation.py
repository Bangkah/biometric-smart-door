"""Test eksplisit 'invalid payload ditolak' untuk endpoint events — item
yang secara spesifik diminta checklist review (bukan hanya auth salah)."""


def test_events_rejects_missing_required_field(client, registered_device):
    # 'sequence' wajib ada di setiap event (schemas.EventIn) — hilang -> 422.
    payload = {
        "device_id": "DOOR-01",
        "log_epoch": 1,
        "events": [{"boot_id": 1, "uptime_ms": 1, "time_source": "ntp", "type": "SYSTEM_BOOT"}],
    }
    r = client.post("/api/v1/events", json=payload, headers=registered_device["bearer"])
    assert r.status_code == 422


def test_events_rejects_invalid_time_source_enum(client, registered_device):
    payload = {
        "device_id": "DOOR-01",
        "log_epoch": 1,
        "events": [
            {
                "sequence": 1, "boot_id": 1, "uptime_ms": 1,
                "time_source": "waktu-ngasal",  # bukan salah satu dari ntp/estimated/unsynced
                "type": "SYSTEM_BOOT",
            }
        ],
    }
    r = client.post("/api/v1/events", json=payload, headers=registered_device["bearer"])
    assert r.status_code == 422


def test_events_rejects_malformed_json_body(client, registered_device):
    r = client.post(
        "/api/v1/events",
        content=b"{not valid json",
        headers={**registered_device["bearer"], "Content-Type": "application/json"},
    )
    assert r.status_code == 422


def test_commands_create_rejects_invalid_command_type(client, admin_headers, registered_device):
    r = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "SELF_DESTRUCT", "ttl_seconds": 60},
        headers=admin_headers,
    )
    assert r.status_code == 422


def test_commands_create_rejects_ttl_out_of_range(client, admin_headers, registered_device):
    r = client.post(
        "/api/v1/commands",
        json={"device_id": "DOOR-01", "command_type": "REMOTE_UNLOCK", "ttl_seconds": 99999},
        headers=admin_headers,
    )
    assert r.status_code == 422
