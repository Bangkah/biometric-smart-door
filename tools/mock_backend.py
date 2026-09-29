#!/usr/bin/env python3
"""
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
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

WIB = timezone(timedelta(hours=7))
LOCK = threading.Lock()
SEEN = set()            # kunci dedup: (device_id, log_epoch, boot_id, sequence)
STATE = {"requests": 0, "fail_left": 0, "token": "", "events": 0, "dups": 0}


def fmt_ts(ts):
    if ts is None:
        return "         (tanpa waktu)        "
    return datetime.fromtimestamp(ts, tz=timezone.utc).astimezone(WIB).strftime("%Y-%m-%d %H:%M:%S WIB")


class Handler(BaseHTTPRequestHandler):
    def _reply(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):   # senyapkan log akses default
        pass

    def do_POST(self):
        with LOCK:
            STATE["requests"] += 1
            n = STATE["requests"]
            if STATE["fail_left"] > 0:
                STATE["fail_left"] -= 1
                print(f"[#{n}] (simulasi gagal) -> 503, sisa {STATE['fail_left']}")
                return self._reply(503, {"error": "simulated outage"})

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
    ThreadingHTTPServer(("0.0.0.0", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
