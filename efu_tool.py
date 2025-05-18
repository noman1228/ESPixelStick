# efu_tool.py - Jason Theriault 
# with tons of code stolen from original ESPixelStick (EFU build + validate only)
# 
import argparse
import configparser
import csv
import os
import struct
import shutil
import sys
from datetime import datetime
from pathlib import Path

# Optional color output (fallback to plain if unavailable)
try:
    from colorama import Fore, Style, init
    init(autoreset=True)
except ImportError:
    print("⚠️  colorama not found — output will be uncolored.")
    class _Dummy:
        RED = GREEN = YELLOW = CYAN = MAGENTA = RESET_ALL = ""
    Fore = Style = _Dummy()


def load_partitions(csv_path):
    """Parse partition CSV file"""
    partitions = []
    
    if not os.path.exists(csv_path):
        print(f"{Fore.RED}❌ Partition file not found: {csv_path}{Style.RESET_ALL}")
        return []
        
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
    """Parse EFU file header"""
    if data[:4] != b'EFU\x00':
        return None, None, "Invalid EFU signature"
    version = struct.unpack('<H', data[4:6])[0]
    return version, 6, None


def detect_partition_csv(project_dir, env_name):
    """Find partition CSV file from project settings"""
    ini_path = os.path.join(project_dir, "platformio.ini")
    user_ini_path = os.path.join(project_dir, "platformio_user.ini")

    config = configparser.ConfigParser()
    files_read = config.read([ini_path, user_ini_path])
    if not files_read:
        print(f"{Fore.YELLOW}⚠️ No platformio.ini or platformio_user.ini found{Style.RESET_ALL}")
    
    # Try environment-specific section first
    section = f'env:{env_name}'
    if section not in config:
        print(f"{Fore.YELLOW}⚠️ Missing section: {section} in INI file, trying global config{Style.RESET_ALL}")
        section = 'env'  # fallback to global env settings

    # Look for partition file in config
    partition_path = None
    if section in config:
        partition_path = config[section].get('board_build.partitions', None)
    
    if not partition_path:
        print(f"{Fore.YELLOW}⚠️ 'board_build.partitions' not found, trying defaults{Style.RESET_ALL}")
        
        # Check common locations
        fallback_paths = [
            os.path.join(project_dir, "ESP32_partitions.csv"),
            os.path.join(project_dir, "partitions.csv"),
            os.path.join(project_dir, "boards", "partitions.csv"),
        ]
        
        for path in fallback_paths:
            if os.path.isfile(path):
                print(f"{Fore.GREEN}✅ Found partition file: {path}{Style.RESET_ALL}")
                return path
                
        raise FileNotFoundError(f"{Fore.RED}❌ No partition file found in any standard location{Style.RESET_ALL}")

    # Try relative to project root first
    csv_path = os.path.join(project_dir, partition_path.strip())
    if os.path.isfile(csv_path):
        return csv_path
        
    # Try in boards directory
    alt_path = os.path.join(project_dir, "boards", os.path.basename(partition_path))
    if os.path.isfile(alt_path):
        print(f"{Fore.GREEN}✅ Using fallback from boards/: {alt_path}{Style.RESET_ALL}")
        return alt_path

    # Try absolute path as last resort
    if os.path.isfile(partition_path):
        return partition_path

    raise FileNotFoundError(f"{Fore.RED}❌ Partition file not found: {partition_path}{Style.RESET_ALL}")


