#!/usr/bin/env python3
"""
<<<<<<< HEAD
mock_backend.py — backend palsu untuk bench-test END-TO-END Phase 3 + Phase 4
(HANYA stdlib Python 3.7+, tanpa dependensi eksternal).

Phase 3: POST /api/v1/events, POST /api/v1/devices/heartbeat
Phase 4: GET /api/v1/commands/poll, POST /api/v1/commands/{id}/ack,
         plus endpoint ADMIN lokal (--admin-port) untuk MENGANTRE command
         baru secara interaktif saat server sedang jalan — mensimulasikan
         "operator menekan tombol Remote Unlock di dashboard" tanpa perlu
         backend FastAPI sungguhan.

HMAC per-command DIHITUNG SUNGGUHAN di sini (bukan string kosong/dummy),
persis seperti backend/app/security.py, supaya firmware yang verifikasi
signature-nya benar-benar diuji ujung-ke-ujung, bukan dilewati.

Contoh pemakaian:
  # Terminal 1: jalankan server (port event/command + port admin terpisah)
  python tools/mock_backend.py --token dev-token-123 --hmac-secret dev-hmac-456

  # Terminal 2: antre command REMOTE_UNLOCK untuk DOOR-01
  python tools/mock_backend.py --enqueue REMOTE_UNLOCK --device DOOR-01

  # Antre ALLOW_SLOT / REVOKE_SLOT dengan payload
  python tools/mock_backend.py --enqueue ALLOW_SLOT --device DOOR-01 --slot 3
  python tools/mock_backend.py --enqueue REVOKE_SLOT --device DOOR-01 --slot 3

  # Simulasikan backend down N request pertama (uji retry/backoff)
  python tools/mock_backend.py --token dev-token-123 --hmac-secret dev-hmac-456 --fail-next 3

Di firmware (include/secrets.h), untuk env esp32dev-bench:
  #define DEVICE_API_TOKEN        "dev-token-123"
  #define DEVICE_HMAC_SECRET      "dev-hmac-456"
  #define BACKEND_EVENTS_ENDPOINT "http://<IP-PC-Anda>:8000/api/v1/events"
"""
import argparse
import hashlib
import hmac as hmac_lib
import json
import threading
import time
import urllib.request
import uuid
from datetime import datetime, timedelta, timezone
=======
mock_backend.py — backend palsu untuk bench-test Phase 3 (HANYA stdlib Python 3.7+).

Menerima POST /api/v1/events dari firmware, memvalidasi token, melakukan
deduplikasi idempoten, dan mencetak setiap event.

Contoh:
  python tools/mock_backend.py --token dev-token-123
  python tools/mock_backend.py --token dev-token-123 --fail-next 3   # 3 request pertama dibalas 503
  python tools/mock_backend.py --token dev-token-123 --port 8000

Di firmware (include/secrets.h), untuk env esp32dev-bench:
  #define DEVICE_API_TOKEN        "dev-token-123"
  #define BACKEND_EVENTS_ENDPOINT "http://<IP-PC-Anda>:8000/api/v1/events"
