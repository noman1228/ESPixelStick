# --- IMPORTS ---
import os
import sys
import csv
import time
import subprocess
import serial
import psutil
from pathlib import Path
from colorama import Fore, Style
import serial.tools.list_ports
def has_changed_since_stamp(target_path, stamp_path):
    if not stamp_path.exists():
        return True
    stamp_time = stamp_path.stat().st_mtime
    for root, _, files in os.walk(target_path):
        for file in files:
            full_path = Path(root) / file
            if full_path.stat().st_mtime > stamp_time:
                return True
    return False

# --- CONFIG ---
PROJECT_DIR = Path(os.path.join(os.getcwd(), "file_sys.py")).parent
PARTITIONS_CSV = PROJECT_DIR / "ESP32_partitions.csv"
MYENV_TXT = PROJECT_DIR / "MyEnv.txt"
BUILD_ROOT = PROJECT_DIR / ".pio" / "build"
GULP_SCRIPT = PROJECT_DIR / "gulpme.bat"
GULP_STAMP_FILE = PROJECT_DIR / ".gulp_stamp"
ESPNOW_TOOL = PROJECT_DIR / ".pio" / "packages" / "tool-esptoolpy" / "esptool.py"

# --- FUNCTIONS ---
def detect_active_environment(build_dir_root):
    if not build_dir_root.exists():
        print(f"{Fore.RED}✘ Build directory not found: {build_dir_root}{Style.RESET_ALL}")
        sys.exit(1)

    envs = [d.name for d in build_dir_root.iterdir() if d.is_dir()]
    if not envs:
        print(f"{Fore.RED}✘ No environments found in build directory.{Style.RESET_ALL}")
        sys.exit(1)

    if len(envs) == 1:
        return envs[0]

    print(f"{Fore.YELLOW}⚠ Multiple environments detected:{Style.RESET_ALL}")
    for idx, env in enumerate(envs):
        print(f"{Fore.CYAN}[{idx+1}]{Style.RESET_ALL} {env}")

    print(f"\nType the number of the environment to use, or type '{Fore.GREEN}all{Style.RESET_ALL}' to run all.")

    while True:
        choice = input(f"{Fore.MAGENTA}Select environment: {Style.RESET_ALL}").strip().lower()
        if choice == 'all':
            return envs
        if choice.isdigit() and 1 <= int(choice) <= len(envs):
            return envs[int(choice) - 1]
        print(f"{Fore.RED}Invalid choice. Try again.{Style.RESET_ALL}")

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

def erase_flash():
    print(f"{Fore.MAGENTA}💥 Erasing entire flash...{Style.RESET_ALL}")
    subprocess.run([
        sys.executable, str(ESPNOW_TOOL),
        "--chip", "esp32",
        "--port", serial_port,
        "erase_flash"
    ], check=True)
            
def copy_and_prepare_binaries(env_name):
    from shutil import copyfile
    import json

    output_dir = PROJECT_DIR / "firmware" / "esp32"  # or use BOARD_MCU dynamically
    output_dir.mkdir(parents=True, exist_ok=True)

    env_file = PROJECT_DIR / "MyEnv.txt"
    if not env_file.exists():
        print(f"{Fore.RED}✘ MyEnv.txt not found. Run a PlatformIO build with CustomTargets.py enabled first.{Style.RESET_ALL}")
        return

    # Pull board info
    board_flash_mode = "dio"
    board_flash_freq = "80m"
    board_mcu = "esp32"

    with open(env_file, "r") as f:
        for line in f.readlines():
            if "BOARD_FLASH_MODE" in line:
                board_flash_mode = line.split(":")[1].strip().strip("',\"")
            elif "BOARD_F_FLASH" in line:
                hz = line.split(":")[1].strip().strip("',\"").rstrip("L")
                board_flash_freq = f"{int(hz) // 1000000}m"
            elif "BOARD_MCU" in line:
                board_mcu = line.split(":")[1].strip().strip("',\"")

    # Define paths
    boot = BUILD_DIR / "bootloader.bin"
    app = BUILD_DIR / "firmware.bin"
    part = BUILD_DIR / "partitions.bin"
    app0 = next((BUILD_DIR.glob("*boot_app0.bin")), None)
    fs = BUILD_DIR / "littlefs.bin"

    dst_prefix = output_dir / f"{env_name}"
    paths = {
        "bootloader": (boot, dst_prefix.with_name(dst_prefix.name + "-bootloader.bin")),
        "application": (app, dst_prefix.with_name(dst_prefix.name + "-app.bin")),
        "partitions": (part, dst_prefix.with_name(dst_prefix.name + "-partitions.bin")),
        "fs": (fs, dst_prefix.with_name(dst_prefix.name + "-littlefs.bin")),
    }
    if app0:
        paths["boot_app0"] = (app0, dst_prefix.with_name(dst_prefix.name + "-boot_app0.bin"))

    for label, (src, dst) in paths.items():
        if src.exists():
            print(f"{Fore.GREEN}✔ Copying {label} → {dst.name}{Style.RESET_ALL}")
            copyfile(src, dst)
        else:
            print(f"{Fore.YELLOW}⚠ Missing {label} at {src}{Style.RESET_ALL}")

    # Optionally: merged image
    merged_bin = dst_prefix.with_name(dst_prefix.name + "-merged.bin")
    esptool_cmd = [
        "esptool.py", "--chip", board_mcu, "merge_bin",
        "-o", str(merged_bin),
        "--flash_mode", board_flash_mode,
        "--flash_freq", board_flash_freq,
        "--flash_size", "4MB",
        "0x0000", str(paths["bootloader"][1]),
        "0x8000", str(paths["partitions"][1]),
        "0xe000", str(paths["boot_app0"][1]) if "boot_app0" in paths else "",
        "0x10000", str(paths["application"][1]),
        "0x3B0000", str(paths["fs"][1]),
    ]
    print(f"{Fore.CYAN}🔧 Generating merged image: {merged_bin.name}{Style.RESET_ALL}")
    subprocess.run([arg for arg in esptool_cmd if arg], check=False)
