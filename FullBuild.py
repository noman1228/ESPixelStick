# FullBuild.py - Jason Theriault 2025
# Does a bit of snooping and builds what needs to be built. Define your envs and hit go
# jacked a few things from OG ESPixelStick Scripts, even uses them
# #mybrotherinchrist this has been a savior


# --- IMPORTS --- (no Tariffs)
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
import configparser

# --- CONFIG ---
PROJECT_DIR = Path(os.path.join(os.getcwd(), "file_sys.py")).parent
PARTITIONS_CSV = PROJECT_DIR / "ESP32_partitions.csv"
MYENV_TXT = PROJECT_DIR / "MyEnv.txt"
BUILD_ROOT = PROJECT_DIR / ".pio" / "build"
GULP_SCRIPT = PROJECT_DIR / "gulpme.bat"
GULP_STAMP_FILE = PROJECT_DIR / ".gulp_stamp"
NO_FS_STAMP = PROJECT_DIR / ".no_filesystem"
ESPNOW_TOOL = next(
    (
        p.resolve()
        for base in [PROJECT_DIR, Path.home() / ".platformio"]
        for p in (base / "packages" / "tool-esptoolpy").rglob("esptool.py")
        if p.exists()
    ),
    "esptool.py"
)

# --- FILESYSTEM SUPPORT CHECK --- (need all the support we can get)
def should_use_filesystem():
    if NO_FS_STAMP.exists():
        return False

    if not GULP_SCRIPT.exists():
        print(f"{Fore.YELLOW}⏭ Gulp script not found. No data folder expected. Skipping FS.{Style.RESET_ALL}")
        NO_FS_STAMP.write_text("no_fs")
        return False

    data_dir = PROJECT_DIR / "data"
    if data_dir.exists():
        return True

    print(f"{Fore.YELLOW}⏭ No data/ directory present. Filesystem will be skipped.{Style.RESET_ALL}")
    NO_FS_STAMP.write_text("no_fs")
    return False
# --- BUILD --- (You build me up)
def build_all(env_name):
    print(f"{Fore.CYAN}🔨 Building firmware for {env_name}...{Style.RESET_ALL}")
    subprocess.run(["platformio", "run", "-e", env_name], check=True)

    if should_use_filesystem():
        print(f"{Fore.CYAN}📦 Building filesystem image for {env_name}...{Style.RESET_ALL}")
        subprocess.run(["platformio", "run", "-t", "buildfs", "-e", env_name], check=True)

    else:
        print(f"{Fore.YELLOW}⏭ Skipping filesystem build for {env_name}.{Style.RESET_ALL}")

# --- COPY BINARIES --- (Trump says there's only one though)
def copy_and_prepare_binaries(env_name):
    from shutil import copyfile
    output_dir = PROJECT_DIR / "firmware" / "esp32"
    output_dir.mkdir(parents=True, exist_ok=True)

    env_file = PROJECT_DIR / "MyEnv.txt"
    if not env_file.exists():
        print(f"{Fore.RED}✘ MyEnv.txt not found. Run a PlatformIO build with CustomTargets.py enabled first.{Style.RESET_ALL}")
        return

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

    boot = BUILD_DIR / "bootloader.bin"
    app = BUILD_DIR / "firmware.bin"
    part = BUILD_DIR / "partitions.bin"
    app0 = next(BUILD_DIR.glob("*boot_app0.bin"), None)
    fs = BUILD_DIR / "littlefs.bin" if not NO_FS_STAMP.exists() else None

    dst_prefix = output_dir / f"{env_name}"
    paths = {
        "bootloader": (boot, dst_prefix.with_name(dst_prefix.name + "-bootloader.bin")),
        "application": (app, dst_prefix.with_name(dst_prefix.name + "-app.bin")),
        "partitions": (part, dst_prefix.with_name(dst_prefix.name + "-partitions.bin")),
    }
    if fs:
        paths["fs"] = (fs, dst_prefix.with_name(dst_prefix.name + "-littlefs.bin"))
    if app0:
        paths["boot_app0"] = (app0, dst_prefix.with_name(dst_prefix.name + "-boot_app0.bin"))

    for label, item in paths.items():
        src, dst = item
        if src.exists():
            print(f"{Fore.GREEN}✔ Copying {label} → {dst.name}{Style.RESET_ALL}")
            copyfile(src, dst)
        else:
            print(f"{Fore.YELLOW}⚠ Missing {label} at {src}{Style.RESET_ALL}")

    merged_bin = dst_prefix.with_name(dst_prefix.name + "-merged.bin")
    esptool_cmd = [
        "esptool.py", "--chip", board_mcu, "merge_bin",
        "-o", str(merged_bin),
        "--flash_mode", board_flash_mode,
        "--flash_freq", board_flash_freq,
        "--flash_size", "4MB",
        "0x0000", str(paths["bootloader"][1]),
        "0x8000", str(paths["partitions"][1]),
        "0x10000", str(paths["application"][1]),
    ]

    if "boot_app0" in paths:
        esptool_cmd.extend(["0xe000", str(paths["boot_app0"][1])])
    if "fs" in paths:
        esptool_cmd.extend(["0x3B0000", str(paths["fs"][1])])
    else:
        print(f"{Fore.YELLOW}⏭ Skipping FS in merged image. No filesystem selected.{Style.RESET_ALL}")

    print(f"{Fore.CYAN}🔧 Generating merged image: {merged_bin.name}{Style.RESET_ALL}")
    subprocess.run(esptool_cmd, check=False)

