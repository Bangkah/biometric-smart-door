"""routers/events.py — Ingestion Endpoint (SRS 10.2, FR-009).

Kontrak payload IDENTIK dengan yang dibangun api_client.cpp Phase 3:
firmware yang sudah di-deploy TIDAK PERLU di-update untuk memakai backend
Phase 4 ini. Idempoten berdasarkan (device_id, log_epoch, boot_id, sequence)
— retry firmware akibat jaringan putus di tengah tidak pernah menggandakan log.
"""
from __future__ import annotations

from datetime import UTC, datetime

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from ... import crud, models, schemas
from ...database import get_db
from ...security import get_current_device

router = APIRouter(tags=["events"])


@router.post("/api/v1/events", response_model=schemas.EventBatchOut, status_code=201)
def ingest_events(
    batch: schemas.EventBatchIn,
    device: models.Device = Depends(get_current_device),
    db: Session = Depends(get_db),
):
    crud.touch_device_seen(db, device, firmware_version=batch.firmware_version)
    if batch.log_epoch > device.last_log_epoch:
        device.last_log_epoch = batch.log_epoch

    if not batch.events:
        db.commit()
        return schemas.EventBatchOut(accepted=0, duplicates=0, last_sequence=0)

    # Cek existing dalam SATU query (bukan per-baris) untuk idempotensi.
    existing_rows = (
        db.query(
            models.AccessLog.device_id,
            models.AccessLog.log_epoch,
            models.AccessLog.boot_id,
            models.AccessLog.sequence,
        )
        .filter(
            models.AccessLog.device_id == device.id,
            models.AccessLog.log_epoch == batch.log_epoch,
            models.AccessLog.boot_id.in_({e.boot_id for e in batch.events}),
        )
        .all()
    )
    existing_keys = set(existing_rows)

    accepted = 0
    last_sequence = 0
    for e in batch.events:
        last_sequence = max(last_sequence, e.sequence)
        key = (device.id, batch.log_epoch, e.boot_id, e.sequence)
        if key in existing_keys:
            continue
        ts = datetime.fromtimestamp(e.timestamp, tz=UTC) if e.timestamp else None
        db.add(
            models.AccessLog(
                device_id=device.id,
                log_epoch=batch.log_epoch,
                boot_id=e.boot_id,
                sequence=e.sequence,
                uptime_ms=e.uptime_ms,
                timestamp=ts,
                time_source=e.time_source,
                event_type=e.type,
                detail=e.detail,
                message=e.message,
            )
        )
        existing_keys.add(key)  # jaga-jaga ada duplikat DI DALAM satu batch yang sama
        accepted += 1

    db.commit()
    duplicates = len(batch.events) - accepted
    return schemas.EventBatchOut(accepted=accepted, duplicates=duplicates, last_sequence=last_sequence)
