#!/usr/bin/env python3
"""Embed a binary file as a C const uint8_t array.

Usage: bin2c.py <input> <output.c> <symbol_name>

The resulting object is placed in `.rodata.<symbol_name>` so it lives
in flash (XIP-mapped, zero RAM cost) on RP2350.
"""
import sys

inp, outp, sym = sys.argv[1], sys.argv[2], sys.argv[3]
with open(inp, "rb") as f:
    data = f.read()

with open(outp, "w") as f:
    f.write("/* Auto-generated from %s — DO NOT EDIT. */\n" % inp)
    f.write("#include <stdint.h>\n")
    f.write("#include <stddef.h>\n\n")
    f.write("__attribute__((section(\".rodata.%s\"), aligned(4)))\n" % sym)
    f.write("const uint8_t %s[%d] = {\n" % (sym, len(data)))
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        f.write("    " + ",".join("0x%02x" % b for b in chunk) + ",\n")
    f.write("};\n\n")
    f.write("const size_t %s_size = %d;\n" % (sym, len(data)))

print("wrote %s (%d bytes, sym=%s)" % (outp, len(data), sym))
