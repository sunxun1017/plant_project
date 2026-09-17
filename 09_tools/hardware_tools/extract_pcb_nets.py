"""Read Altium Pads6/Components6/Nets6 membership; NOT copper DRC or ERC.

Usage: python extract_pcb_nets.py ZW.PcbDoc --output pcb_nets.csv
Requires olefile. Supports the six-subrecord Pads6 layout in the supplied release.
Unknown / truncated layouts fail rather than silently guessing connectivity.
"""
import argparse
import csv
import struct
from pathlib import Path

import olefile


def block(data, position):
    if position + 4 > len(data):
        raise ValueError("Truncated record length")
    length = struct.unpack_from("<I", data, position)[0]
    begin = position + 4
    if begin + length > len(data):
        raise ValueError("Truncated record body")
    return data[begin:begin + length], begin + length


def records(document, stream):
    data = document.openstream([stream, "Data"]).read()
    result, position = [], 0
    while position < len(data):
        body, position = block(data, position)
        result.append(dict(field.split("=", 1) for field in
                           body.decode("latin1").rstrip("\0").split("|") if "=" in field))
    return result


def extract(path):
    with olefile.OleFileIO(path) as document:
        nets = records(document, "Nets6")
        components = records(document, "Components6")
        data = document.openstream(["Pads6", "Data"]).read()
        expected = struct.unpack("<I", document.openstream(["Pads6", "Header"]).read())[0]
        position, result = 0, []
        while position < len(data):
            if data[position] != 2:
                raise ValueError(f"Unsupported pad type at byte {position}")
            position += 1
            parts = []
            for _ in range(6):
                body, position = block(data, position)
                parts.append(body)
            if len(parts[4]) < 9 or not parts[0] or parts[0][0] != len(parts[0]) - 1:
                raise ValueError("Unsupported Pads6 layout")
            net = struct.unpack_from("<H", parts[4], 3)[0]
            component = struct.unpack_from("<H", parts[4], 7)[0]
            if (net != 65535 and net >= len(nets)) or (component != 65535 and component >= len(components)):
                raise ValueError("Out-of-range net or component reference")
            result.append({
                "component": components[component].get("SOURCEDESIGNATOR", "") if component != 65535 else "",
                "pad": parts[0][1:].decode("latin1"),
                "net": nets[net]["NAME"] if net != 65535 else "",
            })
        if len(result) != expected:
            raise ValueError(f"Header reports {expected} pads, decoded {len(result)}")
        return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcb", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    pads = extract(args.pcb)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=["component", "pad", "net"])
        writer.writeheader()
        writer.writerows(pads)
    print(f"Extracted {len(pads)} pad memberships; no copper connectivity validation performed.")