# --- FLASH IMAGES --- (we're going streaking in the quad)
def flash_all_images(port, flash_mode, flash_freq, fs_offset, max_retries=1, retry_delay=10):
    print(f"{Fore.CYAN}🚀 Flashing all firmware and filesystem images to {port}...{Style.RESET_ALL}")

    cmd = [
        sys.executable,
        str(ESPNOW_TOOL),
        "--chip", "esp32",
        "--port", port,
        "--baud", "460800",
        "write_flash",
        "--flash_mode", flash_mode, #Maybee we go keep. Maybe we be cheap....
        "--flash_freq", flash_freq,
        "--flash_size", "detect",
        "0x1000", str(BOOTLOADER_BIN),
        "0x8000", str(PARTITIONS_BIN),
        "0x10000", str(FIRMWARE_BIN),
    ]

    if not NO_FS_STAMP.exists():
        cmd.extend([f"0x{fs_offset:X}", str(IMAGE_PATH)])
    else:
        print(f"{Fore.YELLOW}⏭ Filesystem image excluded from flash command.{Style.RESET_ALL}")

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

def kill_serial_monitors(): # easily the next big NetFlix title
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
        if any(chip in port.description for chip in ("USB", "UART", "CP210", "CH340", "ESP32")):
            print(f"{Fore.GREEN}✔ Found port: {port.device}{Style.RESET_ALL}")
            return port.device
    print(f"{Fore.RED}✘ No suitable ESP32 device found. Is it plugged in?{Style.RESET_ALL}")
    sys.exit(1)

def run_gulp_if_needed():
    if not GULP_SCRIPT.exists():
        print(f"{Fore.YELLOW}⏭ Gulp script not found. Skipping Gulp step.{Style.RESET_ALL}")
        return

    data_dir = PROJECT_DIR / "data"
    if not data_dir.exists():
        print(f"{Fore.YELLOW}⚠ Gulp script exists but data/ directory is missing. Running Gulp anyway...{Style.RESET_ALL}")
        return

    data_dir = PROJECT_DIR / "data"
    skip_fs_stamp = PROJECT_DIR / ".no_filesystem" #You're a fucking hack
    if skip_fs_stamp.exists():
        print(f"{Fore.YELLOW}⏭ Filesystem was previously marked as unused. Skipping Gulp.{Style.RESET_ALL}")
        return

    has_data = data_dir.exists()

    if not has_data:
        response = input(f"{Fore.MAGENTA}No data/ folder found. Does this environment use a filesystem? [y/N]: {Style.RESET_ALL}").strip().lower()
        if response != 'y':
            print(f"{Fore.YELLOW}⏭ Skipping filesystem setup.{Style.RESET_ALL}")
            with open(skip_fs_stamp, "w") as f:
                f.write("no_fs")
            return
        else:
            data_dir.mkdir(parents=True)
            print(f"{Fore.GREEN}✔ Created empty data/ folder for filesystem.{Style.RESET_ALL}")

    if has_changed_since_stamp(data_dir, GULP_STAMP_FILE):
        print(f"{Fore.CYAN}🛠 Running Gulp: {GULP_SCRIPT}{Style.RESET_ALL}")
        subprocess.run([str(GULP_SCRIPT)], shell=True, check=True)
        with open(GULP_STAMP_FILE, "w") as f:
            f.write(time.strftime("%Y-%m-%d %H:%M:%S"))
        print(f"{Fore.GREEN}✔ Gulp finished and stamp updated.{Style.RESET_ALL}")
    else:
        print(f"{Fore.YELLOW}⏭ data/ unchanged. Skipping Gulp.{Style.RESET_ALL}")

