"""
security.py — Autentikasi & integritas Phase 4 (SRS 12.1-12.5).

Dua lapis kredensial per perangkat, SENGAJA dipisah (defense in depth):
  1. api_token  (Bearer)   — otentikasi permintaan device -> backend
                             (events, poll commands, ack).
  2. hmac_secret            — dipakai BACKEND menandatangani setiap command
                             SEBELUM dikirim; firmware WAJIB verifikasi
                             tanda tangan ini sebelum mengeksekusi apa pun.
                             Bocornya bearer token saja TIDAK cukup untuk
                             memalsukan perintah REMOTE_UNLOCK yang valid.

Fungsi-fungsi murni di bawah (canonical_command_payload, command_signature_string,
is_timestamp_fresh) sengaja dipisah dari I/O (DB/HTTP) agar mudah di-unit-test
(lihat backend/tests/test_security.py) dan agar firmware C++ punya SATU
definisi rujukan yang jelas untuk direplikasi (lihat komentar di setiap fungsi).
"""
from __future__ import annotations

import hashlib
import hmac
import secrets
from datetime import UTC, datetime

from fastapi import Depends, Header, HTTPException, status
from sqlalchemy.orm import Session

from .config import get_settings
from .database import get_db
from .models import Device


# ------------------------------------------------------------
# Pembuatan & hashing kredensial
# ------------------------------------------------------------
def generate_token() -> str:
    """Bearer token acak (48 hex char / 192 bit)."""
    return secrets.token_hex(24)


def generate_hmac_secret() -> str:
    """Kunci HMAC acak, terpisah dari bearer token (64 hex char / 256 bit)."""
    return secrets.token_hex(32)


def hash_token(token: str) -> str:
    """SHA-256 hex. Backend TIDAK PERNAH menyimpan token bearer plaintext
    (hanya ditampilkan sekali saat device didaftarkan)."""
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


# ------------------------------------------------------------
# Tanda tangan command (HMAC-SHA256 atas string ringkas, BUKAN JSON generik)
#
# Kenapa bukan "HMAC atas JSON"? Kanonikalisasi JSON generik (urutan key,
# whitespace, encoding angka/unicode) mudah meleset antara implementasi
# Python (backend) dan C++/ArduinoJson (firmware) yang mem-parsing ulang
# lalu men-serialize ulang — kesalahan 1 spasi saja membuat signature tidak
# pernah cocok. Sebagai gantinya, string yang ditandatangani dibangun dari
# field-field MENTAH (sebelum di-serialize ke JSON) dengan format yang
# eksplisit dan sederhana, sehingga firmware tinggal menyusun ulang string
# yang SAMA PERSIS dari field yang sudah di-parse — tanpa bergantung pada
# representasi JSON apa pun.
# ------------------------------------------------------------
def canonical_command_payload(command_type: str, payload: dict) -> str:
    """Representasi string payload yang RINGKAS & DETERMINISTIK, untuk
    ruang lingkup command_type yang didukung Phase 4 (lihat models.CommandType).
    Firmware HARUS membangun string identik dari command_type + payload
    yang sama (lihat command_policy.h:canonicalCommandPayload di firmware).
    """
    if command_type == "REMOTE_UNLOCK":
        return ""
    if command_type in ("ALLOW_SLOT", "REVOKE_SLOT"):
        slot = payload.get("slot")
        if not isinstance(slot, int):
            raise ValueError(f"payload '{command_type}' wajib berisi integer 'slot'")
        return f"slot={slot}"
    raise ValueError(f"command_type tidak dikenal: {command_type}")


def command_signature_string(
    command_id: str, command_type: str, issued_at: int, expires_at: int, payload: dict
) -> str:
    """String final yang di-HMAC. Format:
    "<id>.<command_type>.<issued_at>.<expires_at>.<canonical_payload>"
    """
    canonical_payload = canonical_command_payload(command_type, payload)
    return f"{command_id}.{command_type}.{issued_at}.{expires_at}.{canonical_payload}"


def sign_command(
    hmac_secret: str, command_id: str, command_type: str, issued_at: int, expires_at: int, payload: dict
) -> str:
    msg = command_signature_string(command_id, command_type, issued_at, expires_at, payload)
    return hmac.new(hmac_secret.encode("utf-8"), msg.encode("utf-8"), hashlib.sha256).hexdigest()


def verify_command_signature(
    hmac_secret: str,
    command_id: str,
    command_type: str,
    issued_at: int,
    expires_at: int,
    payload: dict,
    signature_hex: str,
) -> bool:
    expected = sign_command(hmac_secret, command_id, command_type, issued_at, expires_at, payload)
    # constant-time compare -> hindari timing attack saat verifikasi (juga
    # dipraktikkan simetris di firmware, lihat command_policy.h).
    return hmac.compare_digest(expected, signature_hex)


# ------------------------------------------------------------
# Kesegaran timestamp (anti-replay kasar, SRS 12.5)
# ------------------------------------------------------------
def is_timestamp_fresh(now_unix: int, ts_unix: int, window_sec: int) -> bool:
    """True bila |now - ts| <= window. Dipakai backend untuk memvalidasi
    `sent_at` pada ingestion event (opsional, non-blocking bila device belum
    NTP-sync -> ts_unix boleh None di pemanggil), dan sebagai referensi
    logika yang direplikasi firmware untuk memvalidasi `issued_at` command."""
    return abs(now_unix - ts_unix) <= window_sec


def now_unix() -> int:
    return int(datetime.now(UTC).timestamp())


# ------------------------------------------------------------
# Dependencies FastAPI
# ------------------------------------------------------------
def require_admin(x_admin_key: str = Header(..., alias="X-Admin-Key")) -> None:
    settings = get_settings()
    if not hmac.compare_digest(x_admin_key, settings.admin_api_key):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Admin key tidak valid")


def get_current_device(
    authorization: str = Header(..., alias="Authorization"),
    x_device_id: str = Header(..., alias="X-Device-Id"),
    db: Session = Depends(get_db),
) -> Device:
    """Dependency: validasi Bearer token milik device X-Device-Id. Dipakai
    oleh endpoint events, commands (poll), dan commands ack — SEMUA endpoint
    yang dipanggil firmware, BUKAN endpoint admin/dashboard."""
    if not authorization.startswith("Bearer "):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Header Authorization harus 'Bearer <token>'")
    token = authorization[len("Bearer ") :].strip()
    if not token:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Token kosong")

    token_hash = hash_token(token)
    device = db.query(Device).filter(Device.api_token_hash == token_hash).one_or_none()

    if device is None or device.id != x_device_id:
        # Pesan generik sengaja SAMA untuk "token salah" vs "device_id tidak
        # cocok" -> tidak membocorkan info mana yang salah ke penyerang.
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Kredensial perangkat tidak valid")

    return device
