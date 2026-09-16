"""Read-only live PCM protocol probe; never tunes, mutes, flashes or resets Tab5."""
import argparse
import base64
import hashlib
import json
import os
import socket
import struct
import time


def read_exact(stream, count):
    data = bytearray()
    while len(data) < count:
        part = stream.read(count - len(data))
        if not part:
            raise RuntimeError("Audio connection closed")
        data.extend(part)
    return bytes(data)


def probe(host, seconds):
    key = base64.b64encode(os.urandom(16)).decode("ascii")
    with socket.create_connection((host, 80), timeout=5) as connection:
        connection.settimeout(5)
        connection.sendall((f"GET /api/audio/stream HTTP/1.1\r\nHost: {host}\r\n"
                            f"Origin: http://{host}\r\nUpgrade: websocket\r\n"
                            "Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                            f"Sec-WebSocket-Key: {key}\r\n\r\n").encode("ascii"))
        with connection.makefile("rb") as stream:
            status = stream.readline(256).decode("ascii").strip()
            if not status.startswith("HTTP/1.1 101 "):
                raise RuntimeError(f"WebSocket upgrade rejected: {status}")
            headers = {}
            total = 0
            while True:
                line = stream.readline(1024)
                total += len(line)
                if not line or total > 4096:
                    raise RuntimeError("Invalid handshake headers")
                if line == b"\r\n":
                    break
                name, value = line.decode("ascii").split(":", 1)
                headers[name.lower()] = value.strip()
            expected_accept = base64.b64encode(hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")
            ).digest()).decode("ascii")
            if headers.get("sec-websocket-accept") != expected_accept:
                raise RuntimeError("Invalid WebSocket accept")
            started = time.monotonic()
            stats = dict(packets=0, samples=0, gaps=0, generations=0, nonzero_samples=0)
            generation = expected_position = None
            while time.monotonic() - started < seconds:
                a, b = read_exact(stream, 2)
                if a != 0x82 or b & 0x80:
                    raise RuntimeError(f"Unexpected WebSocket frame: {a:#x}/{b:#x}")
                length = b & 0x7f
                if length == 126:
                    length = struct.unpack("!H", read_exact(stream, 2))[0]
                if length == 127 or not 34 <= length <= 1952:
                    raise RuntimeError("Invalid frame length")
                packet = read_exact(stream, length)
                magic, version, size, epoch, position, rate, frames, channels, fmt, flags = \
                    struct.unpack_from("<4sHHIQIHBBI", packet)
                if (magic, version, size, rate, channels, fmt) != (b"ORCA", 1, 32, 48000, 1, 1):
                    raise RuntimeError("Invalid PCM header")
                if not 1 <= frames <= 960 or length != 32 + frames * 2 or flags > 1:
                    raise RuntimeError("Invalid PCM payload")
                if generation is not None and epoch != generation:
                    stats["generations"] += 1
                elif expected_position is not None and expected_position != position:
                    stats["gaps"] += 1
                generation, expected_position = epoch, position + frames
                stats["packets"] += 1
                stats["samples"] += frames
                stats["nonzero_samples"] += sum(value != 0 for (value,) in struct.iter_unpack("<h", packet[32:]))
            stats["elapsed_seconds"] = round(time.monotonic() - started, 3)
            stats["samples_per_second"] = round(stats["samples"] / stats["elapsed_seconds"], 1)
            return stats


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--seconds", type=int, default=10, choices=range(1, 601), metavar="1..600")
    arguments = parser.parse_args()
    print(json.dumps(probe(arguments.host, arguments.seconds), indent=2))
