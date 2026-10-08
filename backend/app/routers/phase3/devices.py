"""routers/devices.py — Device Management (SRS 8.2, 11.4) + provisioning
kredensial Phase 4 (Bearer token & HMAC secret)."""
from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from ... import models, schemas, security
from ...database import get_db
from ...security import require_admin

router = APIRouter(prefix="/api/v1/devices", tags=["devices"], dependencies=[Depends(require_admin)])


@router.post("", response_model=schemas.DeviceCreated, status_code=201)
def register_device(body: schemas.DeviceCreate, db: Session = Depends(get_db)):
    token = security.generate_token()
    hmac_secret = security.generate_hmac_secret()
    device = models.Device(
        id=body.id,
        name=body.name,
        location=body.location,
        api_token_hash=security.hash_token(token),
        hmac_secret=hmac_secret,
    )
    db.add(device)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(status.HTTP_409_CONFLICT, f"Device id '{body.id}' sudah terdaftar") from None

    # PENTING: token plaintext HANYA muncul di response ini, tidak pernah
    # lagi bisa diambil ulang (backend hanya menyimpan hash-nya). Salin ke
    # firmware secrets.h SEKARANG.
    return schemas.DeviceCreated(id=device.id, api_token=token, hmac_secret=hmac_secret)


@router.get("", response_model=list[schemas.DeviceOut])
def list_devices(db: Session = Depends(get_db)):
    devices = db.query(models.Device).order_by(models.Device.id).all()
    return [
        schemas.DeviceOut(
            id=d.id,
            name=d.name,
            location=d.location,
            firmware_version=d.firmware_version,
            last_seen=d.last_seen,
            status=d.status().value,
            last_log_epoch=d.last_log_epoch,
        )
        for d in devices
    ]


@router.get("/{device_id}", response_model=schemas.DeviceOut)
def get_device(device_id: str, db: Session = Depends(get_db)):
    d = db.get(models.Device, device_id)
    if d is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Device tidak ditemukan")
    return schemas.DeviceOut(
        id=d.id,
        name=d.name,
        location=d.location,
        firmware_version=d.firmware_version,
        last_seen=d.last_seen,
        status=d.status().value,
        last_log_epoch=d.last_log_epoch,
    )


@router.get("/{device_id}/logs", response_model=list[schemas.AccessLogOut])
def get_device_logs(device_id: str, limit: int = 100, db: Session = Depends(get_db)):
    if db.get(models.Device, device_id) is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Device tidak ditemukan")
    limit = max(1, min(limit, 500))
    rows = (
        db.query(models.AccessLog)
        .filter(models.AccessLog.device_id == device_id)
        .order_by(models.AccessLog.received_at.desc())
        .limit(limit)
        .all()
    )
    return rows
