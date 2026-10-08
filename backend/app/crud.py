"""crud.py — operasi database yang dipakai bersama beberapa router."""
from __future__ import annotations

from datetime import UTC, datetime, timedelta

from sqlalchemy.orm import Session

from . import models, schemas, security


def touch_device_seen(db: Session, device: models.Device, firmware_version: str | None = None) -> None:
    """Perbarui last_seen (+firmware_version bila dikirim). Dipanggil dari
    endpoint events DAN commands/poll -> keduanya berfungsi sebagai
    heartbeat implisit (SRS 10.3), tanpa memerlukan endpoint/panggilan
    firmware terpisah untuk itu (lihat catatan desain di wiki/API-Reference)."""
    device.last_seen = datetime.now(UTC)
    if firmware_version:
        device.firmware_version = firmware_version


def expire_stale_commands(db: Session, device_id: str | None = None) -> int:
    """Tandai command pending/acknowledged yang sudah lewat expires_at
    sebagai EXPIRED. Lazy: dijalankan tiap kali command dibaca (poll device
    atau list admin), bukan lewat scheduler terpisah — cukup untuk skala
    Phase 4 dan menghindari perlu proses background tambahan."""
    q = db.query(models.Command).filter(
        models.Command.status.in_([models.CommandStatus.PENDING, models.CommandStatus.ACKNOWLEDGED]),
        models.Command.expires_at < datetime.now(UTC),
    )
    if device_id:
        q = q.filter(models.Command.device_id == device_id)
    n = 0
    for cmd in q.all():
        cmd.status = models.CommandStatus.EXPIRED
        cmd.completed_at = datetime.now(UTC)
        cmd.result_detail = "expired sebelum diambil/dieksekusi perangkat"
        n += 1
    if n:
        db.flush()
    return n


def build_command_wire(device: models.Device, cmd: models.Command) -> schemas.CommandWire:
    issued_at = int(cmd.issued_at.timestamp())
    expires_at = int(cmd.expires_at.timestamp())
    signature = security.sign_command(
        device.hmac_secret, cmd.id, cmd.command_type.value, issued_at, expires_at, cmd.payload
    )
    return schemas.CommandWire(
        id=cmd.id,
        command_type=cmd.command_type.value,
        payload=cmd.payload,
        issued_at=issued_at,
        expires_at=expires_at,
        signature=signature,
    )


def create_command(
    db: Session,
    device_id: str,
    command_type: str,
    payload: dict,
    ttl_seconds: int,
) -> models.Command:
    now = datetime.now(UTC)
    cmd = models.Command(
        device_id=device_id,
        command_type=models.CommandType(command_type),
        payload=payload,
        status=models.CommandStatus.PENDING,
        issued_at=now,
        expires_at=now + timedelta(seconds=ttl_seconds),
    )
    db.add(cmd)
    db.flush()
    return cmd