def validate_efu(efu_path, partitions_csv):
    """Validate EFU file against partition constraints"""
    if not os.path.exists(efu_path):
        return [f"{Fore.RED}❌ EFU file not found: {efu_path}{Style.RESET_ALL}"], None
        
    with open(efu_path, 'rb') as f:
        data = f.read()

    version, offset, err = parse_efu_header(data)
    if err:
        return [f"{Fore.RED}❌ {err}{Style.RESET_ALL}"], None

    partitions = load_partitions(partitions_csv)
    app0 = next((p for p in partitions if p['name'] == 'app0'), None)
    spiffs = next((p for p in partitions if p['name'] == 'spiffs'), None)

    if not app0 or not spiffs:
        return [f"{Fore.RED}❌ Missing required partitions: app0 or spiffs{Style.RESET_ALL}"], version

    results = []
    record_index = 0
    sketch_size = fs_size = None

    # Add file info
    results.append(f"{Fore.CYAN}EFU File: {os.path.basename(efu_path)} ({len(data) // 1024} KB){Style.RESET_ALL}")
    results.append(f"{Fore.CYAN}EFU Version: {version}{Style.RESET_ALL}")

    while offset + 6 <= len(data):
        rtype = struct.unpack('>H', data[offset:offset+2])[0]
        rlen = struct.unpack('>I', data[offset+2:offset+6])[0]
        offset += 6

        if offset + rlen > len(data):
            results.append(f"{Fore.RED}❌ Record {record_index} size exceeds file bounds.{Style.RESET_ALL}")
            break

        if rtype == 0x0001:
            sketch_size = rlen
            results.append(f"{Fore.CYAN}Sketch Size: {sketch_size // 1024} KB{Style.RESET_ALL}")
        elif rtype == 0x0002:
            fs_size = rlen
            results.append(f"{Fore.CYAN}Filesystem Size: {fs_size // 1024} KB{Style.RESET_ALL}")
        else:
            results.append(f"{Fore.YELLOW}⚠️ Unknown record type: 0x{rtype:04X}{Style.RESET_ALL}")

        offset += rlen
        record_index += 1

    # Validate against partition table
    if sketch_size:
        if sketch_size > app0['size']:
            results.append(f"{Fore.RED}❌ Sketch size {sketch_size // 1024} KB > app0 partition ({app0['size'] // 1024} KB){Style.RESET_ALL}")
        else:
            percent = sketch_size / app0['size'] * 100
            results.append(f"{Fore.GREEN}✅ Sketch OK: {sketch_size // 1024} KB / {app0['size'] // 1024} KB ({percent:.1f}%){Style.RESET_ALL}")

    if fs_size:
        if fs_size > spiffs['size']:
            results.append(f"{Fore.RED}❌ FS size {fs_size // 1024} KB > spiffs partition ({spiffs['size'] // 1024} KB){Style.RESET_ALL}")
        else:
            percent = fs_size / spiffs['size'] * 100
            results.append(f"{Fore.GREEN}✅ FS OK: {fs_size // 1024} KB / {spiffs['size'] // 1024} KB ({percent:.1f}%){Style.RESET_ALL}")

    if offset != len(data):
        results.append(f"{Fore.YELLOW}⚠️ EFU trailing data after last record: {len(data)-offset} bytes{Style.RESET_ALL}")

    return results, version


def main():
    parser = argparse.ArgumentParser(description="EFU Validator and Processor")
    parser.add_argument('--efu', required=True, help="Path to EFU file")
    parser.add_argument('--project', required=True, help="Path to project root")
    parser.add_argument('--env', required=True, help="PIO env name")
    parser.add_argument('--output', required=True, help="Output dir for final .efu")
    parser.add_argument('--install', action='store_true', help="Attempt to install EFU to device")
    args = parser.parse_args()

    try:
        partition_csv = detect_partition_csv(args.project, args.env)
    except FileNotFoundError as e:
        print(str(e))
        return 1

    print(f"\n{Fore.CYAN}====== EFU Validator ======{Style.RESET_ALL}")
    print(f"{Fore.CYAN}EFU File: {args.efu}{Style.RESET_ALL}")
    print(f"{Fore.CYAN}Partitions: {partition_csv}{Style.RESET_ALL}\n")
    
    results, version = validate_efu(args.efu, partition_csv)

    for line in results:
        print(line)

    if any("❌" in line for line in results):
        print(f"\n{Fore.RED}Build failed: EFU is invalid{Style.RESET_ALL}")
        return 1

    # Save copy to firmware/EFU with timestamp
    timestamp = datetime.now().strftime('%Y%m%d')
    basename = os.path.basename(args.efu).replace('.efu', '')
    new_name = f"{basename}_{timestamp}.efu"
    os.makedirs(args.output, exist_ok=True)
    final_path = os.path.join(args.output, new_name)
    shutil.copy2(args.efu, final_path)

    print(f"\n{Fore.GREEN}✅ EFU validated and copied to: {final_path}{Style.RESET_ALL}")
    
    # Attempt to install if requested
    if args.install:
        print(f"\n{Fore.CYAN}====== EFU Installation ======{Style.RESET_ALL}")
        # Future enhancement: Implement automatic upload to ESP device
        print(f"{Fore.YELLOW}⚠️ Auto-installation not yet implemented{Style.RESET_ALL}")
        print("To install manually:")
        print(f"1. Access your ESP device at http://esps-XXXX.local")
        print(f"2. Go to 'File Upload' section")
        print(f"3. Upload file: {final_path}")
        
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as e:
        print(f"\n{Fore.RED}❌ Error: {str(e)}{Style.RESET_ALL}")
        raise SystemExit(1)