"""
import argparse
import json
import threading
from datetime import datetime, timezone, timedelta
>>>>>>> origin/main
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

WIB = timezone(timedelta(hours=7))
LOCK = threading.Lock()
<<<<<<< HEAD
SEEN = set()  # kunci dedup event: (device_id, log_epoch, boot_id, sequence)
COMMANDS = []  # list of dict, in-memory (hilang saat server di-restart -- ini mock, bukan Phase 3/4 asli)
STATE = {"requests": 0, "fail_left": 0, "token": "", "hmac_secret": "", "events": 0, "dups": 0}
=======
SEEN = set()            # kunci dedup: (device_id, log_epoch, boot_id, sequence)
STATE = {"requests": 0, "fail_left": 0, "token": "", "events": 0, "dups": 0}
>>>>>>> origin/main


def fmt_ts(ts):
    if ts is None:
<<<<<<< HEAD
        return "(tanpa waktu)"
    return datetime.fromtimestamp(ts, tz=timezone.utc).astimezone(WIB).strftime("%Y-%m-%d %H:%M:%S WIB")


def canonical_payload(command_type, payload):
    """HARUS identik dengan backend/app/security.py:canonical_command_payload
    dan firmware src/phase4/command_policy.cpp:canonicalPayload."""
    if command_type == "REMOTE_UNLOCK":
        return ""
    if command_type in ("ALLOW_SLOT", "REVOKE_SLOT"):
        return f"slot={payload['slot']}"
    raise ValueError(f"command_type tidak dikenal: {command_type}")


def sign_command(hmac_secret, command_id, command_type, issued_at, expires_at, payload):
    """HARUS identik dengan backend/app/security.py:sign_command."""
    msg = f"{command_id}.{command_type}.{issued_at}.{expires_at}.{canonical_payload(command_type, payload)}"
    return hmac_lib.new(hmac_secret.encode(), msg.encode(), hashlib.sha256).hexdigest()


=======
        return "         (tanpa waktu)        "
    return datetime.fromtimestamp(ts, tz=timezone.utc).astimezone(WIB).strftime("%Y-%m-%d %H:%M:%S WIB")


>>>>>>> origin/main
class Handler(BaseHTTPRequestHandler):
    def _reply(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

<<<<<<< HEAD
    def log_message(self, *args):  # senyapkan log akses default
        pass

    def _check_auth(self, device_id_from_body=None):
        if self.headers.get("Authorization", "") != f"Bearer {STATE['token']}":
            self._reply(401, {"error": "invalid device token"})
            return False
        xdid = self.headers.get("X-Device-Id")
        if device_id_from_body is not None and xdid != device_id_from_body:
            self._reply(403, {"error": "device id mismatch"})
            return False
        return True

    # ---------------- Phase 3 ----------------
=======
    def log_message(self, *args):   # senyapkan log akses default
        pass

>>>>>>> origin/main
    def do_POST(self):
        with LOCK:
            STATE["requests"] += 1
            n = STATE["requests"]
<<<<<<< HEAD
            if STATE["fail_left"] > 0 and "/ack" not in self.path:
=======
            if STATE["fail_left"] > 0:
>>>>>>> origin/main
                STATE["fail_left"] -= 1
                print(f"[#{n}] (simulasi gagal) -> 503, sisa {STATE['fail_left']}")
                return self._reply(503, {"error": "simulated outage"})

<<<<<<< HEAD
        if self.path == "/api/v1/events":
            return self._handle_events(n)
        if self.path == "/api/v1/devices/heartbeat":
            return self._handle_heartbeat(n)
        if self.path.startswith("/api/v1/commands/") and self.path.endswith("/ack"):
            return self._handle_ack(n)
        return self._reply(404, {"error": "not found"})

    def do_GET(self):
        if self.path == "/api/v1/commands/poll":
            return self._handle_poll()
        return self._reply(404, {"error": "not found"})

    def _handle_events(self, n):
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length))
            device, events, epoch = payload["device_id"], payload["events"], payload.get("log_epoch", 0)
        except Exception as exc:
            return self._reply(400, {"error": f"bad payload: {exc}"})

        if not self._check_auth(device):
            return

        new, dup = 0, 0
        print(f"\n[#{n}] EVENTS {device} fw={payload.get('firmware_version')} epoch={epoch} "
=======
        if self.path != "/api/v1/events":
            return self._reply(404, {"error": "not found"})

        if self.headers.get("Authorization", "") != f"Bearer {STATE['token']}":
            print(f"[#{n}] TOKEN DITOLAK dari {self.client_address[0]}")
            return self._reply(401, {"error": "invalid device token"})

        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length))
            device = payload["device_id"]
            events = payload["events"]
            epoch = payload.get("log_epoch", 0)
        except Exception as exc:
            print(f"[#{n}] payload tidak valid: {exc}")
            return self._reply(400, {"error": "bad payload"})

        if self.headers.get("X-Device-Id") != device:
            return self._reply(403, {"error": "device id mismatch"})

        new, dup = 0, 0
        print(f"\n[#{n}] {device} fw={payload.get('firmware_version')} epoch={epoch} "
>>>>>>> origin/main
              f"missed={payload.get('missed_events', 0)} sent_at={fmt_ts(payload.get('sent_at'))}")
        for e in events:
            key = (device, epoch, e["boot_id"], e["sequence"])
            with LOCK:
                is_dup = key in SEEN
                SEEN.add(key)
                STATE["dups" if is_dup else "events"] += 1
            new, dup = (new, dup + 1) if is_dup else (new + 1, dup)
            print(f"   {'DUP' if is_dup else 'NEW'} seq={e['sequence']:<4} boot={e['boot_id']:<3} "
                  f"{fmt_ts(e.get('timestamp'))} [{e['time_source']:<9}] "
                  f"{e['type']:<21} detail={e['detail']:<3} \"{e['message']}\"")
<<<<<<< HEAD
        print(f"   -> baru={new} duplikat={dup} | total unik={STATE['events']} total duplikat={STATE['dups']}")
        last = events[-1]["sequence"] if events else 0
        return self._reply(201, {"accepted": new, "duplicates": dup, "last_sequence": last})

    def _handle_heartbeat(self, n):
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length)) if length else {}
        except Exception as exc:
            return self._reply(400, {"error": f"bad payload: {exc}"})

        device = self.headers.get("X-Device-Id", "?")
        if not self._check_auth():
            return
        print(f"[#{n}] HEARTBEAT {device} fw={payload.get('firmware_version')} "
              f"uptime_ms={payload.get('uptime_ms')} free_heap={payload.get('free_heap_bytes')}")
        return self._reply(200, {"status": "ok", "server_time": int(time.time())})

    def _handle_poll(self):
        device = self.headers.get("X-Device-Id", "?")
        if not self._check_auth():
            return

        now = time.time()
        with LOCK:
            pending = [c for c in COMMANDS if c["device_id"] == device and c["status"] == "pending"]
            wire = []
            for c in pending:
                c["status"] = "acknowledged"
                sig = sign_command(STATE["hmac_secret"], c["id"], c["command_type"],
                                    c["issued_at"], c["expires_at"], c["payload"])
                wire.append({
                    "id": c["id"], "command_type": c["command_type"], "payload": c["payload"],
                    "issued_at": c["issued_at"], "expires_at": c["expires_at"], "signature": sig,
                })
        if wire:
            print(f"[POLL] {device} mengambil {len(wire)} command: "
                  f"{', '.join(c['command_type'] for c in wire)}")
        return self._reply(200, {"commands": wire})

    def _handle_ack(self, n):
        command_id = self.path.split("/")[-2]
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length))
        except Exception as exc:
            return self._reply(400, {"error": f"bad payload: {exc}"})

        device = self.headers.get("X-Device-Id", "?")
        if not self._check_auth():
            return

        with LOCK:
            cmd = next((c for c in COMMANDS if c["id"] == command_id), None)
            if cmd is None or cmd["device_id"] != device:
                return self._reply(404, {"error": "command not found"})
            cmd["status"] = payload.get("status", "failed")
            cmd["result_detail"] = payload.get("detail", "")

        print(f"[#{n}] ACK {device} command={command_id[:8]}... -> "
              f"{cmd['status']} ({cmd['result_detail']})")
        return self._reply(200, {"id": cmd["id"], "status": cmd["status"]})


class AdminHandler(BaseHTTPRequestHandler):
    """Port ADMIN terpisah (default 8001): simulasi tombol dashboard untuk
    mengantre command baru TANPA perlu merestart server utama."""

    def log_message(self, *args):
        pass

    def do_POST(self):
        if self.path != "/enqueue":
            return self._reply(404, {"error": "not found"})
        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = json.loads(self.rfile.read(length))
            device_id = body["device_id"]
            command_type = body["command_type"]
            payload = body.get("payload", {})
            ttl = int(body.get("ttl_seconds", 120))
        except Exception as exc:
            return self._reply(400, {"error": str(exc)})

        now = int(time.time())
        cmd = {
            "id": str(uuid.uuid4()), "device_id": device_id, "command_type": command_type,
            "payload": payload, "status": "pending", "issued_at": now, "expires_at": now + ttl,
            "result_detail": "",
        }
        with LOCK:
            COMMANDS.append(cmd)
        print(f"\n[ADMIN] Command baru diantre: {command_type} -> {device_id} (payload={payload}, ttl={ttl}s)")
        return self._reply(201, cmd)

    def do_GET(self):
        if self.path == "/commands":
            with LOCK:
                return self._reply(200, {"commands": COMMANDS})
        return self._reply(404, {"error": "not found"})

    def _reply(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def enqueue_via_admin(admin_port, device, command_type, slot, ttl):
    payload = {"slot": slot} if command_type in ("ALLOW_SLOT", "REVOKE_SLOT") else {}
    body = json.dumps({
        "device_id": device, "command_type": command_type, "payload": payload, "ttl_seconds": ttl,
    }).encode()
    req = urllib.request.Request(
        f"http://127.0.0.1:{admin_port}/enqueue", data=body,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req) as resp:
        result = json.loads(resp.read())
    print(f"Command diantre: {result['command_type']} -> {result['device_id']} (id={result['id']})")
    print(f"Device akan mengambilnya pada poll berikutnya (lihat COMMAND_POLL_INTERVAL_MS di config.h).")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8000, help="Port utama (events/heartbeat/commands)")
    ap.add_argument("--admin-port", type=int, default=8001, help="Port admin lokal (enqueue command)")
    ap.add_argument("--token", default="dev-token-123", help="Bearer token yang dianggap sah")
    ap.add_argument("--hmac-secret", default="dev-hmac-456", help="Kunci HMAC untuk menandatangani command")
    ap.add_argument("--fail-next", type=int, default=0, help="balas 503 untuk N request pertama (uji retry)")
    # Mode "klien admin": kirim satu command ke server yang SUDAH BERJALAN, lalu keluar.
    ap.add_argument("--enqueue", choices=["REMOTE_UNLOCK", "ALLOW_SLOT", "REVOKE_SLOT"],
                     help="Kirim satu command ke server yang sudah jalan (lihat --admin-port), lalu keluar")
    ap.add_argument("--device", default="DOOR-01", help="device_id target untuk --enqueue")
    ap.add_argument("--slot", type=int, default=0, help="slot untuk ALLOW_SLOT/REVOKE_SLOT")
    ap.add_argument("--ttl", type=int, default=120, help="masa berlaku command (detik) untuk --enqueue")
    args = ap.parse_args()

    if args.enqueue:
        enqueue_via_admin(args.admin_port, args.device, args.enqueue, args.slot, args.ttl)
        return

    STATE["token"], STATE["hmac_secret"], STATE["fail_left"] = args.token, args.hmac_secret, args.fail_next

    admin_server = ThreadingHTTPServer(("127.0.0.1", args.admin_port), AdminHandler)
    threading.Thread(target=admin_server.serve_forever, daemon=True).start()

    print(f"Mock backend (Phase 3+4) di 0.0.0.0:{args.port}  (Ctrl+C untuk berhenti)")
    print(f"Admin API (enqueue command) di 127.0.0.1:{args.admin_port}")
    print(f"Contoh dari terminal lain: python {__file__} --enqueue REMOTE_UNLOCK --device {args.device}")
=======
        with LOCK:
            print(f"   -> baru={new} duplikat={dup} | total unik={STATE['events']} total duplikat={STATE['dups']}")
        last = events[-1]["sequence"] if events else 0
        return self._reply(201, {"accepted": new, "duplicates": dup, "last_sequence": last})


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--token", required=True, help="Bearer token yang dianggap sah")
    ap.add_argument("--fail-next", type=int, default=0, help="balas 503 untuk N request pertama (uji retry/backoff)")
    args = ap.parse_args()
    STATE["token"], STATE["fail_left"] = args.token, args.fail_next
    print(f"Mock backend di 0.0.0.0:{args.port}  (Ctrl+C untuk berhenti)")
>>>>>>> origin/main
    ThreadingHTTPServer(("0.0.0.0", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
