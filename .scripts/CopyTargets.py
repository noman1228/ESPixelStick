import platform
import shutil
import os
import subprocess
Import("env")

# Optional: Enable or disable verbose logging
VERBOSE = True

# Dump full environment to a file for debugging
with open("./MyEnv.txt", "w") as f:
    f.write(env.Dump())

def log(msg):
    if VERBOSE:
        print(msg)

def PrepareDestinationDirectory(DirRoot, DirPath):
    log(f"mkdirs: path - '{DirPath}'")
    os.makedirs(DirPath, 0o777, exist_ok=True)
    if platform.system() != "Windows":
        subprocess.run(f"chmod -R a+rwx {DirRoot}", shell=True)

# Get environment values from PlatformIO
BUILD_DIR = env['PROJECT_BUILD_DIR']
PIOENV    = env['PIOENV']
BOARD     = env['BOARD']
PROGNAME  = env['PROGNAME']
BOARD_MCU = env['BOARD_MCU']
BOARD_FLASH_MODE = env['BOARD_FLASH_MODE']
BOARD_F_FLASH = env['BOARD_F_FLASH'].removesuffix('000000L') + 'm'

# Source paths
SRC_DIR  = os.path.join(BUILD_DIR, PIOENV)
SRC_BIN  = os.path.join(SRC_DIR, PROGNAME + ".bin")
SRC_PART = os.path.join(SRC_DIR, "partitions.bin")
SRC_ELF  = os.path.join(SRC_DIR, PROGNAME + ".elf")
SRC_MAP  = os.path.join(SRC_DIR, PROGNAME + ".map")
SRC_MAP2 = os.path.join(".", PROGNAME + ".map")

# Destination paths
DST_ROOT  = "./firmware/"
DST_DIR   = os.path.join(DST_ROOT, BOARD_MCU)
DST_BIN   = os.path.join(DST_DIR, PIOENV + "-app.bin")
DST_PART  = os.path.join(DST_DIR, PIOENV + "-partitions.bin")
DST_BOOT  = os.path.join(DST_DIR, PIOENV + "-bootloader.bin")
DST_BOOT_APP0 = os.path.join(DST_DIR, "boot_app0.bin")
DST_MERG  = os.path.join(DST_DIR, PIOENV + "-merged.bin")
DST_FS    = os.path.join(DST_DIR, PIOENV + "-littlefs.bin")

# Debug artifact paths
DBG_ROOT  = "./debug/"
DBG_DIR   = os.path.join(DBG_ROOT, BOARD_MCU)
DST_ELF   = os.path.join(DBG_DIR, PIOENV + ".elf")
DST_MAP   = os.path.join(DBG_DIR, PIOENV + ".map")

# SPIFFS image configuration (adjusted to match partition table)
SPIFFS_SIZE = "0xE0000"

# Determine host OS
OS_NAME = platform.system().lower()
FS_BLD_PATH = "./dist/bin/"

if "win" in OS_NAME:
    FS_BLD_PATH += "win32/mklittlefs.exe"
elif "linux" in OS_NAME:
    FS_BLD_PATH += "linux64/mklittlefs"
elif "darwin" in OS_NAME:
    FS_BLD_PATH += "macos/mklittlefs"
else:
    raise RuntimeError(f"Unknown OS: {OS_NAME}")

def merge_bin():
    log("Creating SPIFFS filesystem image")
    FS_BLD_CMD = f"{FS_BLD_PATH} -b 4096 -p 256 -s {SPIFFS_SIZE} -c ./data {DST_FS}"

    MERGE_CMD = (
        "esptool"
        + f" --chip {BOARD_MCU}"
        + " merge_bin"
        + f" -o {DST_MERG}"
        + f" --flash_mode {BOARD_FLASH_MODE}"
        + f" --flash_freq {BOARD_F_FLASH}"
        + " --flash_size 4MB"
        + f" 0x1000 {DST_BOOT}"
        + f" 0x8000 {DST_PART}"
        + f" 0xe000 {DST_BOOT_APP0}"
        + f" 0x10000 {DST_BIN}"
        + f" 0x310000 {DST_FS}"
    )

    if "win" in OS_NAME:
        FS_BLD_CMD = FS_BLD_CMD.replace("/", "\\")
        MERGE_CMD = MERGE_CMD.replace("/", "\\")

    log("FS_BLD_CMD: " + FS_BLD_CMD)
    result = subprocess.run(FS_BLD_CMD, shell=True)
    if result.returncode != 0:
        raise RuntimeError("SPIFFS image generation failed.")

    log("MERGE_CMD: " + MERGE_CMD)
    result = subprocess.run(MERGE_CMD, shell=True)
    if result.returncode != 0:
        raise RuntimeError("Binary merge failed.")

def after_build(source, target, env):
    log("Starting after-build process")

    # Prepare firmware output directory
    PrepareDestinationDirectory(DST_ROOT, DST_DIR)
    log(f"Copy: {SRC_BIN} -> {DST_BIN}")
    shutil.copyfile(SRC_BIN, DST_BIN)

    # Prepare debug output directory
    PrepareDestinationDirectory(DBG_ROOT, DBG_DIR)
    log(f"Copy: {SRC_ELF} -> {DST_ELF}")
    shutil.copyfile(SRC_ELF, DST_ELF)

    # Move map files
    if os.path.exists(SRC_MAP):
        log(f"Move: {SRC_MAP} -> {DST_MAP}")
        shutil.move(SRC_MAP, DST_MAP)

    if os.path.exists(SRC_MAP2):
        log(f"Move: {SRC_MAP2} -> {DST_MAP}")
        shutil.move(SRC_MAP2, DST_MAP)

    # Copy bootloader, boot_app0, and partition table from extra images
    if "FLASH_EXTRA_IMAGES" in env:
        for _, imagePath in env["FLASH_EXTRA_IMAGES"]:
            if "boot_app0" in imagePath:
                log(f"Copy: {imagePath} -> {DST_BOOT_APP0}")
                shutil.copyfile(imagePath, DST_BOOT_APP0)
            elif "partitions" in imagePath:
                log(f"Copy: {imagePath} -> {DST_PART}")
                shutil.copyfile(imagePath, DST_PART)
            elif "bootloader" in imagePath:
                path = imagePath.replace('${BOARD_FLASH_MODE}', BOARD_FLASH_MODE)\
                                .replace("${__get_board_f_flash(__env__)}", BOARD_F_FLASH)
                log(f"Copy: {path} -> {DST_BOOT}")
                shutil.copyfile(path, DST_BOOT)

    merge_bin()

# Attach the post-build hook
env.AddPostAction("buildprog", after_build)
