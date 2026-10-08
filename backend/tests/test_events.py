def _make_event(seq, boot_id=5, type_="ACCESS_GRANTED", detail=1, msg="ok", ts=1735689700):
    return {
        "sequence": seq, "boot_id": boot_id, "uptime_ms": seq * 1000,
        "timestamp": ts, "time_source": "ntp", "type": type_, "detail": detail, "message": msg,
    }


def test_ingest_events_accepts_new_batch(client, registered_device):
    payload = {
        "device_id": "DOOR-01", "firmware_version": "0.4.0", "log_epoch": 1,
        "sent_at": 1735689700, "missed_events": 0,
        "events": [_make_event(1), _make_event(2), _make_event(3)],
    }
    r = client.post("/api/v1/events", json=payload, headers=registered_device["bearer"])
    assert r.status_code == 201
    body = r.json()
    assert body == {"accepted": 3, "duplicates": 0, "last_sequence": 3}


def test_ingest_events_is_idempotent_on_retry(client, registered_device):
    payload = {"device_id": "DOOR-01", "log_epoch": 1, "events": [_make_event(1), _make_event(2)]}
    headers = registered_device["bearer"]

    r1 = client.post("/api/v1/events", json=payload, headers=headers)
    assert r1.json() == {"accepted": 2, "duplicates": 0, "last_sequence": 2}

    # Retry identik (mis. respons pertama hilang di jaringan) -> tidak boleh dobel.
    r2 = client.post("/api/v1/events", json=payload, headers=headers)
    assert r2.json() == {"accepted": 0, "duplicates": 2, "last_sequence": 2}


def test_ingest_events_partial_overlap_only_new_accepted(client, registered_device):
    headers = registered_device["bearer"]
    client.post(
        "/api/v1/events",
        json={"device_id": "DOOR-01", "log_epoch": 1, "events": [_make_event(1), _make_event(2)]},
        headers=headers,
    )
    r = client.post(
        "/api/v1/events",
        json={"device_id": "DOOR-01", "log_epoch": 1, "events": [_make_event(2), _make_event(3)]},
        headers=headers,
    )
    assert r.json() == {"accepted": 1, "duplicates": 1, "last_sequence": 3}


def test_ingest_events_different_epoch_not_deduped(client, registered_device):
    """Sequence sama tapi log_epoch beda (setelah device di-`clearAll()`,
    Phase 2/3) HARUS dianggap event baru, bukan duplikat."""
    headers = registered_device["bearer"]
    client.post(
        "/api/v1/events",
        json={"device_id": "DOOR-01", "log_epoch": 1, "events": [_make_event(1)]},
        headers=headers,
    )
    r = client.post(
        "/api/v1/events",
        json={"device_id": "DOOR-01", "log_epoch": 2, "events": [_make_event(1)]},
        headers=headers,
    )
    assert r.json() == {"accepted": 1, "duplicates": 0, "last_sequence": 1}


def test_ingest_events_rejects_missing_token(client, registered_device):
    payload = {"device_id": "DOOR-01", "log_epoch": 1, "events": [_make_event(1)]}
    r = client.post("/api/v1/events", json=payload, headers={"X-Device-Id": "DOOR-01"})
    assert r.status_code in (401, 422)


def test_ingest_events_rejects_wrong_token(client, registered_device):
    payload = {"device_id": "DOOR-01", "log_epoch": 1, "events": [_make_event(1)]}
    bad_headers = {"Authorization": "Bearer salah-token-ini", "X-Device-Id": "DOOR-01"}
    r = client.post("/api/v1/events", json=payload, headers=bad_headers)
    assert r.status_code == 401


def test_ingest_events_rejects_device_id_header_mismatch(client, registered_device):
    """Token benar milik DOOR-01, tapi header X-Device-Id mengaku device lain."""
    payload = {"device_id": "DOOR-02", "log_epoch": 1, "events": [_make_event(1)]}
    headers = dict(registered_device["bearer"])
    headers["X-Device-Id"] = "DOOR-02"
    r = client.post("/api/v1/events", json=payload, headers=headers)
    assert r.status_code == 401


def test_logs_visible_via_admin_endpoint(client, admin_headers, registered_device):
    payload = {"device_id": "DOOR-01", "log_epoch": 1, "events": [_make_event(1), _make_event(2)]}
    client.post("/api/v1/events", json=payload, headers=registered_device["bearer"])

    r = client.get("/api/v1/devices/DOOR-01/logs", headers=admin_headers)
    assert r.status_code == 200
    logs = r.json()
    assert len(logs) == 2
    assert logs[0]["sequence"] in (1, 2)
