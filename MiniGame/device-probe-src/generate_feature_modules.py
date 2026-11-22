"""Writes the tiny WebAssembly feature-test modules used by the WeChat device probe.

WXWebAssembly.instantiate only accepts a path inside the code package, so every feature needs its own .wasm
file; bytes cannot be compiled at runtime. Each module exports f() returning 42 unless noted otherwise.

Usage: python generate_feature_modules.py <output_dir>
"""
import os
import struct
import sys

MAGIC = b"\0asm\x01\0\0\0"
I32, I64 = 0x7F, 0x7E
VOID_TO_I32 = bytes([0x60, 0x00, 0x01, I32])


def uleb(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def section(section_id: int, payload: bytes) -> bytes:
    return bytes([section_id]) + uleb(len(payload)) + payload


def vec(items) -> bytes:
    items = list(items)
    return uleb(len(items)) + b"".join(items)


def export(name: str, kind: int, index: int) -> bytes:
    raw = name.encode()
    return uleb(len(raw)) + raw + bytes([kind]) + uleb(index)


def body(code: bytes) -> bytes:
    full = b"\x00" + code + b"\x0b"
    return uleb(len(full)) + full


def module(types, functions, codes, exports, memory=None, tag=None) -> bytes:
    out = MAGIC + section(1, vec(types)) + section(3, vec(uleb(f) for f in functions))
    if memory is not None:
        out += section(5, vec([memory]))
    if tag is not None:
        out += section(13, vec([tag]))
    out += section(7, vec(exports)) + section(10, vec(body(c) for c in codes))
    return out


def simple(code: bytes, memory=None, tag=None, extra_types=()) -> bytes:
    return module([VOID_TO_I32, *extra_types], [0], [code], [export("f", 0, 0)], memory, tag)


PAGE_1 = b"\x00\x01"  # one page, no maximum
SHARED_1 = b"\x03\x01\x01"  # shared, one page minimum and maximum
MEMORY64_1 = b"\x04\x01"  # memory64, one page

MODULES = {
    # i32x4.extract_lane 3 of v128.const
    "simd128": simple(b"\xfd\x0c" + bytes(12) + struct.pack("<i", 42) + b"\xfd\x1b\x03"),
    # memory.fill, then read the first byte back
    "bulk-memory": simple(b"\x41\x00\x41\x2a\x41\x04\xfc\x0b\x00\x41\x00\x2d\x00\x00", memory=PAGE_1),
    # i32.extend8_s
    "sign-ext": simple(b"\x41\x2a\xc0"),
    # f32.const 42.0, i32.trunc_sat_f32_s
    "nontrapping-fptoint": simple(b"\x43" + struct.pack("<f", 42.0) + b"\xfc\x00"),
    # f() calls g() returning (0, 42) and keeps the second value
    "multi-value": module(
        [VOID_TO_I32, bytes([0x60, 0x00, 0x02, I32, I32])],
        [0, 1],
        [b"\x10\x01\x21\x00\x1a\x20\x00".replace(b"\x21\x00\x1a\x20\x00", b""), b"\x41\x00\x41\x2a"],
        [export("f", 0, 0)],
    ) if False else module(
        [VOID_TO_I32, bytes([0x60, 0x00, 0x02, I32, I32])],
        [0, 1],
        [b"", b"\x41\x00\x41\x2a"],
        [export("f", 0, 0)],
    ) if False else b"",
    # return_call into g()
    "tail-call": module([VOID_TO_I32], [0, 0], [b"\x12\x01", b"\x41\x2a"], [export("f", 0, 0)]),
    # try / throw / catch with the legacy exception encoding; the tag type is () -> ()
    "exceptions": simple(
        b"\x06\x7f\x08\x00\x07\x00\x41\x2a\x0b",
        tag=b"\x00\x01",
        extra_types=[bytes([0x60, 0x00, 0x00])],
    ),
    # memory.size on a 64-bit memory, then a constant
    "memory64": simple(b"\x3f\x00\x1a\x41\x2a", memory=MEMORY64_1),
    # i32.atomic.rmw.add then i32.atomic.load on a shared memory
    "threads": simple(b"\x41\x00\x41\x2a\xfe\x1e\x02\x00\x1a\x41\x00\xfe\x10\x02\x00", memory=SHARED_1),
    # f(i64) -> i64 identity: needs the JavaScript BigInt integration MEMORY64 relies on
    "i64-bigint": module(
        [bytes([0x60, 0x01, I64, 0x01, I64])], [0], [b"\x20\x00"], [export("f", 0, 0)]
    ),
}

# multi-value needs a local to keep the second result, which module() does not model, so it is written out here:
# g() -> (i32, i32) returns (0, 42); f() -> i32 stores the top value in local 0, drops the rest and returns it.
_multi_f = b"\x01\x01\x7f" + b"\x10\x01\x21\x00\x1a\x20\x00\x0b"
_multi_g = b"\x00\x41\x00\x41\x2a\x0b"
MODULES["multi-value"] = (
    MAGIC
    + section(1, vec([VOID_TO_I32, bytes([0x60, 0x00, 0x02, I32, I32])]))
    + section(3, vec([uleb(0), uleb(1)]))
    + section(7, vec([export("f", 0, 0)]))
    + section(10, vec([uleb(len(_multi_f)) + _multi_f, uleb(len(_multi_g)) + _multi_g]))
)


def memory_probe() -> bytes:
    """grow(pages) -> old size; touch(first_page, page_count) writes one byte per 4 KiB to commit the pages."""
    grow = b"\x20\x00\x40\x00"
    # locals: 2 = address, 3 = end
    touch_body = (
        b"\x01\x02\x7f"  # two i32 locals
        + b"\x20\x00\x41\x10\x74\x22\x02"  # addr = first << 16 (tee)
        + b"\x20\x01\x41\x10\x74\x6a\x21\x03"  # end = addr + (count << 16)
        + b"\x02\x40\x03\x40"  # block, loop
        + b"\x20\x02\x20\x03\x4f\x0d\x01"  # br_if out when addr >= end
        + b"\x20\x02\x41\x01\x3a\x00\x00"  # i32.store8
        + b"\x20\x02\x41\x80\x20\x6a\x21\x02"  # addr += 4096
        + b"\x0c\x00\x0b\x0b"  # br loop; end; end
        + b"\x0b"
    )
    # Two exported functions and an exported growable memory (initial 16 pages, maximum 65536 pages = 4 GiB).
    types = [bytes([0x60, 0x01, I32, 0x01, I32]), bytes([0x60, 0x02, I32, I32, 0x00])]
    return (
        MAGIC
        + section(1, vec(types))
        + section(3, vec([uleb(0), uleb(1)]))
        + section(5, vec([b"\x01\x10" + uleb(65536)]))
        + section(7, vec([export("grow", 0, 0), export("touch", 0, 1), export("memory", 2, 0)]))
        + section(10, vec([body(grow), uleb(len(touch_body)) + touch_body]))
    )


def main() -> None:
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    for name, data in MODULES.items():
        with open(os.path.join(out_dir, name + ".wasm"), "wb") as handle:
            handle.write(data)
    with open(os.path.join(out_dir, "memory-probe.wasm"), "wb") as handle:
        handle.write(memory_probe())
    print(f"wrote {len(MODULES) + 1} modules to {out_dir}")


if __name__ == "__main__":
    main()
