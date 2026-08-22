#!/usr/bin/env python3
"""Plant V1/V2 BLE protocol smoke-test, telemetry, and OTA utility.

The codec uses only the Python standard library. Hardware commands additionally
require ``bleak`` (``python3 -m pip install bleak``).
"""

from __future__ import annotations

import argparse
import asyncio
import binascii
from dataclasses import dataclass
from pathlib import Path
import re
import struct
import sys
from typing import Any


DEVICE_NAME = "Plant-V2-C3"
COMMAND_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
RESPONSE_UUID = "0000fff2-0000-1000-8000-00805f9b34fb"
MAGIC = 0xA5
VERSION = 1
VERSION_V2 = 2
RESPONSE_TYPE = 0x80
MAX_OTA_CHUNK = 496
NOTIFY_SUBSCRIBE_ATTEMPTS = 5
NOTIFY_SUBSCRIBE_RETRY_DELAY_S = 0.5

COMMANDS = {
    "ping": 0x01,
    "state": 0x02,
    "behavior": 0x03,
    "stop": 0x04,
    "begin-ota": 0x10,
    "ota-chunk": 0x11,
    "finish-ota": 0x12,
    "cancel-ota": 0x13,
    "forget-bonds": 0x20,
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
    "SensorFailure",
)
DEVICE_STATES = ("Booting", "Idle", "Interacting", "Sleeping", "Fault", "Updating")
POWER_MODES = ("Active", "LightSleep", "DeepSleep")
BEHAVIOR_NAMES = ("WakeUp", "Happy", "Attention", "Calm", "Sleep", "Error", "Grow")
OTA_STATES = ("Idle", "Receiving", "Verifying", "ReadyToReboot", "Failed")
POSITION_FEEDBACK_STATES = ("Unavailable", "Valid", "OpenCircuit", "ShortCircuit", "OutOfRange")
MOTION_FAULTS = ("None", "FeedbackInvalid", "Stalled", "OppositeDirection", "Timeout")
ACOUSTIC_STATES = ("Quiet", "Speaking", "SustainedSpeech", "SensorFault")
ILLUMINATION_STATES = ("Dark", "Ambient", "BrightExposure", "SensorFault")
CLIMATE_STATES = ("TooCold", "TooHot", "TooDry", "TooHumid", "Suitable", "SensorFault")
BATTERY_STATES = ("Unavailable", "Normal", "Low", "Critical", "SensorFault")
GROWTH_SOURCES = ("None", "Touch", "SustainedSpeech", "BrightExposure", "SuitableClimate")
BLUETOOTH_ADDRESS_PATTERN = re.compile(r"(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}")
BLUEZ_ADAPTER_PATTERN = re.compile(r"hci[0-9]+")


@dataclass(frozen=True)
class Response:
    protocol_version: int
    request_id: int
    request_type: int
    error: int
    device_state: int
    power_mode: int
    behavior: int
    ota_state: int
    ota_received_bytes: int
    firmware_version: int
    capabilities: int = 0
    actual_position: int = 0
    target_position: int = 0
    position_feedback: int = 0
    motion_fault: int = 0
    motion_flags: int = 0
    acoustic_state: int = 0
    volume_level: int = 0
    noise_floor: int = 0
    speaking_duration_s: int = 0
    illumination_state: int = 1
    relative_light: int = 0
    bright_duration_s: int = 0
    climate_state: int = 5
    temperature_centi_c: int = 0
    humidity_tenths_percent: int = 0
    suitable_duration_s: int = 0
    battery_state: int = 0
    battery_level_per_mille: int = 0
    battery_voltage_mv: int = 0
    recent_growth_source: int = 0
    pending_growth_source: int = 0
    growth_flags: int = 0
    ble_flags: int = 0
    active_fault: int = 0

    @property
    def ok(self) -> bool:
        return self.error == 0


def crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def encode_request(
    command_type: int,
    request_id: int,
    payload: bytes = b"",
    version: int = VERSION,
) -> bytes:
    if not 0 <= command_type <= 0xFF:
        raise ValueError("command type must fit in one byte")
    if not 0 <= request_id <= 0xFFFF:
        raise ValueError("request id must fit in two bytes")
    if len(payload) > 500:
        raise ValueError("payload exceeds the 500-byte protocol limit")
    if version not in (VERSION, VERSION_V2):
        raise ValueError("protocol version must be 1 or 2")
    header = struct.pack("<BBBBHH", MAGIC, version, command_type, 0, request_id, len(payload))
    body = header + payload
    return body + struct.pack("<I", crc32(body))


