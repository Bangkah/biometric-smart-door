"""routers/heartbeat.py — Device Heartbeat (SRS 8.2/10.3, FR-015).

SENGAJA endpoint TERPISAH dari /events dan /commands/poll: perangkat WAJIB
memanggilnya secara periodik TERLEPAS dari apakah ada event/command untuk
disinkronkan, sehingga dashboard tetap tahu perangkat "online" walau tidak
ada aktivitas akses sama sekali. Kegagalan endpoint ini (backend down)
TIDAK PERNAH memengaruhi fungsi buka-kunci lokal firmware (SRS 13.6) —
lihat wiki/Architecture.md untuk penjelasan arsitektur network task.
"""
from __future__ import annotations

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from ... import crud, models, schemas
from ...database import get_db
from ...security import get_current_device, now_unix

router = APIRouter(prefix="/api/v1/devices", tags=["heartbeat"])


@router.post("/heartbeat", response_model=schemas.HeartbeatOut)
def heartbeat(
    body: schemas.HeartbeatIn,
    device: models.Device = Depends(get_current_device),
    db: Session = Depends(get_db),
):
    crud.touch_device_seen(db, device, firmware_version=body.firmware_version or None)
    db.commit()
    return schemas.HeartbeatOut(status="ok", server_time=now_unix())
