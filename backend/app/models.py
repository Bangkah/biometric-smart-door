"""
models.py — Skema database relasional (SRS Bab 9), diperluas Phase 4
dengan kolom autentikasi per-perangkat (api_token_hash, hmac_secret).
"""
from __future__ import annotations

import enum
import uuid
from datetime import UTC, datetime

from sqlalchemy import JSON, DateTime, Enum, ForeignKey, Integer, String, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .database import Base


def _now() -> datetime:
    return datetime.now(UTC)


def _uuid() -> str:
    return str(uuid.uuid4())


class UserStatus(str, enum.Enum):
    ACTIVE = "active"
    REVOKED = "revoked"


class CredentialStatus(str, enum.Enum):
    ACTIVE = "active"
    REVOKED = "revoked"


class DeviceStatus(str, enum.Enum):
    ONLINE = "online"
    OFFLINE = "offline"


class AccessResult(str, enum.Enum):
    GRANTED = "GRANTED"
    DENIED = "DENIED"


class AccessSource(str, enum.Enum):
    BIOMETRIC = "biometric"
    REMOTE_COMMAND = "remote_command"


class CommandType(str, enum.Enum):
    REMOTE_UNLOCK = "REMOTE_UNLOCK"
    ALLOW_SLOT = "ALLOW_SLOT"
    REVOKE_SLOT = "REVOKE_SLOT"


class CommandStatus(str, enum.Enum):
    PENDING = "pending"
    ACKNOWLEDGED = "acknowledged"  # device sudah menerima (GET), belum lapor hasil
    EXECUTED = "executed"
    EXPIRED = "expired"
    FAILED = "failed"
    CANCELLED = "cancelled"


# ------------------------------------------------------------
# 9.3 devices (+ kolom autentikasi Phase 4)
# ------------------------------------------------------------
class Device(Base):
    __tablename__ = "devices"

    id: Mapped[str] = mapped_column(String(64), primary_key=True)  # mis. "DOOR-01"
    name: Mapped[str] = mapped_column(String(128), default="")
    location: Mapped[str] = mapped_column(String(128), default="")
    firmware_version: Mapped[str] = mapped_column(String(64), default="")
    last_seen: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_now)

    # Bearer token yang dipakai firmware (DEVICE_API_TOKEN) disimpan sebagai
    # HASH SHA-256, bukan plaintext (SRS 12.1 semangat: kredensial tidak
    # disimpan mentah). Token asli hanya ditampilkan SEKALI saat device
    # didaftarkan (lihat routers/devices.py).
    api_token_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True)

    # Kunci HMAC-SHA256 terpisah dari bearer token (defense in depth):
    # dipakai backend untuk MENANDATANGANI daftar command sebelum dikirim,
    # dan firmware WAJIB memverifikasi tanda tangan ini sebelum mengeksekusi
    # apa pun (terutama REMOTE_UNLOCK) — mencegah command dipalsukan lewat
    # MITM/DNS-spoof walau bearer token bocor di satu jalur.
    hmac_secret: Mapped[str] = mapped_column(String(64))

    # Untuk melacak epoch ring buffer terakhir yang diterima per perangkat
    # (mendeteksi device yang di-reset log-nya, murni informasional/dashboard).
    last_log_epoch: Mapped[int] = mapped_column(Integer, default=0)

    credentials: Mapped[list[FingerprintCredential]] = relationship(back_populates="device")
    commands: Mapped[list[Command]] = relationship(back_populates="device")

    def status(self, stale_after_sec: int = 300) -> DeviceStatus:
        if self.last_seen is None:
            return DeviceStatus.OFFLINE
        last_seen = self.last_seen
        if last_seen.tzinfo is None:
            # SQLite tidak menyimpan tzinfo (nilai yang ditulis SELALU UTC,
            # lihat _now() di atas) -> tempel kembali UTC sebelum dikurangi
            # dengan datetime timezone-aware lain, supaya tidak TypeError.
            last_seen = last_seen.replace(tzinfo=UTC)
        age = (_now() - last_seen).total_seconds()
        return DeviceStatus.ONLINE if age <= stale_after_sec else DeviceStatus.OFFLINE


# ------------------------------------------------------------
# 9.1 users
# ------------------------------------------------------------
class User(Base):
    __tablename__ = "users"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    name: Mapped[str] = mapped_column(String(128))
    status: Mapped[UserStatus] = mapped_column(Enum(UserStatus), default=UserStatus.ACTIVE)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_now)

    credentials: Mapped[list[FingerprintCredential]] = relationship(back_populates="user")


# ------------------------------------------------------------
# 9.2 fingerprint_credentials
# ------------------------------------------------------------
class FingerprintCredential(Base):
    __tablename__ = "fingerprint_credentials"
    __table_args__ = (UniqueConstraint("device_id", "sensor_slot_id", name="uq_device_slot"),)

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    user_id: Mapped[str] = mapped_column(ForeignKey("users.id"))
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"))
    sensor_slot_id: Mapped[int] = mapped_column(Integer)
    status: Mapped[CredentialStatus] = mapped_column(Enum(CredentialStatus), default=CredentialStatus.ACTIVE)
    enrolled_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_now)
    revoked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    user: Mapped[User] = relationship(back_populates="credentials")
    device: Mapped[Device] = relationship(back_populates="credentials")


# ------------------------------------------------------------
# 9.4 access_logs
# ------------------------------------------------------------
class AccessLog(Base):
    __tablename__ = "access_logs"
    __table_args__ = (
        # Idempotensi ingestion (SRS 12.5): kombinasi ini identik dengan
        # kunci dedup yang sudah dipakai tools/mock_backend.py Phase 3.
        UniqueConstraint("device_id", "log_epoch", "boot_id", "sequence", name="uq_event_identity"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    log_epoch: Mapped[int] = mapped_column(Integer)
    boot_id: Mapped[int] = mapped_column(Integer)
    sequence: Mapped[int] = mapped_column(Integer)

    uptime_ms: Mapped[int] = mapped_column(Integer)
    timestamp: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    time_source: Mapped[str] = mapped_column(String(16))

    event_type: Mapped[str] = mapped_column(String(32))
    detail: Mapped[int] = mapped_column(Integer, default=-1)
    message: Mapped[str] = mapped_column(String(64), default="")

    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_now)


# ------------------------------------------------------------
# 9.5 commands
# ------------------------------------------------------------
class Command(Base):
    __tablename__ = "commands"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    command_type: Mapped[CommandType] = mapped_column(Enum(CommandType))
    payload: Mapped[dict] = mapped_column(JSON, default=dict)
    status: Mapped[CommandStatus] = mapped_column(Enum(CommandStatus), default=CommandStatus.PENDING)

    issued_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_now)
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_now)

    acknowledged_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    completed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    result_detail: Mapped[str] = mapped_column(String(128), default="")

    device: Mapped[Device] = relationship(back_populates="commands")
