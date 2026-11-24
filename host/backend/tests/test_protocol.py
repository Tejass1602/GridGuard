import pytest

from app.protocol import ProtocolError, cobs_decode, cobs_encode, decode_frame, encode_frame


def test_cobs_frame_round_trip_zero_and_ff_bytes():
    payload = bytes([0, 255, 1, 0, 255, 0, 17, 34, 0])
    wire = encode_frame(0x10, 0xBEEF, payload)
    assert wire.endswith(b"\x00")
    frame = decode_frame(wire)
    assert frame.msg_type == 0x10
    assert frame.seq == 0xBEEF
    assert frame.payload == payload
    assert cobs_decode(cobs_encode(payload)) == payload


def test_crc_rejects_one_bit_flip():
    wire = bytearray(encode_frame(0x02, 7, b"\x00\xff\x42"))
    wire[2] ^= 0x01
    with pytest.raises(ProtocolError, match="CRC"):
        decode_frame(bytes(wire))


def test_malformed_cobs_is_rejected():
    with pytest.raises(ProtocolError):
        cobs_decode(b"\x04\x10")
