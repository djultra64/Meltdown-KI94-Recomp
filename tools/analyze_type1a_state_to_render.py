#!/usr/bin/env python3
"""Verify KI 1.5d type-0x1a state-to-render traces.

The MAME debugger log is intentionally verbose: it captures original R4600
registers at the instructions where the game resolves each transform.  This
tool reduces that evidence to a stable CSV, but first recomputes every declared
field with the original 32-bit integer and 12-bit fixed-point semantics.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
import re
import sys
from typing import Iterable, TextIO


TRACE_PREFIX = "KI_S2R "
FRAMEBUFFER_BASE = 0x80030000
FRAMEBUFFER_BANK_BYTES = 0x28000
SCREEN_WIDTH = 320
ROW_STRIDE = SCREEN_WIDTH * 2
PACKED_HEADER_BYTES = 8
LIVE_PALETTE_TABLE = 0x8803480A

REQUIRED_PHASES = (
    "input",
    "renderer",
    "vertical_fixed",
    "surface",
    "clip",
    "dispatch",
    "first_row",
    "palette_lookup",
    "done",
)
PHASE_ORDER = {
    "input": 0,
    "renderer": 1,
    "vertical_fixed": 2,
    "surface": 3,
    "horizontal_fixed": 4,
    "clip": 5,
    "dispatch": 6,
    "first_row": 7,
    "palette_lookup": 8,
    "done": 9,
}

CSV_FIELDS = (
    "call",
    "tick",
    "pass_index",
    "object",
    "token",
    "frame_pointer",
    "frame_origin_x",
    "frame_origin_y",
    "frame_width",
    "frame_height",
    "framebuffer_bank",
    "palette_source",
    "horizontal_minimum",
    "horizontal_limit",
    "scaled_width",
    "horizontal_clip",
    "render_mode",
    "row_callback",
    "packed_data_offset",
    "source_cursor_offset",
    "destination_x",
    "destination_y",
    "x_direction",
    "y_direction",
    "x_scale",
    "x_remainder",
    "y_accumulator",
    "y_scale",
    "output_rows",
    "first_palette_index",
    "milestones",
)

# POC-110's native fixture contains only scalar object/header state and the
# resulting descriptor. It deliberately excludes packed-frame and palette
# bytes, which remain lawful local inputs rather than repository artifacts.
BRIDGE_CSV_FIELDS = (
    "call",
    "tick",
    "pass_index",
    "milestones",
    "record_address",
    "token",
    "pixel_step",
    "frame_address",
    "record_scale_x",
    "record_scale_y",
    "word68",
    "word6c",
    "word70",
    "facing",
    "display_class",
    "flags",
    "horizontal_minimum",
    "horizontal_limit",
    "framebuffer_select",
    "viewport_height",
    "frame_origin_x",
    "frame_origin_y",
    "frame_width",
    "frame_height",
    "skipped_source_rows",
    "skipped_source_bytes",
    "palette_source",
    "framebuffer_address",
    "scaled_width",
    "horizontal_clip",
    "render_mode",
    "row_callback",
    "destination_x",
    "destination_y",
    "x_direction",
    "y_direction",
    "x_scale",
    "x_remainder",
    "y_accumulator",
    "y_scale",
    "source_offset",
    "output_rows",
    "field8e",
    "field97",
)


class TraceError(ValueError):
    """Raised when a trace is incomplete, reordered, or internally inconsistent."""


@dataclass
class TraceCall:
    """All debugger phases belonging to one original renderer call."""

    call_id: int
    phases: dict[str, dict[str, str]]


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def s32(value: int) -> int:
    value = u32(value)
    return value - 0x100000000 if value & 0x80000000 else value


def s16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def s8(value: int) -> int:
    value &= 0xFF
    return value - 0x100 if value & 0x80 else value


def hex_value(fields: dict[str, str], name: str, *, phase: str) -> int:
    try:
        text = fields[name]
    except KeyError as error:
        raise TraceError(f"phase {phase!r} is missing field {name!r}") from error
    if not re.fullmatch(r"[0-9A-Fa-f]+", text):
        raise TraceError(f"phase {phase!r} field {name!r} is not hexadecimal: {text!r}")
    return int(text, 16)


def tuple_hex(fields: dict[str, str], name: str, count: int, *, phase: str) -> list[int]:
    try:
        parts = fields[name].split(":")
    except KeyError as error:
        raise TraceError(f"phase {phase!r} is missing field {name!r}") from error
    if len(parts) != count or any(not re.fullmatch(r"[0-9A-Fa-f]+", part) for part in parts):
        raise TraceError(
            f"phase {phase!r} field {name!r} must contain {count} hexadecimal values"
        )
    return [int(part, 16) for part in parts]


def parse_trace(lines: Iterable[str]) -> list[TraceCall]:
    """Parse relevant log lines and enforce strict call/phase ordering."""
    calls: list[TraceCall] = []
    current: TraceCall | None = None
    previous_order = -1

    for line_number, raw_line in enumerate(lines, 1):
        marker = raw_line.find(TRACE_PREFIX)
        if marker < 0:
            continue
        tokens = raw_line[marker + len(TRACE_PREFIX) :].strip().split()
        fields: dict[str, str] = {}
        for token in tokens:
            if "=" not in token:
                raise TraceError(f"line {line_number}: malformed token {token!r}")
            name, value = token.split("=", 1)
            if not name or not value or name in fields:
                raise TraceError(f"line {line_number}: malformed or duplicate field {name!r}")
            fields[name] = value

        phase = fields.pop("phase", None)
        if phase not in PHASE_ORDER:
            raise TraceError(f"line {line_number}: unknown or missing phase {phase!r}")
        call_id = hex_value(fields, "call", phase=phase)
        fields.pop("call")

        if phase == "input":
            if current is not None:
                raise TraceError(
                    f"line {line_number}: call {current.call_id:X} has no done phase"
                )
            if call_id != len(calls):
                raise TraceError(
                    f"line {line_number}: expected call {len(calls):X}, got {call_id:X}"
                )
            current = TraceCall(call_id, {})
            previous_order = -1
        elif current is None:
            raise TraceError(f"line {line_number}: phase {phase!r} appears before input")

        assert current is not None
        if call_id != current.call_id:
            raise TraceError(
                f"line {line_number}: phase {phase!r} belongs to call {call_id:X}, "
                f"but call {current.call_id:X} is active"
            )
        order = PHASE_ORDER[phase]
        if order <= previous_order:
            raise TraceError(f"line {line_number}: phase {phase!r} is duplicated or reordered")
        if phase in current.phases:
            raise TraceError(f"line {line_number}: duplicate phase {phase!r}")
        current.phases[phase] = fields
        previous_order = order

        if phase == "done":
            missing = [name for name in REQUIRED_PHASES if name not in current.phases]
            if missing:
                raise TraceError(
                    f"call {current.call_id:X} is missing phase(s): {', '.join(missing)}"
                )
            calls.append(current)
            current = None

    if current is not None:
        raise TraceError(f"call {current.call_id:X} is missing its done phase")
    if not calls:
        raise TraceError("trace contains no KI_S2R calls")
    return calls


def expect(call_id: int, phase: str, field: str, actual: int, expected: int) -> None:
    """Report the first arithmetic mismatch with its original trace location."""
    if actual != expected:
        raise TraceError(
            f"call {call_id:X} phase {phase} field {field}: "
            f"got 0x{actual:X}, expected 0x{expected:X}"
        )


def scaled_word(base: int, scale: int) -> int:
    """Model 0x88001c20-0x88001c4c, including MIPS LO truncation."""
    if scale == 0:
        return u32(base)
    return u32(base * scale) >> 12


def resolve_horizontal_mode(
    display_class: int,
    x_scale: int,
    pixel_step: int,
    x_integer: int,
    scaled_width: int,
    horizontal_minimum: int,
    horizontal_limit: int,
) -> tuple[int, str]:
    """Model the scale and horizontal-clip mode bits set before 0x88010bd4."""
    if x_scale == 0x1000:
        mode = display_class
    elif x_scale > 0x1000:
        mode = display_class + 0x10
    else:
        mode = display_class + 0x20

    # 0x88010aa4 adds the reverse-direction selector before the clip bit.
    if pixel_step < 0:
        mode += 0x04

    far_edge = x_integer + scaled_width if pixel_step > 0 else x_integer - scaled_width
    if pixel_step > 0:
        if x_integer >= horizontal_limit or far_edge <= horizontal_minimum:
            raise TraceError("horizontal span is entirely outside the clip bounds")
        left_clipped = x_integer < horizontal_minimum
        right_clipped = far_edge > horizontal_limit
    else:
        if x_integer < horizontal_minimum or far_edge >= horizontal_limit:
            raise TraceError("negative-step span is entirely outside the clip bounds")
        left_clipped = far_edge < horizontal_minimum
        right_clipped = x_integer >= horizontal_limit

    if left_clipped or right_clipped:
        mode |= 0x08
    if left_clipped and right_clipped:
        label = "both"
    elif left_clipped:
        label = "left"
    elif right_clipped:
        label = "right"
    else:
        label = "none"
    return mode, label


def derive_row(call: TraceCall) -> dict[str, object]:
    """Recompute the observed type-0x1a transform from record/global inputs."""
    call_id = call.call_id
    input_fields = call.phases["input"]
    renderer = call.phases["renderer"]
    vertical = call.phases["vertical_fixed"]
    surface = call.phases["surface"]
    clip = call.phases["clip"]
    dispatch = call.phases["dispatch"]
    first_row = call.phases["first_row"]
    done = call.phases["done"]

    object_address = u32(hex_value(input_fields, "object", phase="input"))
    expect(call_id, "input", "type", hex_value(input_fields, "type", phase="input"), 0x1A)
    expect(call_id, "input", "display94", hex_value(input_fields, "display94", phase="input"), 0x60)
    expect(call_id, "input", "flags95", hex_value(input_fields, "flags95", phase="input"), 0)
    expect(call_id, "done", "object", u32(hex_value(done, "object", phase="done")), object_address)

    frame_record = u32(hex_value(input_fields, "frame30", phase="input"))
    frame_register = u32(hex_value(renderer, "frame", phase="renderer"))
    expect(call_id, "renderer", "frame", frame_register, frame_record)
    expect(
        call_id,
        "first_row",
        "frame",
        u32(hex_value(first_row, "frame", phase="first_row")),
        frame_register,
    )

    origin_x_raw, origin_y_raw, width_raw, height_raw = tuple_hex(
        renderer, "frame_header", 4, phase="renderer"
    )
    origin_x = s16(origin_x_raw)
    origin_y = s16(origin_y_raw)
    frame_width = s16(width_raw)
    frame_height = s16(height_raw)
    if frame_width <= 0 or frame_height <= 0:
        raise TraceError(f"call {call_id:X}: non-positive packed-frame dimensions")

    word68 = tuple_hex(input_fields, "word68", 1, phase="input")[0]
    word6c = tuple_hex(input_fields, "word6c", 1, phase="input")[0]
    word70 = tuple_hex(input_fields, "word70", 1, phase="input")[0]
    scale_x_record, scale_y_record = tuple_hex(input_fields, "scale58", 2, phase="input")
    x_scale = scaled_word(word70, scale_x_record)
    y_scale = scaled_word(word70, scale_y_record)
    if x_scale == 0 or y_scale == 0:
        raise TraceError(f"call {call_id:X}: zero resolved scale is outside this oracle")
    x_position = s32(word68) >> 8
    y_position = s32(word6c) >> 8
    pixel_step = s8(hex_value(input_fields, "byte24", phase="input"))
    if pixel_step == 0:
        raise TraceError(f"call {call_id:X}: zero pixel step is unsupported")

    expected_registers = {
        "t1": x_scale,
        "t3": y_scale,
        "t4": u32(pixel_step),
        "s6": 0x60,
        "s7": u32(x_position),
        "t8": u32(y_position),
        "a0": 0,
        "gp": 0,
        "k0": hex_value(input_fields, "clipc8", phase="input"),
        "k1": hex_value(input_fields, "widthca", phase="input") or SCREEN_WIDTH,
        "fp": hex_value(input_fields, "viewport", phase="input"),
        "s2": ROW_STRIDE,
    }
    for name, expected in expected_registers.items():
        actual = u32(hex_value(renderer, name, phase="renderer"))
        expect(call_id, "renderer", name, actual, u32(expected))

    vertical_clip_origin = s32(hex_value(renderer, "s5", phase="renderer")) >> 12
    vertical_product = u32((origin_y - expected_registers["a0"]) * y_scale)
    y_base_fixed = u32((y_position << 4) + 0x800)
    y_with_origin = u32(vertical_product + y_base_fixed)
    for name, expected in (
        ("frame_origin_y_product", vertical_product),
        ("y_scale", y_scale),
        ("y_base_fixed", y_base_fixed),
        ("y_with_origin", y_with_origin),
    ):
        expect(
            call_id,
            "vertical_fixed",
            name,
            u32(hex_value(vertical, name, phase="vertical_fixed")),
            expected,
        )

    y_integer = s32(y_with_origin) >> 12
    vertical_minimum = s32(expected_registers["gp"])
    vertical_limit = s32(expected_registers["fp"])
    if y_integer < vertical_minimum:
        raise TraceError(f"call {call_id:X}: trace unexpectedly reaches renderer below top clip")
    if y_integer >= vertical_limit:
        y_integer = vertical_limit - 1

    fbselect_register = hex_value(surface, "fbselect_register", phase="surface")
    expect(
        call_id,
        "input",
        "fbselect",
        hex_value(input_fields, "fbselect", phase="input"),
        fbselect_register & 0xFF,
    )
    framebuffer_bank = fbselect_register & 1
    framebuffer = FRAMEBUFFER_BASE + framebuffer_bank * FRAMEBUFFER_BANK_BYTES
    row_byte_offset = u32(ROW_STRIDE * y_integer)
    row_pointer = u32(framebuffer + row_byte_offset)
    origin_x_product = u32(origin_x * x_scale)
    scaled_width = u32(frame_width * x_scale) >> 12
    scaled_height = u32(frame_height * y_scale) >> 12
    x_base_fixed = u32((x_position << 4) + 0x800)
    expected_surface = {
        "framebuffer": framebuffer,
        "row_y": u32(y_integer),
        "row_byte_offset": row_byte_offset,
        "frame_origin_x_product": origin_x_product,
        "scaled_width": scaled_width,
        "scaled_height": scaled_height,
        "x_base_fixed": x_base_fixed,
        "row_pointer": row_pointer,
    }
    for name, expected in expected_surface.items():
        expect(
            call_id,
            "surface",
            name,
            u32(hex_value(surface, name, phase="surface")),
            expected,
        )

    if pixel_step > 0:
        x_fixed = u32(x_base_fixed - origin_x_product)
        x_remainder = x_fixed & 0xFFF
        x_direction = 1
    else:
        x_fixed = u32(x_base_fixed + origin_x_product)
        x_remainder = (x_fixed & 0xFFF) ^ 0xFFF
        x_direction = -1
    x_integer = s32(x_fixed) >> 12
    horizontal_minimum = expected_registers["k0"]
    horizontal_limit = expected_registers["k1"]
    try:
        render_mode, horizontal_clip = resolve_horizontal_mode(
            hex_value(input_fields, "display94", phase="input"),
            x_scale,
            pixel_step,
            x_integer,
            scaled_width,
            horizontal_minimum,
            horizontal_limit,
        )
    except TraceError as error:
        raise TraceError(f"call {call_id:X}: {error}") from error

    available_rows = y_integer - vertical_minimum + 1
    output_rows = min(scaled_height, available_rows)
    if output_rows <= 0:
        raise TraceError(f"call {call_id:X}: no output rows remain after vertical clipping")
    vertical_fraction_prebias = (y_with_origin & 0xFFF) ^ 0xFFF
    expected_clip = {
        "render_mode": render_mode,
        "next_row_left_boundary": u32(row_pointer + ROW_STRIDE + 2 * horizontal_minimum),
        "next_row_right_boundary": u32(row_pointer + ROW_STRIDE + 2 * horizontal_limit),
        "available_rows": available_rows,
        "y_integer": u32(y_integer),
        "vertical_clip_origin": u32(vertical_clip_origin),
        "vertical_fraction_prebias": vertical_fraction_prebias,
        "horizontal_remainder": x_remainder,
        "row_stride": ROW_STRIDE,
    }
    for name, expected in expected_clip.items():
        expect(call_id, "clip", name, u32(hex_value(clip, name, phase="clip")), u32(expected))

    expect(
        call_id,
        "dispatch",
        "render_mode",
        hex_value(dispatch, "render_mode", phase="dispatch"),
        render_mode,
    )
    expect(
        call_id,
        "dispatch",
        "palette_table",
        u32(hex_value(dispatch, "palette_table", phase="dispatch")),
        LIVE_PALETTE_TABLE,
    )
    row_callback = u32(hex_value(dispatch, "row_callback", phase="dispatch"))
    expected_callback = 0x880118C0 if horizontal_clip != "none" else 0x88011918
    expect(
        call_id,
        "dispatch",
        "row_callback",
        row_callback,
        expected_callback,
    )

    source_cursor = u32(hex_value(first_row, "source", phase="first_row"))
    source_cursor_offset = u32(source_cursor - frame_register)
    skipped_source_rows = hex_value(first_row, "skipped_source_rows", phase="first_row")
    skipped_source_bytes = hex_value(first_row, "skipped_source_bytes", phase="first_row")
    expected_skipped_rows = 0
    downscale_accumulator = vertical_fraction_prebias + y_scale
    while downscale_accumulator <= 0xFFF:
        expected_skipped_rows += 1
        downscale_accumulator += y_scale
    expect(
        call_id,
        "first_row",
        "skipped_source_rows",
        skipped_source_rows,
        expected_skipped_rows,
    )
    expected_source_cursor = PACKED_HEADER_BYTES + skipped_source_bytes + 1
    expect(call_id, "first_row", "source cursor", source_cursor_offset, expected_source_cursor)
    if (skipped_source_rows == 0) != (skipped_source_bytes == 0):
        raise TraceError(
            f"call {call_id:X}: skipped source row/byte counters disagree"
        )
    destination = u32(hex_value(first_row, "destination", phase="first_row"))
    expected_destination = u32(row_pointer + 2 * x_integer)
    expect(call_id, "first_row", "destination", destination, expected_destination)
    destination_word = (destination - framebuffer) // 2
    destination_y, destination_x = divmod(destination_word, SCREEN_WIDTH)

    # At 0x88010bd4 the renderer biases the complemented vertical fraction by
    # 0x1000. The first packed-row dispatch consumes that bias while adding one
    # y-scale step. Each rejected downscale row adds another step without
    # consuming an output row.
    y_accumulator = vertical_fraction_prebias + (skipped_source_rows + 1) * y_scale
    expected_first_row = {
        "x_scale": x_scale,
        "x_remainder": x_remainder,
        "y_accumulator": y_accumulator,
        "y_scale": y_scale,
        "rows_minus_one": output_rows - 1,
        "row_stride": ROW_STRIDE,
        "pixel_step": u32(pixel_step),
        "palette": LIVE_PALETTE_TABLE,
    }
    for name, expected in expected_first_row.items():
        expect(
            call_id,
            "first_row",
            name,
            u32(hex_value(first_row, name, phase="first_row")),
            u32(expected),
        )

    facing = s8(hex_value(input_fields, "facing8c", phase="input"))
    selected_palette = 0x43 if facing < 0 else facing
    palette_source = u32(0x88090100 + (selected_palette << 8))
    expect(
        call_id,
        "input",
        "palette_source",
        u32(hex_value(input_fields, "palette_source", phase="input")),
        palette_source,
    )
    palette = call.phases["palette_lookup"]
    expect(
        call_id,
        "palette_lookup",
        "table",
        u32(hex_value(palette, "table", phase="palette_lookup")),
        LIVE_PALETTE_TABLE,
    )
    palette_address = u32(hex_value(palette, "address", phase="palette_lookup"))
    palette_byte_offset = palette_address - LIVE_PALETTE_TABLE
    if palette_byte_offset < 0 or palette_byte_offset > 62 or palette_byte_offset & 1:
        raise TraceError(
            f"call {call_id:X}: invalid palette lookup address 0x{palette_address:08X}"
        )
    first_palette_index = str(palette_byte_offset // 2)

    return {
        "call": call_id,
        "tick": -1,
        "pass_index": -1,
        "object": f"{object_address:08x}",
        "token": f"{hex_value(input_fields, 'token', phase='input'):02x}",
        "frame_pointer": f"{frame_register:08x}",
        "frame_origin_x": origin_x,
        "frame_origin_y": origin_y,
        "frame_width": frame_width,
        "frame_height": frame_height,
        "framebuffer_bank": framebuffer_bank,
        "palette_source": f"{palette_source:08x}",
        "horizontal_minimum": horizontal_minimum,
        "horizontal_limit": horizontal_limit,
        "scaled_width": scaled_width,
        "horizontal_clip": horizontal_clip,
        "render_mode": f"{render_mode:02x}",
        "row_callback": f"{row_callback:08x}",
        "packed_data_offset": f"{PACKED_HEADER_BYTES:04x}",
        "source_cursor_offset": f"{source_cursor_offset:04x}",
        "destination_x": destination_x,
        "destination_y": destination_y,
        "x_direction": x_direction,
        "y_direction": -1 if expected_registers["s2"] > 0 else 1,
        "x_scale": f"{x_scale:04x}",
        "x_remainder": f"{x_remainder:04x}",
        "y_accumulator": f"{y_accumulator:04x}",
        "y_scale": f"{y_scale:04x}",
        "output_rows": output_rows,
        "first_palette_index": first_palette_index,
        "milestones": "",
    }


def assign_timeline(rows: list[dict[str, object]]) -> None:
    """Assign framebuffer-defined ticks, pass order, lifetimes, and landmarks."""
    tick = -1
    prior_bank: int | None = None
    pass_index = 0
    ticks: dict[int, list[int]] = {}
    for index, row in enumerate(rows):
        bank = int(row["framebuffer_bank"])
        if prior_bank is None or bank != prior_bank:
            tick += 1
            pass_index = 0
        else:
            pass_index += 1
        row["tick"] = tick
        row["pass_index"] = pass_index
        ticks.setdefault(tick, []).append(index)
        prior_bank = bank

    labels: dict[int, list[str]] = {index: [] for index in range(len(rows))}
    labels[0].append("first_visible")

    right_clips = [
        index for index, row in enumerate(rows) if row["horizontal_clip"] == "right"
    ]
    unexpected_clips = [
        index
        for index, row in enumerate(rows)
        if row["horizontal_clip"] not in {"none", "right"}
    ]
    if unexpected_clips:
        raise TraceError("controlled trace contains an unexpected horizontal clip side")
    if len(right_clips) != 54:
        raise TraceError(f"expected 54 right-clipped calls, observed {len(right_clips)}")
    labels[right_clips[0]].append("first_right_clip")

    maximum_overlap = max(len(indices) for indices in ticks.values())
    if maximum_overlap != 6:
        raise TraceError(f"expected a six-particle peak, observed {maximum_overlap}")
    peak_tick = next(tick_id for tick_id, indices in ticks.items() if len(indices) == 6)
    for index in ticks[peak_tick]:
        labels[index].append("six_particle_peak")

    downscale_index = next(
        (index for index, row in enumerate(rows) if int(str(row["x_scale"]), 16) < 0x1000),
        None,
    )
    if downscale_index is None:
        raise TraceError("trace contains no horizontal downscale call")
    labels[downscale_index].append("first_horizontal_downscale")

    # A slot begins a new lifetime after an absent tick or when its animation
    # token wraps to an earlier value without an intervening visible frame.
    lifetime_for_index: dict[int, int] = {}
    active: dict[str, tuple[int, int, int]] = {}
    next_lifetime = 0
    for tick_id in sorted(ticks):
        for index in ticks[tick_id]:
            row = rows[index]
            object_id = str(row["object"])
            token = int(str(row["token"]), 16)
            prior = active.get(object_id)
            if prior is None or prior[0] != tick_id - 1 or token < prior[1]:
                lifetime = next_lifetime
                next_lifetime += 1
            else:
                lifetime = prior[2]
            lifetime_for_index[index] = lifetime
            active[object_id] = (tick_id, token, lifetime)

    if next_lifetime != 7:
        raise TraceError(f"expected seven particle lifetimes, observed {next_lifetime}")
    last_by_lifetime: dict[int, int] = {}
    for index, lifetime in lifetime_for_index.items():
        last_by_lifetime[lifetime] = index
    for lifetime, index in sorted(last_by_lifetime.items()):
        labels[index].append(f"last_visible_lifetime_{lifetime}")

    for index, row in enumerate(rows):
        row["milestones"] = ";".join(labels[index])


def analyze(lines: Iterable[str]) -> list[dict[str, object]]:
    calls = parse_trace(lines)
    rows = [derive_row(call) for call in calls]
    assign_timeline(rows)
    return rows


def write_csv(rows: list[dict[str, object]], handle: TextIO) -> None:
    handle.write(
        "# Derived from live KI 1.5d R4600 registers; contains no ROM, frame, or palette bytes.\n"
    )
    writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)


def derive_bridge_row(
    call: TraceCall, row: dict[str, object]
) -> dict[str, object]:
    """Join live record inputs to one independently derived native result."""
    input_fields = call.phases["input"]
    first_row = call.phases["first_row"]
    surface = call.phases["surface"]
    scale_x, scale_y = tuple_hex(input_fields, "scale58", 2, phase="input")
    pixel_step = s8(hex_value(input_fields, "byte24", phase="input"))
    source_cursor_offset = int(str(row["source_cursor_offset"]), 16)
    if source_cursor_offset <= PACKED_HEADER_BYTES:
        raise TraceError(
            f"call {call.call_id:X}: first-row cursor does not follow its size byte"
        )

    return {
        "call": row["call"],
        "tick": row["tick"],
        "pass_index": row["pass_index"],
        "milestones": row["milestones"] or "-",
        "record_address": row["object"],
        "token": row["token"],
        "pixel_step": pixel_step,
        "frame_address": row["frame_pointer"],
        "record_scale_x": f"{scale_x:04x}",
        "record_scale_y": f"{scale_y:04x}",
        "word68": f"{hex_value(input_fields, 'word68', phase='input'):08x}",
        "word6c": f"{hex_value(input_fields, 'word6c', phase='input'):08x}",
        "word70": f"{hex_value(input_fields, 'word70', phase='input'):08x}",
        "facing": s8(hex_value(input_fields, "facing8c", phase="input")),
        "display_class": f"{hex_value(input_fields, 'display94', phase='input'):02x}",
        "flags": f"{hex_value(input_fields, 'flags95', phase='input'):02x}",
        "horizontal_minimum": row["horizontal_minimum"],
        "horizontal_limit": row["horizontal_limit"],
        "framebuffer_select": hex_value(surface, "fbselect_register", phase="surface"),
        "viewport_height": hex_value(input_fields, "viewport", phase="input"),
        "frame_origin_x": row["frame_origin_x"],
        "frame_origin_y": row["frame_origin_y"],
        "frame_width": row["frame_width"],
        "frame_height": row["frame_height"],
        "skipped_source_rows": hex_value(
            first_row, "skipped_source_rows", phase="first_row"
        ),
        "skipped_source_bytes": hex_value(
            first_row, "skipped_source_bytes", phase="first_row"
        ),
        "palette_source": row["palette_source"],
        "framebuffer_address": (
            f"{FRAMEBUFFER_BASE + int(row['framebuffer_bank']) * FRAMEBUFFER_BANK_BYTES:08x}"
        ),
        "scaled_width": row["scaled_width"],
        "horizontal_clip": row["horizontal_clip"],
        "render_mode": row["render_mode"],
        "row_callback": row["row_callback"],
        "destination_x": row["destination_x"],
        "destination_y": row["destination_y"],
        "x_direction": row["x_direction"],
        "y_direction": row["y_direction"],
        "x_scale": row["x_scale"],
        "x_remainder": row["x_remainder"],
        "y_accumulator": row["y_accumulator"],
        "y_scale": row["y_scale"],
        # The native renderer wants the selected row's size byte. MAME's
        # first-row breakpoint logs the cursor one byte after that size.
        "source_offset": f"{source_cursor_offset - 1:04x}",
        "output_rows": row["output_rows"],
        # Both bytes feed the mode-0x60 palette path. They are retained in
        # the fixture even though every observed Endokuken call contains 0.
        "field8e": f"{hex_value(input_fields, 'palette8e', phase='input'):02x}",
        "field97": f"{hex_value(input_fields, 'aux97', phase='input'):02x}",
    }


def bridge_rows(
    calls: list[TraceCall], rows: list[dict[str, object]]
) -> list[dict[str, object]]:
    if len(calls) != len(rows):
        raise TraceError("bridge input/output row counts differ")
    return [derive_bridge_row(call, row) for call, row in zip(calls, rows)]


def write_bridge_csv(rows: list[dict[str, object]], handle: TextIO) -> None:
    handle.write(
        "# Scalar KI 1.5d guest state and verified results; contains no frame or palette bytes.\n"
    )
    writer = csv.DictWriter(handle, fieldnames=BRIDGE_CSV_FIELDS, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)


def load_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        lines = (line for line in handle if not line.startswith("#"))
        reader = csv.DictReader(lines)
        if tuple(reader.fieldnames or ()) != CSV_FIELDS:
            raise TraceError(f"fixture {path} has an unexpected header")
        return list(reader)


def load_bridge_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        lines = (line for line in handle if not line.startswith("#"))
        reader = csv.DictReader(lines)
        if tuple(reader.fieldnames or ()) != BRIDGE_CSV_FIELDS:
            raise TraceError(f"bridge fixture {path} has an unexpected header")
        return list(reader)


def compare_fixture(rows: list[dict[str, object]], fixture: Path) -> None:
    expected_rows = load_csv(fixture)
    if len(rows) != len(expected_rows):
        raise TraceError(
            f"fixture row count differs: trace has {len(rows)}, fixture has {len(expected_rows)}"
        )
    for index, (actual, expected) in enumerate(zip(rows, expected_rows)):
        normalized = {name: str(actual[name]) for name in CSV_FIELDS}
        if normalized != expected:
            differing = next(name for name in CSV_FIELDS if normalized[name] != expected[name])
            raise TraceError(
                f"fixture mismatch at row {index}, field {differing}: "
                f"got {normalized[differing]!r}, expected {expected[differing]!r}"
            )


def compare_bridge_fixture(rows: list[dict[str, object]], fixture: Path) -> None:
    expected_rows = load_bridge_csv(fixture)
    if len(rows) != len(expected_rows):
        raise TraceError(
            "bridge fixture row count differs: "
            f"trace has {len(rows)}, fixture has {len(expected_rows)}"
        )
    for index, (actual, expected) in enumerate(zip(rows, expected_rows)):
        normalized = {name: str(actual[name]) for name in BRIDGE_CSV_FIELDS}
        if normalized != expected:
            differing = next(
                name
                for name in BRIDGE_CSV_FIELDS
                if normalized[name] != expected[name]
            )
            raise TraceError(
                f"bridge fixture mismatch at row {index}, field {differing}: "
                f"got {normalized[differing]!r}, expected {expected[differing]!r}"
            )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="raw MAME -oslog output")
    parser.add_argument("--fixture", type=Path, help="compare against a normalized CSV")
    parser.add_argument("--write-fixture", type=Path, help="write the normalized CSV")
    parser.add_argument(
        "--bridge-fixture", type=Path, help="compare against the native bridge CSV"
    )
    parser.add_argument(
        "--write-bridge-fixture", type=Path, help="write the native bridge CSV"
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    selected_outputs = sum(
        option is not None
        for option in (
            args.fixture,
            args.write_fixture,
            args.bridge_fixture,
            args.write_bridge_fixture,
        )
    )
    if selected_outputs > 1:
        print("choose only one fixture read/write option", file=sys.stderr)
        return 2
    try:
        with args.trace.open("r", encoding="utf-8-sig") as handle:
            calls = parse_trace(handle)
        rows = [derive_row(call) for call in calls]
        assign_timeline(rows)
        if args.fixture:
            compare_fixture(rows, args.fixture)
        if args.write_fixture:
            args.write_fixture.parent.mkdir(parents=True, exist_ok=True)
            with args.write_fixture.open("w", encoding="utf-8", newline="") as handle:
                write_csv(rows, handle)
        native_rows = bridge_rows(calls, rows)
        if args.bridge_fixture:
            compare_bridge_fixture(native_rows, args.bridge_fixture)
        if args.write_bridge_fixture:
            args.write_bridge_fixture.parent.mkdir(parents=True, exist_ok=True)
            with args.write_bridge_fixture.open(
                "w", encoding="utf-8", newline=""
            ) as handle:
                write_bridge_csv(native_rows, handle)
        print(
            f"verified {len(rows)} renderer calls across "
            f"{int(rows[-1]['tick']) + 1} displayed ticks"
        )
        return 0
    except (OSError, TraceError) as error:
        print(f"state-to-render verification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
