"""Unit test murni untuk security.py (tanpa DB/HTTP) — logika yang juga
direplikasi firmware (command_policy.h) didokumentasikan & diuji di sini
sebagai RUJUKAN, supaya kedua sisi (Python & C++) bisa dibandingkan."""
import pytest

from app.security import (
    canonical_command_payload,
    command_signature_string,
    is_timestamp_fresh,
    sign_command,
    verify_command_signature,
)


def test_canonical_payload_remote_unlock_is_empty():
    assert canonical_command_payload("REMOTE_UNLOCK", {}) == ""


def test_canonical_payload_allow_slot():
    assert canonical_command_payload("ALLOW_SLOT", {"slot": 7}) == "slot=7"


def test_canonical_payload_revoke_slot():
    assert canonical_command_payload("REVOKE_SLOT", {"slot": 0}) == "slot=0"


def test_canonical_payload_rejects_missing_slot():
    with pytest.raises(ValueError):
        canonical_command_payload("ALLOW_SLOT", {})


def test_canonical_payload_rejects_unknown_type():
    with pytest.raises(ValueError):
        canonical_command_payload("SELF_DESTRUCT", {})


def test_signature_string_format():
    s = command_signature_string("abc-123", "ALLOW_SLOT", 1000, 1060, {"slot": 7})
    assert s == "abc-123.ALLOW_SLOT.1000.1060.slot=7"


def test_sign_and_verify_roundtrip():
    sig = sign_command("secret1", "id1", "REMOTE_UNLOCK", 1000, 1060, {})
    assert verify_command_signature("secret1", "id1", "REMOTE_UNLOCK", 1000, 1060, {}, sig)


def test_verify_fails_with_wrong_secret():
    sig = sign_command("secret1", "id1", "REMOTE_UNLOCK", 1000, 1060, {})
    assert not verify_command_signature("secret2", "id1", "REMOTE_UNLOCK", 1000, 1060, {}, sig)


def test_verify_fails_if_any_field_tampered():
    sig = sign_command("secret1", "id1", "ALLOW_SLOT", 1000, 1060, {"slot": 7})
    assert not verify_command_signature("secret1", "id1", "ALLOW_SLOT", 1000, 1060, {"slot": 8}, sig)
    assert not verify_command_signature("secret1", "id1", "ALLOW_SLOT", 1000, 9999, {"slot": 7}, sig)
    assert not verify_command_signature("secret1", "id1-tampered", "ALLOW_SLOT", 1000, 1060, {"slot": 7}, sig)


def test_timestamp_freshness_window():
    assert is_timestamp_fresh(1000, 1000, window_sec=60)
    assert is_timestamp_fresh(1000, 940, window_sec=60)
    assert is_timestamp_fresh(1000, 1060, window_sec=60)
    assert not is_timestamp_fresh(1000, 939, window_sec=60)
    assert not is_timestamp_fresh(1000, 1061, window_sec=60)
