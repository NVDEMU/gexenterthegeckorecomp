#!/usr/bin/env python3
"""Convert mono 16-bit PCM WAV into a small VAG file using PS1 ADPCM blocks.

The encoder uses predictor 0 and per-block shift selection. ffmpeg performs the
MP3 decode/resample step. VAG output contains a 0x30-byte VAGp header followed
by 16-byte SPU ADPCM blocks.
"""
from __future__ import annotations

import argparse
import struct
import wave
from pathlib import Path


def _round_ratio(value: int, step: int) -> int:
    # Symmetric nearest-integer rounding, avoiding Python's bankers' rounding.
    if value >= 0:
        return (value + step // 2) // step
    return -((-value + step // 2) // step)


def encode_adpcm(samples: list[int]) -> bytes:
    if not samples:
        raise ValueError("WAV contains no audio samples")

    # Headroom ensures the positive endpoint can be represented by predictor 0.
    samples = [int(value * 0.85) for value in samples]
    padded = samples + [0] * ((-len(samples)) % 28)
    output = bytearray()
    blocks = len(padded) // 28

    for block_index in range(blocks):
        block = padded[block_index * 28:(block_index + 1) * 28]
        quantized: list[int] | None = None
        selected_shift = 0
        # Higher shift means finer resolution. Take the finest shift that fits
        # all signed 4-bit samples without clipping.
        for shift in range(12, -1, -1):
            step = 1 << (12 - shift)
            candidate = [_round_ratio(sample, step) for sample in block]
            if all(-8 <= value <= 7 for value in candidate):
                selected_shift = shift
                quantized = candidate
                break
        if quantized is None:
            raise ValueError("Could not fit an ADPCM block; expected PCM16 audio")

        flags = 0x01 if block_index == blocks - 1 else 0x00
        output.append(selected_shift & 0x0F)  # predictor/filter 0, then shift
        output.append(flags)
        for index in range(0, 28, 2):
            low = quantized[index] & 0x0F
            high = quantized[index + 1] & 0x0F
            output.append(low | (high << 4))

    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_wav", type=Path)
    parser.add_argument("output_vag", type=Path)
    args = parser.parse_args()

    with wave.open(str(args.input_wav), "rb") as wav:
        if wav.getcomptype() != "NONE":
            raise SystemExit("Input WAV must use uncompressed PCM")
        if wav.getsampwidth() != 2 or wav.getnchannels() != 1:
            raise SystemExit("Input WAV must be 16-bit mono PCM")
        sample_rate = wav.getframerate()
        pcm = wav.readframes(wav.getnframes())
    if sample_rate <= 0 or not pcm:
        raise SystemExit("Input WAV is empty or has an invalid sample rate")

    samples = list(struct.unpack("<" + "h" * (len(pcm) // 2), pcm))
    adpcm = encode_adpcm(samples)

    name = args.output_vag.stem.encode("ascii", "replace")[:16].ljust(16, b"\0")
    header = b"VAGp" + struct.pack(">III", 0x20, 0, len(adpcm))
    header += struct.pack(">I", sample_rate) + bytes(12) + name
    if len(header) != 0x30:
        raise SystemExit("Internal error: VAG header is not 0x30 bytes")
    args.output_vag.parent.mkdir(parents=True, exist_ok=True)
    args.output_vag.write_bytes(header + adpcm)
    print(f"Encoded {args.input_wav.name}: {len(samples)} samples, {len(adpcm)} ADPCM bytes -> {args.output_vag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
