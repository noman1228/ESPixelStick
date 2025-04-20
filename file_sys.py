# file_sys.py - Jason Theriault 2025
# Hacked our way out of a lazy mouse move again
# can't be bothered to move to a few different spots for filesystem
# so let's automate
import os
import sys
import csv
import subprocess
import serial.tools.list_ports
from pathlib import Path
from colorama import Fore, Style

# --- ENVIRONMENT DETECTION ---
def find_platformio_root():
    current_dir = os.getcwd()
    while current_dir:
        if os.path.exists(os.path.join(current_dir, "platformio.ini")):
            return current_dir
        parent_dir = os.path.dirname(current_dir)
        if parent_dir == current_dir:
            break
        current_dir = parent_dir
    return None

def get_last_used_env():
    build_dir = Path(find_platformio_root()) / ".pio" / "build"
    if not build_dir.exists():
        return None
    env_dirs = [d for d in build_dir.iterdir() if d.is_dir()]
    if not env_dirs:
        return None
    env_dirs.sort(key=lambda d: d.stat().st_mtime, reverse=True)
    return env_dirs[0].name

def get_all_envs_from_ini():
    pio_ini = Path(find_platformio_root()) / "platformio.ini"
    envs = []
    if pio_ini.exists():
        with open(pio_ini, "r") as f:
            lines = f.readlines()
            for line in lines:
                if line.startswith("[env:"):
                    env_name = line.split(":")[1].strip(" ]\n")
                    envs.append(env_name)
    return envs

def get_active_env_from_ini():
    pio_ini = Path(find_platformio_root()) / "platformio.ini"
    if not pio_ini.exists():
        return None
    with open(pio_ini, "r") as f:
        lines = f.readlines()
        for line in lines:
            if line.startswith("env_default"):
                return line.split("=")[-1].strip()
    return None

# --- CONFIG ---
PROJECT_DIR = Path(find_platformio_root())
ENV_NAME = get_last_used_env() or get_active_env_from_ini() or "unknown_env"
PARTITIONS_CSV = PROJECT_DIR / "ESP32_partitions.csv"
MYENV_TXT = PROJECT_DIR / "MyEnv.txt"
IMAGE_PATH = PROJECT_DIR / ".pio" / "build" / ENV_NAME / "littlefs.bin"
ESPNOW_TOOL = PROJECT_DIR / ".pio" / "packages" / "tool-esptoolpy" / "esptool.py"
GULP_SCRIPT = PROJECT_DIR / "gulpme.bat"

# --- FUNCTIONS ---
def find_serial_port():
    print("🔍 Searching for available serial ports...")
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if any(chip in port.description for chip in ("USB", "UART", "CP210", "CH340")):
            print(f"{Fore.GREEN}✔ Found port: {port.device}{Style.RESET_ALL}")
            return port.device
    print(f"{Fore.RED}✘ No suitable ESP32 device found. Is it plugged in?{Style.RESET_ALL}")
    sys.exit(1)

def extract_flash_config(env_txt_path):
    if not env_txt_path.exists():
        print(f"{Fore.RED}✘ MyEnv.txt not found: {env_txt_path}{Style.RESET_ALL}")
        sys.exit(1)

    flash_mode = "dio"
    flash_freq = "40m"

    with open(env_txt_path, "r") as f:
        for line in f:
            if "'BOARD_FLASH_MODE'" in line:
                flash_mode = line.split(":")[1].strip().strip("',\"")
            elif "'BOARD_F_FLASH'" in line:
                raw = line.split(":")[1].strip().strip("',\"").rstrip("L")
                freq_hz = int(raw)
                flash_freq = f"{freq_hz // 1000000}m"

    print(f"{Fore.GREEN}✔ Using flash mode: {flash_mode}, flash freq: {flash_freq}{Style.RESET_ALL}")
    return flash_mode, flash_freq

def extract_filesystem_partition(csv_path):
    if not csv_path.exists():
        print(f"{Fore.RED}✘ Partition CSV not found: {csv_path}{Style.RESET_ALL}")
        sys.exit(1)

    with open(csv_path, newline='') as csvfile:
        reader = csv.reader(csvfile)
        for row in reader:
            if not row or row[0].strip().startswith("#"):
                continue
            if len(row) >= 5:
                name, type_, subtype, offset, size = [x.strip().lower() for x in row[:5]]
                if (type_ == "data") and (subtype in ("spiffs", "littlefs")):
                    offset_int = int(offset, 0)
                    size_int = int(size, 0)
                    print(f"{Fore.GREEN}✔ Filesystem partition: '{name}', offset=0x{offset_int:X}, size={size_int // 1024} KB{Style.RESET_ALL}")
                    return offset_int, size_int

    print(f"{Fore.RED}✘ Could not find a SPIFFS or LittleFS partition in {csv_path}{Style.RESET_ALL}")
    sys.exit(1)

def flash_filesystem(port, image_path, offset, flash_mode, flash_freq):
    if not image_path.exists():
        print(f"{Fore.RED}✘ Filesystem image not found: {image_path}{Style.RESET_ALL}")
        sys.exit(1)

    size = image_path.stat().st_size
    print(f"{Fore.YELLOW}📦 Using image: {image_path} ({size} bytes){Style.RESET_ALL}")
    print(f"{Fore.CYAN}🚀 Flashing filesystem image to 0x{offset:X} on {port}...{Style.RESET_ALL}")

    cmd = [
        sys.executable,
        str(ESPNOW_TOOL),
        "--chip", "esp32",
        "--port", port,
        "--baud", "460800",
        "write_flash",
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", "detect",
        f"0x{offset:X}", str(image_path)
    ]

    subprocess.run(cmd, check=True)
    print(f"{Fore.GREEN}✅ Filesystem upload successful!{Style.RESET_ALL}")

def build_littlefs_image(env_name):
    print(f"{Fore.CYAN}🔧 Running: 'pio run -t buildfs -e {env_name}'...{Style.RESET_ALL}")
    subprocess.run(["platformio", "run", "-t", "buildfs", "-e", env_name], check=True)

def run_gulp():
    print(f"{Fore.CYAN}🛠 Running Gulp: {GULP_SCRIPT}{Style.RESET_ALL}")
    if not GULP_SCRIPT.exists():
        print(f"{Fore.RED}✘ gulpme.bat not found at {GULP_SCRIPT}{Style.RESET_ALL}")
        sys.exit(1)

    subprocess.run([str(GULP_SCRIPT)], shell=True, check=True)

    data_dir = PROJECT_DIR / "data"
    if not data_dir.exists():
        print(f"{Fore.RED}✘ Expected output folder 'data/' not found after Gulp run.{Style.RESET_ALL}")
        sys.exit(1)

    print(f"{Fore.GREEN}✔ Gulp finished. Found data directory: {data_dir}{Style.RESET_ALL}")

# --- MAIN ---
if __name__ == "__main__":
    serial_port = find_serial_port()
    run_gulp()
    build_littlefs_image(ENV_NAME)
    fs_offset, fs_size = extract_filesystem_partition(PARTITIONS_CSV)
    flash_mode, flash_freq = extract_flash_config(MYENV_TXT)
    flash_filesystem(serial_port, IMAGE_PATH, fs_offset, flash_mode, flash_freq)
