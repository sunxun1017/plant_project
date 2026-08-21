#!/usr/bin/env python3
"""Plant V1 BLE protocol smoke-test and OTA utility.

The codec uses only the Python standard library. Hardware commands additionally
require ``bleak`` (``python3 -m pip install bleak``).
"""

from __future__ import annotations

import argparse
import asyncio
import binascii
from dataclasses import dataclass
from pathlib import Path
import struct
from typing import Any


DEVICE_NAME = "Plant-V1-C3"
COMMAND_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
RESPONSE_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"
MAGIC = 0xA5
VERSION = 1
RESPONSE_TYPE = 0x80
MAX_OTA_CHUNK = 496

COMMANDS = {
    "ping": 0x01,
    "state": 0x02,
    "behavior": 0x03,
    "stop": 0x04,
    "begin-ota": 0x10,
    "ota-chunk": 0x11,
    "finish-ota": 0x12,
    "cancel-ota": 0x13,
}
BEHAVIORS = {"wake-up": 0, "happy": 1, "attention": 2, "calm": 3, "sleep": 4, "error": 5}
ERROR_NAMES = (
    "None",
    "Busy",
    "InvalidArgument",
    "InvalidState",
    "Unsupported",
    "Timeout",
    "MotionFailure",
    "LightingFailure",
    "HapticFailure",
    "StorageFailure",
    "ProtocolFailure",
    "OtaFailure",
    "InternalFailure",
)
DEVICE_STATES = ("Booting", "Idle", "Interacting", "Sleeping", "Fault", "Updating")
POWER_MODES = ("Active", "LightSleep", "DeepSleep")
BEHAVIOR_NAMES = ("WakeUp", "Happy", "Attention", "Calm", "Sleep", "Error")
OTA_STATES = ("Idle", "Receiving", "Verifying", "ReadyToReboot", "Failed")


@dataclass(frozen=True)
class Response:
    request_id: int
    request_type: int
    error: int
    device_state: int
    power_mode: int
    behavior: int
    ota_state: int
    ota_received_bytes: int
    firmware_version: int

    @property
    def ok(self) -> bool:
        return self.error == 0


def crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def encode_request(command_type: int, request_id: int, payload: bytes = b"") -> bytes:
    if not 0 <= command_type <= 0xFF:
        raise ValueError("command type must fit in one byte")
    if not 0 <= request_id <= 0xFFFF:
        raise ValueError("request id must fit in two bytes")
    if len(payload) > 500:
        raise ValueError("payload exceeds the 500-byte protocol limit")
    header = struct.pack("<BBBBHH", MAGIC, VERSION, command_type, 0, request_id, len(payload))
    body = header + payload
    return body + struct.pack("<I", crc32(body))


def decode_response(frame: bytes) -> Response:
    if len(frame) < 12:
        raise ValueError("response frame is too short")
    magic, version, frame_type, flags, request_id, payload_size = struct.unpack_from(
        "<BBBBHH", frame
    )
    if magic != MAGIC or version != VERSION or frame_type != RESPONSE_TYPE or flags != 0:
        raise ValueError("response header is invalid")
    if payload_size != 14 or len(frame) != 8 + payload_size + 4:
        raise ValueError("response length is invalid")
    expected_crc = struct.unpack_from("<I", frame, 8 + payload_size)[0]
    if crc32(frame[: 8 + payload_size]) != expected_crc:
        raise ValueError("response CRC32 mismatch")
    fields = struct.unpack_from("<BBBBBBII", frame, 8)
    return Response(request_id, *fields)


def enum_name(names: tuple[str, ...], value: int) -> str:
    return names[value] if value < len(names) else f"Unknown({value})"


def describe(response: Response) -> str:
    return (
        f"request={response.request_id} type=0x{response.request_type:02x} "
        f"status={enum_name(ERROR_NAMES, response.error)} "
        f"device={enum_name(DEVICE_STATES, response.device_state)} "
        f"power={enum_name(POWER_MODES, response.power_mode)} "
        f"behavior={enum_name(BEHAVIOR_NAMES, response.behavior)} "
        f"ota={enum_name(OTA_STATES, response.ota_state)} "
        f"received={response.ota_received_bytes} "
        f"firmware=0x{response.firmware_version:08x}"
    )