def decode_response(frame: bytes) -> Response:
    if len(frame) < 12:
        raise ValueError("response frame is too short")
    magic, version, frame_type, flags, request_id, payload_size = struct.unpack_from(
        "<BBBBHH", frame
    )
    if magic != MAGIC or version not in (VERSION, VERSION_V2) or frame_type != RESPONSE_TYPE or flags != 0:
        raise ValueError("response header is invalid")
    expected_payload_size = 54 if version == VERSION_V2 else 14
    if payload_size != expected_payload_size or len(frame) != 8 + payload_size + 4:
        raise ValueError("response length is invalid")
    expected_crc = struct.unpack_from("<I", frame, 8 + payload_size)[0]
    if crc32(frame[: 8 + payload_size]) != expected_crc:
        raise ValueError("response CRC32 mismatch")
    fields = struct.unpack_from("<BBBBBBII", frame, 8)
    response = Response(version, request_id, *fields)
    if version == VERSION:
        return response

    response_fields = struct.unpack_from(
        "<IHHBBBBHHHBHHBhHHBHHBBBBB", frame, 22
    )
    return Response(
        version,
        request_id,
        *fields,
        *response_fields,
    )


def enum_name(names: tuple[str, ...], value: int) -> str:
    return names[value] if value < len(names) else f"Unknown({value})"


def describe(response: Response) -> str:
    summary = (
        f"request={response.request_id} type=0x{response.request_type:02x} "
        f"status={enum_name(ERROR_NAMES, response.error)} "
        f"device={enum_name(DEVICE_STATES, response.device_state)} "
        f"power={enum_name(POWER_MODES, response.power_mode)} "
        f"behavior={enum_name(BEHAVIOR_NAMES, response.behavior)} "
        f"ota={enum_name(OTA_STATES, response.ota_state)} "
        f"received={response.ota_received_bytes} "
        f"firmware=0x{response.firmware_version:08x}"
    )
    if response.protocol_version == VERSION:
        return summary
    return (
        summary
        + f" height={response.actual_position}/1000 target={response.target_position}/1000"
        + f" position={enum_name(POSITION_FEEDBACK_STATES, response.position_feedback)}"
        + f" motion_fault={enum_name(MOTION_FAULTS, response.motion_fault)}"
        + f" acoustic={enum_name(ACOUSTIC_STATES, response.acoustic_state)}"
        + f" volume={response.volume_level}/1000"
        + f" light={enum_name(ILLUMINATION_STATES, response.illumination_state)}"
        + f" light_level={response.relative_light}/1000"
        + f" climate={enum_name(CLIMATE_STATES, response.climate_state)}"
        + f" temperature={response.temperature_centi_c / 100:.2f}C"
        + f" humidity={response.humidity_tenths_percent / 10:.1f}%RH"
        + f" battery={enum_name(BATTERY_STATES, response.battery_state)}"
        + f" battery_level={response.battery_level_per_mille / 10:.1f}%"
        + f" battery_voltage={response.battery_voltage_mv}mV"
        + f" growth={enum_name(GROWTH_SOURCES, response.recent_growth_source)}"
        + f" secure={bool(response.ble_flags & 1)} bonded={bool(response.ble_flags & 2)}"
        + f" active_fault={enum_name(ERROR_NAMES, response.active_fault)}"
    )


