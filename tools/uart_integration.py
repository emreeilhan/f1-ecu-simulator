#!/usr/bin/env python3
"""Exercise an already-flashed UART v1 image; does not flash/reset the board."""
import argparse
import datetime
import hashlib
import json
import pathlib
import time


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ (0x07 if crc & 0x80 else 0)) & 0xFF
    return crc


def encode(identifier, value, counter, corrupt=False):
    raw = value & 0xFFFF
    payload = bytes((raw >> 8, raw & 0xFF, counter))
    crc = crc8(bytes((1, identifier >> 8, identifier & 0xFF)) + payload)
    if corrupt:
        crc ^= 1
    return f"{identifier:03X}#{(payload + bytes((crc,))).hex().upper()}\n".encode()


class Exercise:
    def __init__(self, port, trace):
        self.port = port
        self.trace = trace
        self.latest = None
        self.passed = []

    def wait(self, predicate, timeout=4):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = self.port.read_until(b"\n", size=1024)
            if not raw or not raw.endswith(b"\n"):
                continue
            try:
                record = json.loads(raw)
            except (ValueError, UnicodeError):
                continue  # ROM/boot messages are not application evidence.
            if not isinstance(record, dict) or "event" not in record:
                continue
            self.trace.write(json.dumps(record, separators=(",", ":")) + "\n")
            self.trace.flush()
            self.latest = record
            if predicate(record):
                return record
        raise TimeoutError("No matching application response before deadline")

    def send(self, sensor, value, counter=None, result="accepted", corrupt=False):
        identifier = 0x100 + sensor
        if counter is None:
            counter = (self.latest["counter"][sensor] + 1) & 0xFF
        self.port.write(encode(identifier, value, counter, corrupt))
        return self.wait(lambda r: r["event"] == "frame" and
                         r["id"] == identifier and r["result"] == result)

    def passed_check(self, name):
        self.passed.append(name)
        print(f"PASS: {name}", flush=True)

    def nominal(self):
        for sensor, value in enumerate((3000, 750, 905)):
            self.send(sensor, value)
        r = self.latest
        assert r["present"] == 7 and r["active"] == 0 and r["command"] == 750
        self.passed_check("nominal three-sensor command")

    def run(self, exhaustive=False):
        self.wait(lambda r: r["event"] in ("tick", "startup", "frame"))
        self.nominal()
        stale = self.wait(lambda r: (r["active"] & 56) == 56)
        assert stale["command"] == 0
        assert all(stale["t"] - t >= 500 for t in stale["received"])
        self.passed_check("communication loss: all stale at age >=500ms")
        self.nominal()
        before = dict(self.latest)
        bad = self.send(1, 800, corrupt=True, result="crc")
        assert bad["command"] == 0 and bad["active"] & 128
        for key in ("counter", "value", "received"):
            assert bad[key][1] == before[key][1]
        self.passed_check("CRC rejection preserves value/counter/receive time")
        good = self.send(1, 750)
        assert good["active"] == 0 and good["command"] == 750
        assert good["history"] & 128
        self.passed_check("valid frame recovers active fault, retains history")
        before = dict(self.latest)
        duplicate = self.send(1, 750, counter=before["counter"][1], result="duplicate")
        assert duplicate["command"] == 0
        for key in ("counter", "value", "received"):
            assert duplicate[key][1] == before[key][1]
        self.passed_check("duplicate rejection preserves measurement")
        self.send(1, 750)
        gap = self.send(1, 750, counter=(self.latest["counter"][1] + 3) & 0xFF,
                        result="accepted-gap")
        assert gap["active"] == 0 and gap["history"] & 4096
        self.passed_check("forward counter gap accepted as diagnostic")
        before = dict(self.latest)
        bad_range = self.send(1, 1001, result="range")
        assert bad_range["received"][1] == before["received"][1]
        assert bad_range["value"][1] == before["value"][1]
        self.send(1, 750)
        self.passed_check("valid-CRC range failure and recovery")
        before = dict(self.latest)
        self.port.write(b"x" * 80 + b"100#00000000\n")
        overflow = self.wait(lambda r: r["event"] == "transport" and
                              r["result"] == "line-overflow")
        assert overflow["received"] == before["received"]
        self.send(0, 3000)
        self.passed_check("bounded line overflow discards through LF and recovers")
        if exhaustive:
            # Only sequential frames, never an ambiguous jump to 255.
            while self.latest["counter"][1] != 255:
                self.send(1, 750)
            wrapped = self.send(1, 750, counter=0)
            assert wrapped["counter"][1] == 0
            self.passed_check("actual sequential 255-to-0 counter wrap")
            self.nominal()
        self.wait(lambda r: r["event"] == "tick")
        assert self.latest["tx_dropped"] == 0
        self.passed_check("no telemetry drops during this exercise")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--output", type=pathlib.Path, default=pathlib.Path("build/esp32-trace.jsonl"))
    p.add_argument("--exhaustive", action="store_true", help="also walk sequence to 255->0")
    p.add_argument("--firmware-image", type=pathlib.Path)
    args = p.parse_args()
    assert crc8(b"123456789") == 0xF4
    try:
        import serial
    except ImportError as exc:
        raise SystemExit("pyserial is required (included in the ESP-IDF Python environment)") from exc
    args.output.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = args.port, args.baud, 0.1
    port.dtr = port.rts = False
    status = {"completed_at_utc": None, "checks_passed": [], "success": False,
              "baud": args.baud, "limits": ["Observed UART exercise, not timing/safety certification",
              "Device receive time is complete-line observation, not physical FIFO arrival"]}
    if args.firmware_image:
        status["local_firmware_image_sha256"] = hashlib.sha256(args.firmware_image.read_bytes()).hexdigest()
    try:
        port.open()
        with args.output.open("w") as trace:
            run = Exercise(port, trace)
            try:
                run.run(args.exhaustive)
                status["success"] = True
            finally:
                status["checks_passed"] = run.passed
    except Exception as exc:
        status["error"] = str(exc)
        raise
    finally:
        port.close()
        status["completed_at_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        args.output.with_suffix(".summary.json").write_text(json.dumps(status, indent=2) + "\n")


if __name__ == "__main__":
    main()
