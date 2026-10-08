"""routers/phase4/commands.py — [PHASE 4] Remote command queue (SRS 8.4, 10.5, 10.6, 12.5).

Dua kelompok endpoint dengan model autentikasi BERBEDA (SENGAJA dipisah):
  - /api/v1/commands            [Admin: X-Admin-Key]  buat & lihat command
  - /api/v1/commands/poll       [Device: Bearer]       device mengambil command miliknya
  - /api/v1/commands/{id}/ack   [Device: Bearer]       device melaporkan hasil eksekusi

Command yang dikirim ke device SELALU ditandatangani HMAC per-command
(lihat security.sign_command) — firmware wajib verifikasi sebelum eksekusi.
"""
from __future__ import annotations

from datetime import UTC

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from ... import crud, models, schemas
from ...database import get_db
from ...security import get_current_device, require_admin

router = APIRouter(prefix="/api/v1/commands", tags=["commands"])


# ---------------- Admin: buat & lihat command ----------------
@router.post("", response_model=schemas.CommandOut, status_code=201, dependencies=[Depends(require_admin)])
def create_command(body: schemas.CommandCreate, db: Session = Depends(get_db)):
    if db.get(models.Device, body.device_id) is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Device tidak ditemukan")
    cmd = crud.create_command(
        db,
        device_id=body.device_id,
        command_type=body.command_type,
        payload=body.payload,
        ttl_seconds=body.ttl_seconds,
    )
    db.commit()
    return cmd


@router.get("", response_model=list[schemas.CommandOut], dependencies=[Depends(require_admin)])
def list_commands(device_id: str | None = None, db: Session = Depends(get_db)):
    crud.expire_stale_commands(db, device_id=device_id)
    q = db.query(models.Command)
    if device_id:
        q = q.filter(models.Command.device_id == device_id)
    cmds = q.order_by(models.Command.created_at.desc()).limit(200).all()
    db.commit()
    return cmds


# ---------------- Device: ambil & lapor hasil ----------------
@router.get("/poll", response_model=schemas.CommandListOut)
def poll_commands(
    device: models.Device = Depends(get_current_device),
    db: Session = Depends(get_db),
):
    crud.touch_device_seen(db, device)  # poll berkala == heartbeat implisit
    crud.expire_stale_commands(db, device_id=device.id)

    pending = (
        db.query(models.Command)
        .filter(
            models.Command.device_id == device.id,
            models.Command.status == models.CommandStatus.PENDING,
        )
        .order_by(models.Command.issued_at)
        .all()
    )
    wire = [crud.build_command_wire(device, c) for c in pending]
    for c in pending:
        c.status = models.CommandStatus.ACKNOWLEDGED
        from datetime import datetime

        c.acknowledged_at = datetime.now(UTC)
    db.commit()
    return schemas.CommandListOut(commands=wire)


@router.post("/{command_id}/ack", response_model=schemas.CommandOut)
def ack_command(
    command_id: str,
    body: schemas.CommandAckIn,
    device: models.Device = Depends(get_current_device),
    db: Session = Depends(get_db),
):
    cmd = db.get(models.Command, command_id)
    if cmd is None or cmd.device_id != device.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Command tidak ditemukan untuk perangkat ini")

    # Idempoten: ack kedua/berikutnya untuk command yang SUDAH final
    # (mis. firmware retry karena respons pertama hilang di jaringan)
    # tidak mengubah apa pun lagi, hanya mengembalikan status saat ini.
    terminal = {models.CommandStatus.EXECUTED, models.CommandStatus.FAILED, models.CommandStatus.EXPIRED,
                models.CommandStatus.CANCELLED}
    if cmd.status not in terminal:
        from datetime import datetime

        cmd.status = models.CommandStatus(body.status)
        cmd.completed_at = datetime.now(UTC)
        cmd.result_detail = body.detail
        db.commit()
    return cmd
