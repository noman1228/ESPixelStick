import os
import struct

SIGNATURE = b'EFU\x00'
VERSION = 1

RECORD_TYPE = {
    'sketch': 0x01,      # Matches Java RecordType.SKETCH_IMAGE
    'spiffs': 0x02       # Matches Java RecordType.SPIFFS_IMAGE
}

def write_record(f_out, record_type, path):
    with open(path, 'rb') as f_in:
        data = f_in.read()
        f_out.write(struct.pack('>H', record_type))  # ← big-endian
        f_out.write(struct.pack('>I', len(data)))    # ← big-endian for length too

        f_out.write(data)

def make_efu(sketch_bin, spiffs_bin, output_efu):
    if not os.path.exists(sketch_bin):
        raise FileNotFoundError(f"Sketch binary not found: {sketch_bin}")
    if not os.path.exists(spiffs_bin):
        raise FileNotFoundError(f"Filesystem binary not found: {spiffs_bin}")

    with open(output_efu, 'wb') as f:
        f.write(SIGNATURE)
        f.write(struct.pack('<H', VERSION))

        print(f"[EFU] Adding sketch: {sketch_bin}")
        write_record(f, RECORD_TYPE['sketch'], sketch_bin)

        print(f"[EFU] Adding filesystem: {spiffs_bin}")
        write_record(f, RECORD_TYPE['spiffs'], spiffs_bin)

        print(f"[EFU] Created: {output_efu}")
