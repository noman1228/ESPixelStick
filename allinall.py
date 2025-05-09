import os
import sys
import csv
import time
import subprocess
import serial.tools.list_ports
from pathlib import Path
from colorama import Fore, Style
import serial
# --- CONFIG ---
PROJECT_DIR = Path(os.path.join(os.getcwd(), "file_sys.py")).parent
ENV_NAME = "esp32_bigboi"
PARTITIONS_CSV = PROJECT_DIR / "ESP32_partitions.csv"
MYENV_TXT = PROJECT_DIR / "MyEnv.txt"
BUILD_DIR = PROJECT_DIR / ".pio" / "build" / ENV_NAME
IMAGE_PATH = BUILD_DIR / "littlefs.bin"
ESPNOW_TOOL = PROJECT_DIR / ".pio" / "packages" / "tool-esptoolpy" / "esptool.py"
GULP_SCRIPT = PROJECT_DIR / "gulpme.bat"

BOOTLOADER_BIN = BUILD_DIR / "bootloader.bin"
PARTITIONS_BIN = BUILD_DIR / "partitions.bin"
FIRMWARE_BIN = BUILD_DIR / "firmware.bin"
GULP_STAMP_FILE = PROJECT_DIR / ".gulp_stamp"

# --- FUNCTIONS ---
import psutil  # Add this to the top if not already

def kill_serial_monitors():
    print(f"{Fore.YELLOW}⚠ Closing any open serial monitor processes...{Style.RESET_ALL}")
    try:
        current_pid = os.getpid()

        for proc in psutil.process_iter(['pid', 'name']):
            name = proc.info['name']
            pid = proc.info['pid']
            if name and pid != current_pid and name.lower() in ("platformio.exe", "platformio-terminal.exe"):
                subprocess.run(["taskkill", "/f", "/pid", str(pid)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        print(f"{Fore.GREEN}✔ Any known PlatformIO monitors closed.{Style.RESET_ALL}")
    except Exception as e:
        print(f"{Fore.RED}✘ Could not close serial monitor: {e}{Style.RESET_ALL}")

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
    flash_freq = "80m"

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

def check_build_artifacts():
    required = [BOOTLOADER_BIN, PARTITIONS_BIN, FIRMWARE_BIN, IMAGE_PATH]
    for f in required:
        if not f.exists():
            print(f"{Fore.RED}✘ Missing file: {f}{Style.RESET_ALL}")
            sys.exit(1)

def build_all(env_name):
    print(f"{Fore.CYAN}🔨 Building firmware and filesystem...{Style.RESET_ALL}")
    subprocess.run(["platformio", "run", "-e", env_name], check=True)
    subprocess.run(["platformio", "run", "-t", "buildfs", "-e", env_name], check=True)

def run_gulp_if_needed():
    today = time.strftime("%Y-%m-%d")

    # If the stamp file exists and matches today's date, skip Gulp
    if GULP_STAMP_FILE.exists():
        with open(GULP_STAMP_FILE, "r") as f:
            last_run = f.read().strip()
            if last_run == today:
                print(f"{Fore.YELLOW}⏭ Gulp already run today ({today}), skipping...{Style.RESET_ALL}")
                return

    # Otherwise, run Gulp
    print(f"{Fore.CYAN}🛠 Running Gulp: {GULP_SCRIPT}{Style.RESET_ALL}")
    if not GULP_SCRIPT.exists():
        print(f"{Fore.RED}✘ gulpme.bat not found at {GULP_SCRIPT}{Style.RESET_ALL}")
        sys.exit(1)

    subprocess.run([str(GULP_SCRIPT)], shell=True, check=True)

    data_dir = PROJECT_DIR / "data"
    if not data_dir.exists():
        print(f"{Fore.RED}✘ Expected output folder 'data/' not found after Gulp run.{Style.RESET_ALL}")
        sys.exit(1)

    with open(GULP_STAMP_FILE, "w") as f:
        f.write(today)

    print(f"{Fore.GREEN}✔ Gulp finished. Found data directory: {data_dir}{Style.RESET_ALL}")

def enter_bootloader(port):
    print(f"{Fore.CYAN}⏎ Forcing board into bootloader mode on {port}...{Style.RESET_ALL}")
    try:
        with serial.Serial(port, 115200) as ser:
            ser.dtr = False  # EN = High
            ser.rts = True   # IO0 = Low
            time.sleep(0.1)
            ser.dtr = True   # EN = Low -> reset
            time.sleep(0.1)
            ser.dtr = False  # EN = High
            time.sleep(0.2)
        print(f"{Fore.GREEN}✔ Bootloader mode triggered.{Style.RESET_ALL}")
    except Exception as e:
        print(f"{Fore.RED}✘ Could not trigger bootloader: {e}{Style.RESET_ALL}")
def launch_serial_monitor(port, baud=115200):
    print(f"{Fore.CYAN}📡 Launching serial monitor on {port}...{Style.RESET_ALL}")
    try:
        subprocess.run(["platformio", "device", "monitor", "--port", port, "--baud", str(baud)])
    except Exception as e:
        print(f"{Fore.RED}✘ Could not launch serial monitor: {e}{Style.RESET_ALL}")

def flash_all_images(port, flash_mode, flash_freq, fs_offset, max_retries=1, retry_delay=10):
    print(f"{Fore.CYAN}🚀 Flashing all firmware and filesystem images to {port}...{Style.RESET_ALL}")

    cmd = [
        sys.executable,
        str(ESPNOW_TOOL),
        "--chip", "esp32",
        "--port", port,
        "--baud", "460800",
        "write_flash",
        "--flash_mode", "keep",
        "--flash_freq", flash_freq,
        "--flash_size", "detect",
        "0x1000", str(BOOTLOADER_BIN),
        "0x8000", str(PARTITIONS_BIN),
        "0x10000", str(FIRMWARE_BIN),
        f"0x{fs_offset:X}", str(IMAGE_PATH)
    ]

    attempt = 0
    while attempt <= max_retries:
        try:
            subprocess.run(cmd, check=True)
            print(f"{Fore.GREEN}✅ Full flash complete!{Style.RESET_ALL}")
            return
        except subprocess.CalledProcessError as e:
            print(f"{Fore.RED}✘ Flash attempt {attempt + 1} failed with return code {e.returncode}.{Style.RESET_ALL}")
            if attempt < max_retries:
                print(f"{Fore.YELLOW}🔁 Retrying in {retry_delay} seconds...{Style.RESET_ALL}")
                time.sleep(retry_delay)
                enter_bootloader(port)
            else:
                print(f"{Fore.RED}💥 All flash attempts failed. Giving up.{Style.RESET_ALL}")
                sys.exit(1)
        attempt += 1



if __name__ == "__main__":
    kill_serial_monitors()
    serial_port = find_serial_port()
    run_gulp_if_needed()
    build_all(ENV_NAME)
    check_build_artifacts()
    fs_offset, _ = extract_filesystem_partition(PARTITIONS_CSV)
    flash_mode, flash_freq = extract_flash_config(MYENV_TXT)
    enter_bootloader(serial_port)
    flash_all_images(serial_port, flash_mode, flash_freq, fs_offset, max_retries=1)
    launch_serial_monitor(serial_port)

