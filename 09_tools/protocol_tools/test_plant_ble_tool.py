#!/usr/bin/env python3

import struct
import unittest

import plant_ble_tool as protocol


class ProtocolCodecTest(unittest.TestCase):
    def test_crc32_known_vector(self) -> None:
        self.assertEqual(protocol.crc32(b"123456789"), 0xCBF43926)

    def test_encode_ping(self) -> None:
        frame = protocol.encode_request(protocol.COMMANDS["ping"], 0x1234)
        self.assertEqual(frame[:8], bytes((0xA5, 1, 1, 0, 0x34, 0x12, 0, 0)))
        self.assertEqual(struct.unpack_from("<I", frame, 8)[0], protocol.crc32(frame[:8]))

    def test_decode_response(self) -> None:
        payload = struct.pack("<BBBBBBII", 2, 0, 1, 0, 3, 0, 496, 0x00010000)
        body = struct.pack("<BBBBHH", 0xA5, 1, 0x80, 0, 77, len(payload)) + payload
        response = protocol.decode_response(body + struct.pack("<I", protocol.crc32(body)))
        self.assertTrue(response.ok)
        self.assertEqual(response.request_id, 77)
        self.assertEqual(response.ota_received_bytes, 496)

    def test_rejects_corrupt_response(self) -> None:
        payload = bytes(14)
        body = struct.pack("<BBBBHH", 0xA5, 1, 0x80, 0, 1, len(payload)) + payload
        with self.assertRaisesRegex(ValueError, "CRC32"):
            protocol.decode_response(body + bytes(4))


if __name__ == "__main__":
    unittest.main()