class BleSession:
    def __init__(self, client: Any, timeout: float, protocol_version: int) -> None:
        self.client = client
        self.timeout = timeout
        self.next_request_id = 1
        self.notifications: asyncio.Queue[bytes] = asyncio.Queue()
        self.protocol_version = protocol_version

    def on_notification(self, _sender: Any, data: bytearray) -> None:
        self.notifications.put_nowait(bytes(data))

    async def request(self, command_type: int, payload: bytes = b"") -> Response:
        request_id = self.next_request_id
        self.next_request_id = 1 if request_id == 0xFFFF else request_id + 1
        await self.client.write_gatt_char(
            COMMAND_UUID,
            encode_request(command_type, request_id, payload, self.protocol_version),
            response=True,
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


def bluetooth_address(value: str) -> str:
    if BLUETOOTH_ADDRESS_PATTERN.fullmatch(value) is None:
        raise argparse.ArgumentTypeError("BLE address must use XX:XX:XX:XX:XX:XX format")
    return value.upper()


def bluez_adapter(value: str) -> str:
    if BLUEZ_ADAPTER_PATTERN.fullmatch(value) is None:
        raise argparse.ArgumentTypeError("BlueZ adapter must use hci<number> format")
    return value


def bluez_device_path(address: str, adapter: str) -> str:
    return f"/org/bluez/{adapter}/dev_{address.replace(':', '_')}"


def make_known_bluez_device(address: str, adapter: str) -> Any:
    """Build a Bleak target for a device already retained by BlueZ.

    A connected peripheral stops advertising, so a scan cannot rediscover it. BlueZ keeps
    paired and connected devices in its object tree; supplying that object path lets Bleak
    reuse the known device instead of starting another address scan.
    """
    from bleak.backends.device import BLEDevice

    return BLEDevice(
        address,
        None,
        {"path": bluez_device_path(address, adapter), "props": {}},
    )


async def resolve_ble_target(
    args: argparse.Namespace,
    scanner: Any,
    known_bluez_device_factory: Any = make_known_bluez_device,
) -> Any:
    if args.address is None:
        print(f"scanning for {args.name} ...")
        target = await scanner.find_device_by_name(args.name, timeout=args.scan_timeout)
        if target is None:
            raise TimeoutError(f"BLE device {args.name!r} was not found while advertising")
        return target

    print(f"scanning for BLE address {args.address} ...")
    target = await scanner.find_device_by_address(args.address, timeout=args.scan_timeout)
    if target is not None:
        return target

    if sys.platform.startswith("linux"):
        print(
            "device is not advertising; trying the paired/connected BlueZ object "
            f"on {args.bluez_adapter} ..."
        )
        return known_bluez_device_factory(args.address, args.bluez_adapter)

    raise TimeoutError(f"BLE device {args.address} was not found while advertising")


def connection_failure_message(error: BaseException, target_description: str) -> str:
    detail = str(error) or error.__class__.__name__
    if "le-connection-abort-by-local" in detail:
        return (
            f"BLE connection to {target_description} was aborted by local BlueZ; "
            "stop scanning, reset the controller or C3, and retry without pairing again"
        )
    if error.__class__.__name__ == "BleakDeviceNotFoundError":
        return (
            f"BLE device {target_description} is not present in the BlueZ object cache and "
            "was not advertising; reset the C3 and retry"
        )
    if isinstance(error, TimeoutError) or error.__class__.__name__ == "TimeoutError":
        return (
            f"BLE connection to {target_description} timed out; the bond is not deleted, so "
            "stop scanning, reset the local controller or C3, and retry"
        )
    return f"BLE connection to {target_description} failed: {detail}"


async def start_notify_with_retry(
    client: Any,
    callback: Any,
    attempts: int = NOTIFY_SUBSCRIBE_ATTEMPTS,
    retry_delay_s: float = NOTIFY_SUBSCRIBE_RETRY_DELAY_S,
) -> None:
    """Wait for bonded-link encryption before subscribing to protected notifications."""
    last_error: BaseException | None = None
    for attempt in range(1, attempts + 1):
        try:
            await client.start_notify(RESPONSE_UUID, callback)
            return
        except Exception as error:
            last_error = error
            if attempt == attempts or not client.is_connected:
                raise
            print(
                "response notification subscription is not ready; "
                f"waiting for link security ({attempt}/{attempts}) ..."
            )
            await asyncio.sleep(retry_delay_s)
    assert last_error is not None
    raise last_error


async def run_hardware_command(args: argparse.Namespace) -> None:
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError as error:
        raise SystemExit("hardware commands require bleak: python3 -m pip install bleak") from error

    target_description = args.address or args.name
    try:
        target = await resolve_ble_target(args, BleakScanner)
    except TimeoutError as error:
        raise SystemExit(str(error)) from error

    client = BleakClient(target, timeout=args.timeout)
    try:
        await client.connect()
    except Exception as error:
        raise SystemExit(connection_failure_message(error, target_description)) from error

    try:
        session = BleSession(client, args.timeout, args.protocol_version)
        try:
            await start_notify_with_retry(client, session.on_notification)
        except Exception as error:
            detail = str(error) or error.__class__.__name__
            raise SystemExit(
                "BLE connected, but encrypted response notification subscription failed "
                f"after {NOTIFY_SUBSCRIBE_ATTEMPTS} attempts: {detail}"
            ) from error
        if args.command in ("ping", "state", "stop", "cancel-ota", "forget-bonds"):
            await require_success(session, args.command)
        elif args.command == "behavior":
            await require_success(session, "behavior", bytes((BEHAVIORS[args.behavior],)))
        elif args.command == "ota":
            await transfer_ota(session, args)
    finally:
        if client.is_connected:
            await client.disconnect()


def integer(value: str) -> int:
    return int(value, 0)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--address",
        type=bluetooth_address,
        help="BLE address; explicitly discovers it, then reuses the known BlueZ object",
    )
    parser.add_argument("--name", default=DEVICE_NAME, help="device name used while scanning")
    parser.add_argument("--timeout", type=float, default=5.0, help="response timeout in seconds")
    parser.add_argument("--scan-timeout", type=float, default=10.0)
    parser.add_argument(
        "--bluez-adapter",
        type=bluez_adapter,
        default="hci0",
        help="Linux BlueZ adapter used for a cached device path (default: hci0)",
    )
    parser.add_argument(
        "--protocol-version", type=int, choices=(VERSION, VERSION_V2), default=VERSION_V2
    )
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ("ping", "state", "stop", "cancel-ota", "forget-bonds"):
        subparsers.add_parser(name)

    behavior = subparsers.add_parser("behavior")
    behavior.add_argument("behavior", choices=tuple(BEHAVIORS))

    ota = subparsers.add_parser("ota")
    ota.add_argument("image", type=Path)
    ota.add_argument("--version", type=integer, required=True)
    ota.add_argument("--product-id", type=integer, default=0x504C414E)
    ota.add_argument("--hardware-revision", type=integer, default=2)
    return parser


def main() -> None:
    asyncio.run(run_hardware_command(build_parser().parse_args()))


if __name__ == "__main__":
    main()
