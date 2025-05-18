# post_efu.py
Import("env")
import os
import sys
import subprocess
from datetime import datetime
from SCons.Script import AlwaysBuild
from make_efu import make_efu
from pathlib import Path
import serial.tools.list_ports
from colorama import Fore, Style

build_dir = env.subst("$PROJECT_BUILD_DIR")
project_dir = env.subst("$PROJECT_DIR")
output_dir = os.path.join(project_dir, "firmware", "EFU")
os.makedirs(output_dir, exist_ok=True)

# Environment cache file
ENV_CACHE_FILE = os.path.join(project_dir, ".efu_env")

def get_cached_environment():
    """Get previously used environment from cache file"""
    if os.path.exists(ENV_CACHE_FILE):
        with open(ENV_CACHE_FILE, 'r') as f:
            return f.read().strip()
    return None

def save_cached_environment(env_name):
    """Save selected environment to cache file"""
    with open(ENV_CACHE_FILE, 'w') as f:
        f.write(env_name)

def get_cmd_env():
    """Check if environment is specified via command line"""
    # Look for -e or --environment flag in command line args
    for i, arg in enumerate(sys.argv):
        if arg in ['-e', '--environment'] and i+1 < len(sys.argv):
            return sys.argv[i+1]
    return None

def check_env_exists(env_name):
    """Check if environment directory exists in build"""
    if not env_name:
        return False
    env_path = os.path.join(build_dir, env_name)
    return os.path.isdir(env_path)

def detect_latest_environment():
    """Find the most recently built environment"""
    build_root = Path(build_dir)
    
    if not build_root.exists():
        return None
        
    envs = [d for d in build_root.iterdir() if d.is_dir()]
    if not envs:
        return None
        
    # Sort by modification time (newest first)
    latest_env = max(envs, key=lambda d: d.stat().st_mtime)
    return latest_env.name

def detect_active_environment(non_interactive=False):
    """Detect available build environments with option for non-interactive mode"""
    build_root = Path(build_dir)
    current_variant = env.subst("$PIOENV")
    
    if not build_root.exists():
        print(f"{Fore.RED}✘ Build directory not found: {build_root}{Style.RESET_ALL}")
        return current_variant
    
    # First check if the current environment exists in the build directory
    current_env_dir = build_root / current_variant
    if current_env_dir.exists() and current_env_dir.is_dir():
        print(f"{Fore.GREEN}✓ Using current environment: {current_variant}{Style.RESET_ALL}")
        return current_variant
        
    # If not, look for alternatives
    envs = [d.name for d in build_root.iterdir() if d.is_dir()]
    if not envs:
        print(f"{Fore.RED}✘ No environments found in build directory.{Style.RESET_ALL}")
        return current_variant
        
    if len(envs) == 1:
        selected = envs[0]
        print(f"{Fore.GREEN}✓ Using single available environment: {selected}{Style.RESET_ALL}")
        return selected
    
    # Check cache before asking user
    cached_env = get_cached_environment()
    if cached_env and cached_env in envs:
        print(f"{Fore.GREEN}✓ Using cached environment: {cached_env}{Style.RESET_ALL}")
        return cached_env
    
    if non_interactive or os.environ.get('NONINTERACTIVE') == '1':
        print(f"{Fore.YELLOW}⚠ Multiple environments found in non-interactive mode, using current: {current_variant}{Style.RESET_ALL}")
        return current_variant
        
    print(f"{Fore.YELLOW}⚠ Multiple environments detected:{Style.RESET_ALL}")
    for idx, env_name in enumerate(envs):
        print(f"{Fore.CYAN}[{idx+1}]{Style.RESET_ALL} {env_name}")
    
    print(f"\nType the number of the environment to use, or press enter to use current ({current_variant}).")
    
    while True:
        choice = input(f"{Fore.MAGENTA}Select environment: {Style.RESET_ALL}").strip().lower()
        if not choice:
            return current_variant
        if choice.isdigit() and 1 <= int(choice) <= len(envs):
            selected = envs[int(choice) - 1]
            save_cached_environment(selected)
            return selected
        print(f"{Fore.RED}Invalid choice. Try again.{Style.RESET_ALL}")

