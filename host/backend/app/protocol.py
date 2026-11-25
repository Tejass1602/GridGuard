"""COBS/CRC framing and exact little-endian decoders for GridGuard v1."""
from __future__ import annotations

from dataclasses import dataclass
import struct
from typing import Any

VERSION = 1
MAX_FRAME = 512
HEADER = struct.Struct("<BBHH")


class ProtocolError(ValueError):
    """Malformed, unsupported, or corrupt wire frame."""


@dataclass(frozen=True)
class Frame:
    msg_type: int
    seq: int
    payload: bytes
    version: int = VERSION


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def cobs_encode(data: bytes) -> bytes:
    out = bytearray([0])
    code_index = 0
    code = 1
    for byte in data:
        if byte == 0:
            out[code_index] = code
            code_index = len(out)
            out.append(0)
            code = 1
        else:
            out.append(byte)
            code += 1
            if code == 0xFF:
                out[code_index] = code
                code_index = len(out)
                out.append(0)
                code = 1
    out[code_index] = code
    return bytes(out)


def cobs_decode(data: bytes) -> bytes:
    if not data:
        raise ProtocolError("empty COBS packet")
    out = bytearray()
    index = 0
    while index < len(data):
        code = data[index]
        if code == 0:
            raise ProtocolError("zero byte inside COBS packet")
        index += 1
        end = index + code - 1
        if end > len(data):
            raise ProtocolError("COBS run exceeds packet length")
        out.extend(data[index:end])
        index = end
        if code != 0xFF and index < len(data):
            out.append(0)
    return bytes(out)


def encode_frame(msg_type: int, seq: int, payload: bytes = b"") -> bytes:
    if len(payload) + HEADER.size + 2 > MAX_FRAME:
        raise ProtocolError("frame exceeds 512-byte maximum")
    header = HEADER.pack(VERSION, msg_type, seq & 0xFFFF, len(payload))
    raw = header + payload
    raw += struct.pack("<H", crc16_ccitt(raw))
    return cobs_encode(raw) + b"\x00"


def decode_frame(packet: bytes) -> Frame:
    encoded = packet[:-1] if packet.endswith(b"\x00") else packet
    raw = cobs_decode(encoded)
    if len(raw) < HEADER.size + 2 or len(raw) > MAX_FRAME:
        raise ProtocolError("invalid decoded frame length")
    version, msg_type, seq, payload_len = HEADER.unpack_from(raw)
    if version != VERSION:
        raise ProtocolError(f"unsupported protocol version {version}")
    if len(raw) != HEADER.size + payload_len + 2:
        raise ProtocolError("payload length does not match frame")
    expected = struct.unpack_from("<H", raw, len(raw) - 2)[0]
    if crc16_ccitt(raw[:-2]) != expected:
        raise ProtocolError("CRC-16-CCITT mismatch")
    return Frame(msg_type, seq, raw[HEADER.size:-2], version)


def _unpack(fmt: str, payload: bytes) -> tuple[Any, ...]:
    parser = struct.Struct(fmt)
    if len(payload) != parser.size:
        raise ProtocolError(f"message payload has {len(payload)} bytes; expected {parser.size}")
    return parser.unpack(payload)


def decode_message(frame: Frame) -> dict[str, Any]:
    p = frame.payload
    if frame.msg_type == 0x01:
        major, minor, board, mode, uptime = _unpack("<BBBBI", p)
        return {"kind": "hello", "fw_major": major, "fw_minor": minor, "board_id": board, "feedback_mode": mode, "uptime_ms": uptime}
    if frame.msg_type == 0x02:
        values = _unpack("<I16f", p)
        names = ("ts_ms", "v_rms", "i_rms", "p_w", "q1_var", "s_va", "pf", "freq_hz", "thd_i_pct", "thd_v_pct", "energy_wh", "thermal_pct")
        data = dict(zip(names, values[:12]))
        data["harm_i_rms"] = list(values[12:17])
        return {"kind": "telemetry", **data}
    if frame.msg_type == 0x03:
        if len(p) != 4 + 128 * 2:
            raise ProtocolError("waveform payload has invalid size")
        ts = struct.unpack_from("<I", p)[0]
        samples = struct.unpack_from("<128h", p, 4)
        return {"kind": "waveform", "ts_ms": ts, "v_samples": list(samples[:64]), "i_samples": list(samples[64:])}
    if frame.msg_type == 0x04:
        vals = _unpack("<IBBBB10fI", p)
        return {"kind": "nilm", "ts_ms": vals[0], "active_mask": vals[1], "last_event_id": vals[2], "last_event_type": vals[3], "est_power_w": list(vals[5:9]), "confidence": list(vals[9:13]), "unknown_power_w": vals[13], "last_mahalanobis": vals[14], "inference_us": vals[15]}
    if frame.msg_type == 0x05:
        vals = _unpack("<IBBBBBBHffI", p)
        return {"kind": "protection", "ts_ms": vals[0], "relay_state": vals[1], "active_fault": vals[2], "coil_cmd": vals[3], "aux_state": vals[4], "feedback_mode": vals[5], "verify_samples": vals[6], "v_src_v": vals[8], "v_load_v": vals[9], "trip_latency_us": vals[10]}
    if frame.msg_type == 0x06:
        vals = _unpack("<IIBBBBffffHI", p)
        return {"kind": "fault_record", "record_id": vals[0], "ts_ms": vals[1], "fault_code": vals[2], "relay_result": vals[3], "coil_cmd": vals[4], "aux_state": vals[5], "trigger_val": vals[6], "threshold_val": vals[7], "v_src_v": vals[8], "v_load_v": vals[9], "verify_ms": vals[10], "crc32": vals[11]}
    if frame.msg_type == 0x07:
        vals = _unpack("<IIIIIHH", p)
        return {"kind": "health", "ts_ms": vals[0], "dsp_exec_us": vals[1], "max_dsp_us": vals[2], "dropped_frames": vals[3], "uart_crc_err": vals[4], "nvs_records": vals[5], "free_stack_min": vals[6]}
    if frame.msg_type == 0x15:
        seq, cmd, status = _unpack("<HBB", p)
        return {"kind": "ack", "cmd_seq": seq, "cmd_type": cmd, "status": status}
    raise ProtocolError(f"unknown message type 0x{frame.msg_type:02x}")
