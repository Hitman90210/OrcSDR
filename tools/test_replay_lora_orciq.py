import hashlib
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).parent))
import replay_lora_orciq


class FakeSerial:
    def __init__(self):
        self.stream = bytearray()
        self.commands = []
        self.payload = bytearray()

    def _line(self, value):
        self.stream.extend(value.encode("ascii") + b"\n")

    def write(self, value):
        if value.startswith(b"RTL_"):
            command = value.decode("ascii").strip()
            self.commands.append(command)
            if command.startswith("RTL_LORA_REPLAY_BEGIN "):
                self._line("RTL_LORA_REPLAY_READY chunk=3 bytes=6")
            elif command.startswith("RTL_LORA_REPLAY_CHUNK "):
                self._line("RTL_LORA_REPLAY_DATA")
            return len(value)
        self.payload.extend(value)
        if len(self.payload) == 6:
            self._line("RTL_LORA_REPLAY_QUEUED bytes=6")
        else:
            self._line(f"RTL_LORA_REPLAY_ACK bytes={len(self.payload)}")
        return len(value)

    def flush(self):
        pass

    def readline(self):
        newline = self.stream.find(b"\n")
        if newline < 0:
            return b""
        value = bytes(self.stream[: newline + 1])
        del self.stream[: newline + 1]
        return value


class ReplayUploadTests(unittest.TestCase):
    def test_upload_is_chunked_and_content_bound(self):
        connection = FakeSerial()
        replay_lora_orciq._upload_iq(
            connection, b"abcdef", rate=960000, frequency_hz=906875000,
            sf=11, bandwidth_hz=250000,
        )

        digest = hashlib.sha256(b"abcdef").hexdigest()
        self.assertEqual(
            connection.commands[0],
            f"RTL_LORA_REPLAY_BEGIN 6 {digest} 960000 906875000 11 250000",
        )
        self.assertEqual(connection.payload, b"abcdef")
        self.assertEqual(connection.commands[1:], [
            "RTL_LORA_REPLAY_CHUNK 3", "RTL_LORA_REPLAY_CHUNK 3",
        ])

    def test_no_preamble_result_has_no_trace(self):
        self.assertEqual(
            replay_lora_orciq._read_traces(None, "RTL_LORA_NATIVE_DONE preambles=0"),
            [],
        )

    def test_trace_reader_keeps_symbol_alternates(self):
        connection = FakeSerial()
        connection._line("RTL_LORA_NATIVE_TRACE sequence=1")
        connection._line("RTL_LORA_NATIVE_ALTERNATES sequence=1 count=1 values=8:12:1.025")
        connection._line("RTL_LORA_NATIVE_SYMBOLS sequence=1 count=1 values=11")

        self.assertEqual(
            replay_lora_orciq._read_traces(
                connection, "RTL_LORA_NATIVE_DONE preambles=1"
            ),
            [
                "RTL_LORA_NATIVE_TRACE sequence=1",
                "RTL_LORA_NATIVE_ALTERNATES sequence=1 count=1 values=8:12:1.025",
                "RTL_LORA_NATIVE_SYMBOLS sequence=1 count=1 values=11",
            ],
        )

    def test_symbol_difference_reports_first_and_histogram(self):
        self.assertEqual(
            replay_lora_orciq._symbol_difference([1, 2, 3], [1, 3, 2]),
            {"first": 1, "indices": [1, 2], "different": 2, "largest": 1,
             "histogram": {"-1": 1, "0": 1, "+1": 1}},
        )

    def test_alternate_coverage_reports_correct_runner_up_symbols(self):
        line = (
            "RTL_LORA_NATIVE_ALTERNATES sequence=4 count=3 "
            "values=8:12:1.025,9:22:1.400,10:31:1.010"
        )

        alternates = replay_lora_orciq._parse_symbol_alternates(line)

        self.assertEqual(alternates, {
            8: {"symbol": 12, "ratio": 1.025},
            9: {"symbol": 22, "ratio": 1.4},
            10: {"symbol": 31, "ratio": 1.01},
        })
        self.assertEqual(
            replay_lora_orciq._alternate_coverage(
                [1] * 8 + [12, 20, 30],
                [1] * 8 + [11, 20, 32],
                alternates,
            ),
            {"error_indices": [8, 10], "covered_indices": [8], "covered": 1,
             "errors": 2},
        )


if __name__ == "__main__":
    unittest.main()