def build_if_needed(env_name):
    source_dirs = [PROJECT_DIR / "src", PROJECT_DIR / "include"]
    stamp_file = PROJECT_DIR / f".build_stamp_{env_name}"

    needs_build = any(has_changed_since_stamp(path, stamp_file) for path in source_dirs)

    if needs_build:
        print(f"{Fore.CYAN}🔨 Code changed. Starting full build...{Style.RESET_ALL}")
        erase_flash()
        build_all(env_name)
        with open(stamp_file, "w") as f:
            f.write(time.strftime("%Y-%m-%d %H:%M:%S"))
    else:
        print(f"{Fore.YELLOW}⏭ Code unchanged. Skipping build.{Style.RESET_ALL}")

def build_all(env_name):
    print(f"{Fore.CYAN}🔨 Building firmware and filesystem for {env_name}...{Style.RESET_ALL}")
    subprocess.run(["platformio", "run", "-e", env_name], check=True)
    subprocess.run(["platformio", "run", "-t", "buildfs", "-e", env_name], check=True)

def run_gulp_if_needed():
    data_dir = PROJECT_DIR / "data"
    if not data_dir.exists():
        print(f"{Fore.RED}✘ data/ directory missing — cannot run Gulp.{Style.RESET_ALL}")
        sys.exit(1)

    if has_changed_since_stamp(data_dir, GULP_STAMP_FILE):
        print(f"{Fore.CYAN}🛠 Running Gulp: {GULP_SCRIPT}{Style.RESET_ALL}")
        if not GULP_SCRIPT.exists():
            print(f"{Fore.RED}✘ gulpme.bat not found at {GULP_SCRIPT}{Style.RESET_ALL}")
            sys.exit(1)
        subprocess.run([str(GULP_SCRIPT)], shell=True, check=True)
        with open(GULP_STAMP_FILE, "w") as f:
            f.write(time.strftime("%Y-%m-%d %H:%M:%S"))
        print(f"{Fore.GREEN}✔ Gulp finished and stamp updated.{Style.RESET_ALL}")
    else:
        print(f"{Fore.YELLOW}⏭ data/ unchanged. Skipping Gulp.{Style.RESET_ALL}")


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

# --- MAIN ---
if __name__ == "__main__":
    kill_serial_monitors()
    serial_port = find_serial_port()

    env_selection = detect_active_environment(BUILD_ROOT)

    if isinstance(env_selection, list):
        for ENV_NAME in env_selection:
            BUILD_DIR = BUILD_ROOT / ENV_NAME
            BOOTLOADER_BIN = BUILD_DIR / "bootloader.bin"
            PARTITIONS_BIN = BUILD_DIR / "partitions.bin"
            FIRMWARE_BIN = BUILD_DIR / "firmware.bin"
            IMAGE_PATH = BUILD_DIR / "littlefs.bin"

            print(f"\n{Fore.BLUE}=== Processing environment: {ENV_NAME} ==={Style.RESET_ALL}")
            run_gulp_if_needed()
            build_if_needed(ENV_NAME)
            check_build_artifacts()
            fs_offset, _ = extract_filesystem_partition(PARTITIONS_CSV)
            flash_mode, flash_freq = extract_flash_config(MYENV_TXT)
            enter_bootloader(serial_port)
            flash_all_images(serial_port, flash_mode, flash_freq, fs_offset, max_retries=1)

        launch_serial_monitor(serial_port)

    else:
        ENV_NAME = env_selection
        BUILD_DIR = BUILD_ROOT / ENV_NAME
        BOOTLOADER_BIN = BUILD_DIR / "bootloader.bin"
        PARTITIONS_BIN = BUILD_DIR / "partitions.bin"
        FIRMWARE_BIN = BUILD_DIR / "firmware.bin"
        IMAGE_PATH = BUILD_DIR / "littlefs.bin"

        run_gulp_if_needed()
        build_if_needed(ENV_NAME)
        check_build_artifacts()
        fs_offset, _ = extract_filesystem_partition(PARTITIONS_CSV)
        flash_mode, flash_freq = extract_flash_config(MYENV_TXT)
        enter_bootloader(serial_port)
        flash_all_images(serial_port, flash_mode, flash_freq, fs_offset, max_retries=1)
        launch_serial_monitor(serial_port)
