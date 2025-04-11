import argparse
import configparser
import csv
import os
import struct
import shutil
from datetime import datetime

try:
    from colorama import Fore, init
    init(autoreset=True)
except ImportError:
    class DummyColor:
        RED = GREEN = YELLOW = CYAN = RESET = ''
    Fore = DummyColor()

def load_partitions(csv_path):
    partitions = []
    with open(csv_path, 'r') as f:
        reader = csv.reader(line for line in f if not line.strip().startswith('#') and line.strip())
        for row in reader:
            if len(row) < 5:
                continue
            name, type_, subtype, offset, size, *_ = [x.strip() for x in row]
            try:
                partitions.append({
                    "name": name,
                    "type": type_,
                    "subtype": subtype,
                    "offset": int(offset, 0),
                    "size": int(size, 0)
                })
            except ValueError:
                continue
    return partitions

def parse_efu_header(data):
    if data[:4] != b'EFU\x00':
        return None, None, "Invalid EFU signature"
    version = struct.unpack('<H', data[4:6])[0]
    return version, 6, None

def detect_partition_csv(project_dir, env_name):
    ini_path = os.path.join(project_dir, "platformio.ini")
    user_ini_path = os.path.join(project_dir, "platformio_user.ini")
    config = configparser.ConfigParser()
    config.read([ini_path, user_ini_path])
    section = f'env:{env_name}'
    if section not in config:
        raise RuntimeError(f"Missing section: {section} in either INI file")
    partition_path = config[section].get('board_build.partitions', fallback=None)
    if not partition_path:
        print(f"⚠️  'board_build.partitions' not found, using default: ESP32_partitions.csv")
        fallback_csv = os.path.join(project_dir, "ESP32_partitions.csv")
        if not os.path.isfile(fallback_csv):
            raise FileNotFoundError("Partition file not found: ESP32_partitions.csv")
        return fallback_csv
    csv_path = os.path.join(project_dir, partition_path.strip())
    if not os.path.isfile(csv_path):
        alt_path = os.path.join(project_dir, "boards", os.path.basename(partition_path))
        if os.path.isfile(alt_path):
            print(f"📁 Using fallback from boards/: {alt_path}")
            return alt_path
    if not os.path.isfile(csv_path):
        raise FileNotFoundError(f"Partition file not found: {csv_path}")
    return csv_path

def validate_efu(efu_path, partitions_csv):
    with open(efu_path, 'rb') as f:
        data = f.read()
    version, offset, err = parse_efu_header(data)
    if err:
        return [f"{Fore.RED}❌ {err}"], None
    partitions = load_partitions(partitions_csv)
    app0 = next((p for p in partitions if p['name'] == 'app0'), None)
    spiffs = next((p for p in partitions if p['name'] == 'spiffs'), None)
    if not app0 or not spiffs:
        return [f"{Fore.RED}❌ Missing required partitions: app0 or spiffs"], version
    results = []
    record_index = 0
    sketch_size = fs_size = None
    while offset + 6 <= len(data):
        rtype = struct.unpack('<H', data[offset:offset+2])[0]
        rlen = struct.unpack('<I', data[offset+2:offset+6])[0]
        offset += 6
        if offset + rlen > len(data):
            results.append(f"{Fore.RED}❌ Record {record_index} size exceeds file bounds.")
            break
        if rtype == 0x0001:
            sketch_size = rlen
        elif rtype == 0x0002:
            fs_size = rlen
        else:
            results.append(f"{Fore.YELLOW}⚠️ Unknown record type: 0x{rtype:04X}")
        offset += rlen
        record_index += 1
    if sketch_size:
        if sketch_size > app0['size']:
            results.append(f"{Fore.RED}❌ Sketch size {sketch_size} > app0 partition ({app0['size']})")
        else:
            results.append(f"{Fore.GREEN}✅ Sketch OK ({sketch_size} / {app0['size']})")
    if fs_size:
        if fs_size > spiffs['size']:
            results.append(f"{Fore.RED}❌ FS size {fs_size} > spiffs partition ({spiffs['size']})")
        else:
            results.append(f"{Fore.GREEN}✅ FS OK ({fs_size} / {spiffs['size']})")
    if offset != len(data):
        results.append(f"{Fore.YELLOW}⚠️ EFU trailing data after last record: {len(data)-offset} bytes")
    return results, version

def main():
    parser = argparse.ArgumentParser(description="EFU Validator (no zip)")
    parser.add_argument('--efu', required=True)
    parser.add_argument('--project', required=True)
    parser.add_argument('--env', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    partition_csv = detect_partition_csv(args.project, args.env)
    print(f"🔍 Validating EFU: {args.efu}\n📁 Partitions: {partition_csv}\n")
    results, version = validate_efu(args.efu, partition_csv)
    for line in results:
        print(line)
    if any("❌" in line for line in results):
        print(f"\n{Fore.RED}Build failed: EFU is invalid")
        return 1
    timestamp = datetime.now().strftime('%Y%m%d')
    basename = os.path.basename(args.efu).replace('.efu', '')
    new_name = f"{basename}_{timestamp}.efu"
    os.makedirs(args.output, exist_ok=True)
    final_path = os.path.join(args.output, new_name)
    shutil.copy2(args.efu, final_path)
    print(f"\n{Fore.GREEN}✅ EFU validated and copied to: {final_path}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