def erase_flash():
    if not ESPNOW_TOOL:
        print(f"{Fore.RED}✘ esptool.py not found in .pio/packages. Did you run a PlatformIO build first?{Style.RESET_ALL}")
        sys.exit(1)
    print(f"{Fore.MAGENTA}💥 Erasing entire flash...{Style.RESET_ALL}")
    subprocess.run([
        sys.executable, str(ESPNOW_TOOL),
        "--chip", "esp32",
        "--port", serial_port,
        "erase_flash"
    ], check=True)


def build_if_needed(env_name): #HereWEgoAgain
    global BUILD_DIR, BOOTLOADER_BIN, PARTITIONS_BIN, FIRMWARE_BIN, IMAGE_PATH
    BUILD_DIR = BUILD_ROOT / env_name
    BOOTLOADER_BIN = BUILD_DIR / "bootloader.bin"
    PARTITIONS_BIN = BUILD_DIR / "partitions.bin"
    FIRMWARE_BIN = BUILD_DIR / "firmware.bin"
    IMAGE_PATH = BUILD_DIR / "littlefs.bin"
    source_dirs = [PROJECT_DIR / "src", PROJECT_DIR / "include"]
    stamp_file = PROJECT_DIR / f".build_stamp_{env_name}"

    needs_build = any(has_changed_since_stamp(path, stamp_file) for path in source_dirs)

    if needs_build:
        print(f"{Fore.CYAN}🔨 Code changed. Starting full build...{Style.RESET_ALL}")
        erase_flash()
        build_all(env_name)
        with open(stamp_file, "w") as f:
            f.write(time.strftime("%Y-%m-%d %H:%M:%S")) #Still not sold on this hack. False positives are mostly gone at least
    else:
        print(f"{Fore.YELLOW}⏭ Code unchanged. Skipping build.{Style.RESET_ALL}")

def check_build_artifacts():
    required = [BOOTLOADER_BIN, PARTITIONS_BIN, FIRMWARE_BIN]
    if not NO_FS_STAMP.exists():
        required.append(IMAGE_PATH)

    missing = [f for f in required if not f.exists()]
    if missing:
        print(f"{Fore.YELLOW}⚠ Missing build artifacts detected. Forcing rebuild...{Style.RESET_ALL}")
        for f in missing:
            print(f"{Fore.YELLOW} - Missing: {f}{Style.RESET_ALL}")
        build_all(ENV_NAME)
        for f in required:
            if not f.exists():
                print(f"{Fore.RED}✘ Still missing: {f}{Style.RESET_ALL}")
                sys.exit(1)
    else:
        print(f"{Fore.GREEN}✔ All required build artifacts found.{Style.RESET_ALL}")


def extract_filesystem_partition(csv_path):
    if not csv_path.exists():
        print(f"{Fore.YELLOW}⚠ Partition CSV not found. Using default FS offset 0x3B0000, size 0x50000.{Style.RESET_ALL}")
        return 0x3B0000, 0x50000

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

    print(f"{Fore.YELLOW}⚠ No FS partition found in CSV. Using default FS offset 0x3B0000, size 0x50000.{Style.RESET_ALL}")
    return 0x3B0000, 0x50000

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

def enter_bootloader(port):
    print(f"{Fore.CYAN}⏎ Forcing board into bootloader mode on {port}...{Style.RESET_ALL}")
    try:
        with serial.Serial(port, 115200) as ser:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.dtr = True
            time.sleep(0.1)
            ser.dtr = False
            time.sleep(0.2)
        print(f"{Fore.GREEN}✔ Bootloader mode triggered.{Style.RESET_ALL}")
    except Exception as e:
        print(f"{Fore.RED}✘ Could not trigger bootloader: {e}{Style.RESET_ALL}")

def flash_all_images(port, flash_mode, flash_freq, fs_offset, max_retries=1, retry_delay=10):
    print(f"{Fore.CYAN}🚀 Flashing all firmware and filesystem images to {port}...{Style.RESET_ALL}")

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

def launch_serial_monitor(port, baud=115200):
    print(f"{Fore.CYAN}📡 Launching serial monitor on {port}...{Style.RESET_ALL}")
    try:
        subprocess.run(["platformio", "device", "monitor", "--port", port, "--baud", str(baud)])
    except Exception as e:
        print(f"{Fore.RED}✘ Could not launch serial monitor: {e}{Style.RESET_ALL}")

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
            copy_and_prepare_binaries(ENV_NAME)

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

