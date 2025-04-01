def dump_efu(path, num_bytes=64):
    with open(path, "rb") as f:
        data = f.read(num_bytes)

    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_bytes = " ".join(f"{b:02X}" for b in chunk)
        ascii = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        print(f"{i:04X}  {hex_bytes:<48}  {ascii}")

# Replace with your actual EFU path
efu_path = ".pio/build/esp32_tiny/esp32_tiny.efu"
dump_efu(efu_path)