def find_serial_port():
    """Find an available ESP32 serial port"""
    print("🔍 Searching for available serial ports...")
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if any(chip in port.description for chip in ("USB", "UART", "CP210", "CH340", "ESP32")):
            print(f"{Fore.GREEN}✔ Found port: {port.device}{Style.RESET_ALL}")
            return port.device
    print(f"{Fore.RED}✘ No suitable ESP32 device found. Is it plugged in?{Style.RESET_ALL}")
    return None

def after_build(source, target, env):
    print(f"{Fore.CYAN}====== EFU Builder ======{Style.RESET_ALL}")
    print(f"Build directory: {build_dir}")
    print(f"Output directory: {output_dir}")
    
    # Try to get environment in this order:
    # 1. Command line specified
    # 2. Current PlatformIO environment
    # 3. Latest built environment
    # 4. User selection (cached or interactive)
    
    cmd_env = get_cmd_env()
    pio_env = env.subst("$PIOENV")
    latest_env = detect_latest_environment()
    
    if cmd_env and check_env_exists(cmd_env):
        selected_variant = cmd_env
        print(f"{Fore.GREEN}✓ Using command-line specified environment: {selected_variant}{Style.RESET_ALL}")
    elif check_env_exists(pio_env):
        selected_variant = pio_env
        print(f"{Fore.GREEN}✓ Using current environment: {selected_variant}{Style.RESET_ALL}")
    elif latest_env:
        selected_variant = latest_env
        print(f"{Fore.GREEN}✓ Using latest built environment: {selected_variant}{Style.RESET_ALL}")
    else:
        selected_variant = detect_active_environment()
    
    sketch_bin = os.path.join(build_dir, selected_variant, "firmware.bin")
    fs_bin = os.path.join(build_dir, selected_variant, "littlefs.bin")
    efu_out = os.path.join(build_dir, selected_variant, f"{selected_variant}.efu")
    
    # Auto-build firmware if missing
    if not os.path.exists(sketch_bin):
        print(f"{Fore.YELLOW}⚠ Firmware binary not found, building...{Style.RESET_ALL}")
        result = subprocess.run(["pio", "run", "-e", selected_variant], cwd=project_dir)
        if result.returncode != 0 or not os.path.exists(sketch_bin):
            print(f"{Fore.RED}✘ Failed to build firmware for {selected_variant}.{Style.RESET_ALL}")
            return

    # Auto-build filesystem if missing
    if not os.path.exists(fs_bin):
        print(f"{Fore.YELLOW}⚠ Filesystem image not found, building...{Style.RESET_ALL}")
        result = subprocess.run(["pio", "run", "-t", "buildfs", "-e", selected_variant], cwd=project_dir)
        if result.returncode != 0 or not os.path.exists(fs_bin):
            print(f"{Fore.RED}✘ Failed to build filesystem for {selected_variant}.{Style.RESET_ALL}")
            return

    print(f"{Fore.CYAN}◼ Creating EFU: {os.path.basename(efu_out)}{Style.RESET_ALL}")
    make_efu(sketch_bin, fs_bin, efu_out)

    timestamp = datetime.now().strftime('%Y%m%d')
    final_filename = f"{selected_variant}_{timestamp}.efu"
    final_path = os.path.join(output_dir, final_filename)

    efu_tool = os.path.join(project_dir, "efu_tool.py")
    print(f"{Fore.CYAN}◼ Validating EFU with efu_tool.py{Style.RESET_ALL}")
    result = subprocess.run([
        sys.executable, efu_tool,
        "--efu", efu_out,
        "--project", project_dir,
        "--env", selected_variant,
        "--output", output_dir
    ])

    if result.returncode != 0:
        print(f"{Fore.RED}✘ EFU validation failed!{Style.RESET_ALL}")
    else:
        print(f"{Fore.GREEN}✓ EFU validated and saved to {final_path}{Style.RESET_ALL}")
        
        # Output a success message with instruction on how to install
        print("\n" + "="*60)
        print(f"{Fore.GREEN}EFU File Ready: {final_filename}{Style.RESET_ALL}")
        print("To install:")
        print(f"1. Access your ESP device at http://esps-XXXX.local")
        print(f"2. Go to 'File Upload' section")
        print(f"3. Upload file from: {final_path}")
        print("="*60 + "\n")

AlwaysBuild(env.Alias("post_efu", "buildprog", after_build))