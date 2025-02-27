import subprocess
import sys
import os
import configparser
from pathlib import Path
import platform
import shutil
import os
Import("env")

def find_platformio_root():
    """Find the PlatformIO project root by looking for platformio.ini."""
    current_dir = os.getcwd()
    while current_dir:
        if os.path.exists(os.path.join(current_dir, "platformio.ini")):
            return current_dir
        parent_dir = os.path.dirname(current_dir)
        if parent_dir == current_dir:  # Stop if we reach the root
            break
        current_dir = parent_dir
    return None

def get_last_used_env():
    """Find the most recently used environment from .pio/build/."""
    build_dir = Path(find_platformio_root()) / ".pio" / "build"
    
    if not build_dir.exists():
        return None

    env_dirs = [d for d in build_dir.iterdir() if d.is_dir()]
    if not env_dirs:
        return None

    # Sort by last modified time
    env_dirs.sort(key=lambda d: d.stat().st_mtime, reverse=True)
    
    return env_dirs[0].name  # Return the most recently used environment

def run_command(command):
    """Execute a command and print output in real-time."""
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    
    for line in process.stdout:
        print(line, end="")
    
    process.wait()
    if process.returncode != 0:
        print(f"\n❌ Error: Command '{command}' failed with exit code {process.returncode}")
        sys.exit(process.returncode)

def main(target=None, source=None, env=None):
    # Find and switch to PlatformIO root directory
    pio_root = find_platformio_root()
    if not pio_root:
        print("❌ Error: Not a PlatformIO project. No 'platformio.ini' found.")
        sys.exit(1)

    os.chdir(pio_root)
    print(f"📂 Switched to PlatformIO project root: {pio_root}")

    # Ensure PlatformIO is available
    try:
        subprocess.run(["pio", "--version"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    except FileNotFoundError:
        print("❌ Error: PlatformIO is not found in the system PATH.")
        print("➡ Please run this script inside the PlatformIO Terminal in VS Code.")
        sys.exit(1)

    # Detect the last used environment
    env = get_last_used_env()
    if not env:
        print("❌ Error: No recent build environment found in .pio/build/.")
        sys.exit(1)

    print(f"\n🔹 Using active environment: {env}")

    # Define PlatformIO commands with the detected environment
    commands = [
        f"gulpme.bat",     # Make Filesystem
        f"pio run -e {env} -t erase",     # Erase flash
        f"pio run -e {env} -t buildfs",   # Build filesystem
        f"pio run -e {env} -t upload",    # Upload firmware
        f"pio run -e {env} -t uploadfs"   # Upload filesystem
    ]

    for cmd in commands:
        print(f"\n🚀 Executing: {cmd}")
        run_command(cmd)

    print("\n✅ Flashing and uploading completed successfully.")

env.AddCustomTarget(
    name="automation",
    dependencies=None,
    actions=[
        main
    ],
    title="Erase, Build, Flash, Upload Filesys",
    description="Full automation"
)
