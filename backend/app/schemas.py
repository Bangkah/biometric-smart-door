"""
schemas.py — Pydantic request/response models.

EventIn/EventBatchIn mengikuti PERSIS payload yang dibangun api_client.cpp
(Phase 3, lihat syncOnce()) — tidak ada perubahan kontrak, firmware Phase 3
yang sudah di-deploy tetap kompatibel tanpa update.
"""
from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field


# ---------------- Events ingestion (kontrak Phase 3, TIDAK berubah) ----------------
class EventIn(BaseModel):
    sequence: int
    boot_id: int
    uptime_ms: int
    timestamp: int | None = None
    time_source: Literal["ntp", "estimated", "unsynced"]
    type: str
    detail: int = -1
    message: str = ""


class EventBatchIn(BaseModel):
    device_id: str
    firmware_version: str = ""
    log_epoch: int
    sent_at: int | None = None
    missed_events: int = 0
    events: list[EventIn]


class EventBatchOut(BaseModel):
    accepted: int
    duplicates: int
    last_sequence: int


# ---------------- Heartbeat (device) ----------------
class HeartbeatIn(BaseModel):
    """SRS 10.3 Device Heartbeat / FR-015 — SENGAJA endpoint terpisah dari
    /events dan /commands/poll: kegagalan heartbeat TIDAK BOLEH terikat pada
    ada/tidaknya event untuk disinkron (lihat wiki/Architecture.md)."""
    firmware_version: str = ""
    uptime_ms: int = Field(default=0, ge=0)
    free_heap_bytes: int | None = None


class HeartbeatOut(BaseModel):
    status: str  # "ok"
    server_time: int  # epoch unix — device BOLEH pakai ini sebagai time source cadangan


# ---------------- Devices (admin) ----------------
class DeviceCreate(BaseModel):
    id: str = Field(..., min_length=1, max_length=64)
    name: str = ""
    location: str = ""


class DeviceCreated(BaseModel):
    id: str
    api_token: str  # plaintext, DITAMPILKAN SEKALI SAJA saat pembuatan
    hmac_secret: str


class DeviceOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: str
    name: str
    location: str
    firmware_version: str
    last_seen: datetime | None
    status: str
    last_log_epoch: int


# ---------------- Users & credentials (remote user sync) ----------------
class UserCreate(BaseModel):
    name: str = Field(..., min_length=1, max_length=128)


class UserOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: str
    name: str
    status: str
    created_at: datetime


class CredentialCreate(BaseModel):
    device_id: str
    sensor_slot_id: int = Field(..., ge=0)
    # Jika True, otomatis membuat command ALLOW_SLOT ke perangkat (default).
    push_command: bool = True


class CredentialOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: str
    user_id: str
    device_id: str
    sensor_slot_id: int
    status: str
    enrolled_at: datetime
    revoked_at: datetime | None


# ---------------- Commands ----------------
class CommandCreate(BaseModel):
    device_id: str
    command_type: Literal["REMOTE_UNLOCK", "ALLOW_SLOT", "REVOKE_SLOT"]
    payload: dict = Field(default_factory=dict)
    ttl_seconds: int = Field(default=120, ge=1, le=3600)


class CommandOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: str
    device_id: str
    command_type: str
    payload: dict
    status: str
    issued_at: datetime
    expires_at: datetime


class CommandWire(BaseModel):
    """Bentuk command persis seperti dikirim ke firmware, sudah termasuk
    tanda tangan HMAC PER-COMMAND (lihat security.py:sign_command)."""
    id: str
    command_type: str
    payload: dict
    issued_at: int  # epoch unix (detik) — bentuk ringkas untuk firmware
    expires_at: int
    signature: str  # HMAC-SHA256 hex, kunci = device.hmac_secret


class CommandListOut(BaseModel):
    commands: list[CommandWire]


class CommandAckIn(BaseModel):
    status: Literal["executed", "failed", "expired"]
    detail: str = ""


# ---------------- Access logs (dashboard) ----------------
class AccessLogOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    device_id: str
    log_epoch: int
    boot_id: int
    sequence: int
    uptime_ms: int
    timestamp: datetime | None
    time_source: str
    event_type: str
    detail: int
    message: str
    received_at: datetime