class BleSession:
    def __init__(self, client: Any, timeout: float) -> None:
        self.client = client
        self.timeout = timeout
        self.next_request_id = 1
        self.notifications: asyncio.Queue[bytes] = asyncio.Queue()

    def on_notification(self, _sender: Any, data: bytearray) -> None:
        self.notifications.put_nowait(bytes(data))

    async def request(self, command_type: int, payload: bytes = b"") -> Response:
        request_id = self.next_request_id
        self.next_request_id = 1 if request_id == 0xFFFF else request_id + 1
        await self.client.write_gatt_char(
            COMMAND_UUID, encode_request(command_type, request_id, payload), response=True
        )
        while True:
            frame = await asyncio.wait_for(self.notifications.get(), timeout=self.timeout)
            response = decode_response(frame)
            if response.request_id == request_id:
                return response


async def require_success(session: BleSession, command: str, payload: bytes = b"") -> Response:
    response = await session.request(COMMANDS[command], payload)
    print(describe(response))
    if not response.ok:
        raise RuntimeError(f"device rejected {command}: {enum_name(ERROR_NAMES, response.error)}")
    return response


async def wait_for_ota_receiving(session: BleSession, timeout: float = 10.0) -> None:
    deadline = asyncio.get_running_loop().time() + timeout
    while asyncio.get_running_loop().time() < deadline:
        response = await require_success(session, "state")
        if response.ota_state == 1:
            return
        await asyncio.sleep(0.2)
    raise TimeoutError("device did not enter OTA Receiving state")


async def transfer_ota(session: BleSession, args: argparse.Namespace) -> None:
    image = args.image.read_bytes()
    if not image:
        raise ValueError("OTA image is empty")
    print(
        f"image={args.image} size={len(image)} metadata-version=0x{args.version:08x}; "
        "the compiled BoardConfig firmware version must match"
    )
    metadata = struct.pack(
        "<IIIIB", args.product_id, args.hardware_revision, args.version, len(image), 1
    )
    await require_success(session, "begin-ota", metadata)
    await wait_for_ota_receiving(session)

    report_step = max(len(image) // 100, MAX_OTA_CHUNK)
    next_report = report_step
    for offset in range(0, len(image), MAX_OTA_CHUNK):
        chunk = image[offset : offset + MAX_OTA_CHUNK]
        response = await session.request(
            COMMANDS["ota-chunk"], struct.pack("<I", offset) + chunk
        )
        if not response.ok:
            print(describe(response))
            raise RuntimeError(
                f"device rejected ota-chunk at {offset}: "
                f"{enum_name(ERROR_NAMES, response.error)}"
            )
        if (
            response.ota_received_bytes >= next_report
            or response.ota_received_bytes == len(image)
        ):
            print(f"progress={response.ota_received_bytes}/{len(image)}")
            next_report = response.ota_received_bytes + report_step
    await require_success(session, "finish-ota")
    print("OTA image activated; device should restart after the final response.")


async def run_hardware_command(args: argparse.Namespace) -> None:
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError as error:
        raise SystemExit("hardware commands require bleak: python3 -m pip install bleak") from error

    target: Any = args.address
    if target is None:
        print(f"scanning for {args.name} ...")
        target = await BleakScanner.find_device_by_name(args.name, timeout=args.scan_timeout)
        if target is None:
            raise TimeoutError(f"BLE device {args.name!r} was not found")

    async with BleakClient(target, timeout=args.timeout) as client:
        session = BleSession(client, args.timeout)
        await client.start_notify(RESPONSE_UUID, session.on_notification)
        if args.command in ("ping", "state", "stop", "cancel-ota"):
            await require_success(session, args.command)
        elif args.command == "behavior":
            await require_success(session, "behavior", bytes((BEHAVIORS[args.behavior],)))
        elif args.command == "ota":
            await transfer_ota(session, args)


def integer(value: str) -> int:
    return int(value, 0)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--address", help="BLE address; omit to scan by name")
    parser.add_argument("--name", default=DEVICE_NAME, help="device name used while scanning")
    parser.add_argument("--timeout", type=float, default=5.0, help="response timeout in seconds")
    parser.add_argument("--scan-timeout", type=float, default=10.0)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ("ping", "state", "stop", "cancel-ota"):
        subparsers.add_parser(name)

    behavior = subparsers.add_parser("behavior")
    behavior.add_argument("behavior", choices=tuple(BEHAVIORS))

    ota = subparsers.add_parser("ota")
    ota.add_argument("image", type=Path)
    ota.add_argument("--version", type=integer, required=True)
    ota.add_argument("--product-id", type=integer, default=0x504C414E)
    ota.add_argument("--hardware-revision", type=integer, default=1)
    return parser


def main() -> None:
    asyncio.run(run_hardware_command(build_parser().parse_args()))


if __name__ == "__main__":
    main()
