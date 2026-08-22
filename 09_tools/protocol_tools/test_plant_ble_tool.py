#!/usr/bin/env python3

import argparse
import asyncio
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

    def test_encode_v2_ping(self) -> None:
        frame = protocol.encode_request(
            protocol.COMMANDS["ping"], 0x1234, version=protocol.VERSION_V2
        )
        self.assertEqual(frame[:8], bytes((0xA5, 2, 1, 0, 0x34, 0x12, 0, 0)))

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

    def test_decode_v2_telemetry_response(self) -> None:
        base = struct.pack("<BBBBBBII", 2, 0, 1, 0, 3, 0, 496, 0x00020000)
        telemetry = struct.pack(
            "<IHHBBBBHHHBHHBhHHBHHBBBBB",
            0x3F,
            625,
            675,
            1,
            0,
            0x03,
            2,
            720,
            85,
            24,
            2,
            810,
            301,
            4,
            2350,
            568,
            602,
            1,
            742,
            3890,
            2,
            0,
            0x01,
            0x03,
            0,
        )
        payload = base + telemetry
        self.assertEqual(len(payload), 54)
        body = struct.pack("<BBBBHH", 0xA5, 2, 0x80, 0, 78, len(payload)) + payload
        response = protocol.decode_response(body + struct.pack("<I", protocol.crc32(body)))

        self.assertEqual(response.actual_position, 625)
        self.assertEqual(response.temperature_centi_c, 2350)
        self.assertEqual(response.humidity_tenths_percent, 568)
        self.assertEqual(response.battery_level_per_mille, 742)
        self.assertTrue(response.ble_flags & 0x01)
        self.assertTrue(response.ble_flags & 0x02)
        self.assertIn("height=625/1000", protocol.describe(response))

    def test_bluetooth_address_validation(self) -> None:
        self.assertEqual(protocol.bluetooth_address("44:b1:76:07:9f:26"), "44:B1:76:07:9F:26")
        with self.assertRaises(argparse.ArgumentTypeError):
            protocol.bluetooth_address("Plant-V2-C3")

    def test_bluez_device_path(self) -> None:
        self.assertEqual(
            protocol.bluez_device_path("44:B1:76:07:9F:26", "hci0"),
            "/org/bluez/hci0/dev_44_B1_76_07_9F_26",
        )

    def test_address_resolution_prefers_discovered_device(self) -> None:
        discovered = object()

        class Scanner:
            @staticmethod
            async def find_device_by_address(_address: str, timeout: float) -> object:
                self.assertEqual(timeout, 7.0)
                return discovered

        args = argparse.Namespace(
            address="44:B1:76:07:9F:26",
            name="Plant-V2-C3",
            scan_timeout=7.0,
            bluez_adapter="hci0",
        )
        target = asyncio.run(protocol.resolve_ble_target(args, Scanner))
        self.assertIs(target, discovered)

    def test_address_resolution_falls_back_to_known_bluez_device(self) -> None:
        class Scanner:
            @staticmethod
            async def find_device_by_address(_address: str, timeout: float) -> None:
                self.assertEqual(timeout, 7.0)
                return None

        args = argparse.Namespace(
            address="44:B1:76:07:9F:26",
            name="Plant-V2-C3",
            scan_timeout=7.0,
            bluez_adapter="hci1",
        )
        known = object()
        factory_calls = []

        def factory(address: str, adapter: str) -> object:
            factory_calls.append((address, adapter))
            return known

        original_platform = protocol.sys.platform
        protocol.sys.platform = "linux"
        try:
            target = asyncio.run(protocol.resolve_ble_target(args, Scanner, factory))
        finally:
            protocol.sys.platform = original_platform

        self.assertIs(target, known)
        self.assertEqual(factory_calls, [("44:B1:76:07:9F:26", "hci1")])

    def test_connection_timeout_message_preserves_bond(self) -> None:
        message = protocol.connection_failure_message(
            TimeoutError(), "44:B1:76:07:9F:26"
        )
        self.assertIn("bond is not deleted", message)
        self.assertIn("timed out", message)

        backend_timeout = type("TimeoutError", (Exception,), {})()
        backend_message = protocol.connection_failure_message(
            backend_timeout, "44:B1:76:07:9F:26"
        )
        self.assertIn("bond is not deleted", backend_message)

    def test_notification_subscription_retries_during_security_restore(self) -> None:
        class Client:
            is_connected = True

            def __init__(self) -> None:
                self.attempts = 0

            async def start_notify(self, _uuid: str, _callback: object) -> None:
                self.attempts += 1
                if self.attempts < 3:
                    raise RuntimeError("link security is not ready")

        client = Client()
        asyncio.run(
            protocol.start_notify_with_retry(
                client,
                lambda _sender, _data: None,
                attempts=3,
                retry_delay_s=0,
            )
        )
        self.assertEqual(client.attempts, 3)


if __name__ == "__main__":
    unittest.main()
