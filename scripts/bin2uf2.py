# Wraps an ESP32-S3 app image as UF2 for the TinyUF2 bootloader (base 0x0 = start of the app slot).
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
data = open(src, "rb").read()
FAMILY = 0xC47E5767  # ESP32-S3
blocks = [data[i:i + 256] for i in range(0, len(data), 256)]
with open(dst, "wb") as f:
    for n, chunk in enumerate(blocks):
        hdr = struct.pack("<8I", 0x0A324655, 0x9E5D5157, 0x00002000, n * 256, 256, n, len(blocks), FAMILY)
        f.write(hdr + chunk.ljust(476, b"\0") + struct.pack("<I", 0x0AB16F30))
print(f"{len(blocks)} blocks -> {dst}")
