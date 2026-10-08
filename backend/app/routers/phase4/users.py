"""routers/phase4/users.py — [PHASE 4] Remote User Sync (SRS 7.3/7.4, 8.5, FR-013).

Membuat/menghapus fingerprint_credentials di sini SECARA OTOMATIS
mengantre command ALLOW_SLOT/REVOKE_SLOT ke perangkat terkait — inilah
mekanisme "sinkronisasi izin akses over-the-air" yang diminta Phase 4,
melengkapi enrollment LOKAL yang sudah ada sejak Phase 1/2 (mock sidik jari).
"""
from __future__ import annotations

from datetime import UTC, datetime

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from ... import crud, models, schemas
from ...database import get_db
from ...security import require_admin

router = APIRouter(prefix="/api/v1/users", tags=["users"], dependencies=[Depends(require_admin)])


@router.post("", response_model=schemas.UserOut, status_code=201)
def create_user(body: schemas.UserCreate, db: Session = Depends(get_db)):
    user = models.User(name=body.name)
    db.add(user)
    db.commit()
    return user


@router.get("", response_model=list[schemas.UserOut])
def list_users(db: Session = Depends(get_db)):
    return db.query(models.User).order_by(models.User.created_at).all()


@router.post("/{user_id}/credentials", response_model=schemas.CredentialOut, status_code=201)
def enroll_credential(user_id: str, body: schemas.CredentialCreate, db: Session = Depends(get_db)):
    user = db.get(models.User, user_id)
    if user is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "User tidak ditemukan")
    if db.get(models.Device, body.device_id) is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Device tidak ditemukan")

    cred = models.FingerprintCredential(
        user_id=user_id, device_id=body.device_id, sensor_slot_id=body.sensor_slot_id
    )
    db.add(cred)
    try:
        db.flush()
    except IntegrityError:
        db.rollback()
        raise HTTPException(
            status.HTTP_409_CONFLICT,
            f"Slot {body.sensor_slot_id} di device '{body.device_id}' sudah dipakai kredensial lain",
        ) from None

    if body.push_command:
        crud.create_command(
            db,
            device_id=body.device_id,
            command_type="ALLOW_SLOT",
            payload={"slot": body.sensor_slot_id},
            ttl_seconds=3600,
        )
    db.commit()
    return cred


@router.delete("/{user_id}/credentials/{credential_id}", response_model=schemas.CredentialOut)
def revoke_credential(user_id: str, credential_id: str, db: Session = Depends(get_db)):
    cred = db.get(models.FingerprintCredential, credential_id)
    if cred is None or cred.user_id != user_id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Kredensial tidak ditemukan")

    if cred.status != models.CredentialStatus.REVOKED:
        cred.status = models.CredentialStatus.REVOKED
        cred.revoked_at = datetime.now(UTC)
        # SRS 7.4 Local-First Compliance: pencabutan diteruskan ke device
        # sebagai command, device menerapkannya begitu berhasil sinkron
        # (lihat firmware access_policy.h) — bukan seketika/synchronous,
        # konsisten dengan filosofi Edge Autonomy Phase 1.
        crud.create_command(
            db,
            device_id=cred.device_id,
            command_type="REVOKE_SLOT",
            payload={"slot": cred.sensor_slot_id},
            ttl_seconds=3600,
        )
    db.commit()
    return cred
