import xml.etree.ElementTree as ET
import requests
import os
import json
import time
import argparse
import logging
from logging.handlers import RotatingFileHandler
from datetime import datetime
from pathlib import Path
from requests_toolbelt.multipart.encoder import MultipartEncoder

# ---------- CONFIG ----------
XLIGHTS_XML_PATH = r"C:\Users\jay\desktop\Lights 2024\xlights_networks.xml"
LOG_DIR_NAME = "logs"
LOGGER_NAME = "Uploader"
SUMMARY_PREFIX = "upload_log_"
# ----------------------------

def setup_logging(project_dir: Path, verbose: bool = False) -> logging.Logger:
    """Mirror FullBuild.py logging: rotating file + console handler."""
    log_dir = project_dir / LOG_DIR_NAME
    log_dir.mkdir(exist_ok=True)

    logger = logging.getLogger(LOGGER_NAME)
    # Avoid handler duplication if rerun in same interpreter
    if logger.handlers:
        return logger

    logger.setLevel(logging.DEBUG)

    file_formatter = logging.Formatter(
        "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    )
    file_handler = RotatingFileHandler(
        log_dir / "upload.log",
        maxBytes=10 * 1024 * 1024,  # 10MB
        backupCount=5
    )
    file_handler.setFormatter(file_formatter)
    file_handler.setLevel(logging.DEBUG)

    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.DEBUG if verbose else logging.INFO)

    logger.addHandler(file_handler)
    logger.addHandler(console_handler)
    return logger

def is_matrix_controller(ctrl):
    for tag in ["Name", "Description", "comment"]:
        val = ctrl.get(tag, "").lower()
        if "fpp" in val:
            return True
    return False

def get_controller_summary(ctrl):
    ip = ctrl.get("IP", "UNKNOWN_IP")
    name = ctrl.get("Name", "")
    desc = ctrl.get("Description", "")
    display_name = name if name else desc if desc else "(no name)"
    return (ip, display_name)

def get_target_controllers(xml_path, logger):
    tree = ET.parse(xml_path)
    root = tree.getroot()
    controllers = root.findall(".//Controller")

    targets = []
    for ctrl in controllers:
        ip = ctrl.get("IP")
        if not ip:
            continue
        if is_matrix_controller(ctrl):
            logger.info(f"🚫 Skipping matrix/FPP controller: {ip} [{ctrl.get('Name','')}]")
            continue
        targets.append(get_controller_summary(ctrl))
    return targets

def parse_selection(selection, count):
    selection = selection.strip().lower()
    if selection == "all":
        return list(range(count))
    chosen = set()
    for part in selection.split(','):
        part = part.strip()
        if '-' in part:
            start, end = part.split('-', 1)
            try:
                s = int(start) - 1
                e = int(end) - 1
            except ValueError:
                continue
            for i in range(s, e + 1):
                if 0 <= i < count:
                    chosen.add(i)
        else:
            try:
                idx = int(part) - 1
            except ValueError:
                continue
            if 0 <= idx < count:
                chosen.add(idx)
    return sorted(chosen)

def upload_firmware(ip, firmware_path, results, logger, timeout=30):
    url = f"http://{ip}/updatefw"
    try:
        encoder = MultipartEncoder(
            fields={'file': (os.path.basename(firmware_path),
                             open(firmware_path, 'rb'),
                             'application/octet-stream')}
        )
        logger.info(f"📤 Uploading '{firmware_path}' to {ip}...")
        resp = requests.post(
            url, data=encoder,
            headers={'Content-Type': encoder.content_type},
            timeout=timeout
        )
        if resp.status_code == 200:
            msg = f"✅ Update success on {ip}. Waiting for device to reboot..."
            logger.info(msg)
            results[ip] = msg
            return True
        else:
            msg = f"⚠️ Update failed on {ip}. HTTP {resp.status_code}: {resp.text.strip()}"
            logger.warning(msg)
            results[ip] = msg
            return False
    except Exception as e:
        msg = f"❌ {ip} error: {e}"
        logger.error(msg)
        results[ip] = msg
        return False

def wait_for_device(ip, logger, timeout=180, delay=5):
    """Poll until device responds or timeout expires."""
    url = f"http://{ip}/conf/input_config.json"
    logger.info(f"🔄 Waiting for {ip} to come back online (timeout {timeout}s)...")
    start = time.time()
    while time.time() - start < timeout:
        try:
            resp = requests.get(url, timeout=5)
            if resp.status_code == 200:
                logger.info(f"✅ {ip} is back online.")
                return True
        except requests.RequestException:
            pass
        time.sleep(delay)
    logger.error(f"❌ Timeout waiting for {ip} to come back online.")
    return False

def enable_fpp(ip, results, logger):
    """Set secondary input to FPP Remote and configure Marquee effect groups."""
    url = f"http://{ip}/conf/input_config.json"
    try:
        response = requests.get(url, timeout=10)
        if response.status_code != 200:
            raise Exception(f"HTTP {response.status_code}")

        config = response.json()
        # Secondary input (channel '1') -> type 5 (FPP Remote)
        config['input_config']['channels']['1']['type'] = 5

        effects_cfg = config['input_config']['channels']['1'].get('1', {})
        effects_cfg.update({
            "type": "Effects",
            "currenteffect": "Marquee",
            "EffectSpeed": 6,
            "EffectReverse": False,
            "EffectMirror": False,
            "EffectAllLeds": False,
            "EffectBrightness": 100,
            "EffectWhiteChannel": False,
            "EffectColor": "#b700ff",
            "pixel_count": 1,
            "FlashEnable": False,
            "FlashMinInt": 100,
            "FlashMaxInt": 100,
            "FlashMinDelay": 100,
            "FlashMaxDelay": 5000,
            "FlashMinDur": 25,
            "FlashMaxDur": 50,
            "TransCount": 300,
            "TransDelay": 100,
            "effects": [
                {"name": "Solid"}, {"name": "Blink"}, {"name": "Flash"},
                {"name": "Rainbow"}, {"name": "Chase"}, {"name": "Fire flicker"},
                {"name": "Lightning"}, {"name": "Breathe"}, {"name": "Random"},
                {"name": "Transition"}, {"name": "Marquee"}
            ],
            "MarqueeGroups": [
                {"brightness": 100, "brightnessEnd": 100, "pixel_count": 5,
                 "color": {"r": 255, "g": 0, "b": 0}},
                {"brightness": 100, "brightnessEnd": 100, "pixel_count": 5,
                 "color": {"r": 0, "g": 255, "b": 0}},
                {"brightness": 100, "brightnessEnd": 100, "pixel_count": 5,
                 "color": {"r": 0, "g": 0, "b": 255}},
                {"brightness": 100, "brightnessEnd": 100, "pixel_count": 5,
                 "color": {"r": 128, "g": 128, "b": 128}},
                {"brightness": 100, "brightnessEnd": 100, "pixel_count": 5,
                 "color": {"r": 0, "g": 0, "b": 0}}
            ]
        })
        config['input_config']['channels']['1']['1'] = effects_cfg

        headers = {'Content-Type': 'application/json'}
        response = requests.post(url, headers=headers, data=json.dumps(config), timeout=10)

        if response.status_code == 200:
            msg = f"🔧 FPP + Marquee configured on {ip}"
            logger.info(msg)
            results[ip] += f" | {msg}"
        else:
            raise Exception(f"HTTP {response.status_code}")
    except Exception as e:
        msg = f"❌ Failed to configure FPP/Marquee on {ip}: {e}"
        logger.error(msg)
        results[ip] += f" | {msg}"

def update_system_config(ip, device_name, results, logger):
    """Update /conf/config.json with ID, hostname, and AP SSID."""
    url = f"http://{ip}/conf/config.json"
    try:
        response = requests.get(url, timeout=10)
        if response.status_code != 200:
            raise Exception(f"HTTP {response.status_code}")

        config = response.json()
        config['system']['device']['id'] = device_name
        config['system']['network']['hostname'] = device_name
        config['system']['network']['wifi']['ap_ssid'] = f"{device_name} - AP"

        headers = {'Content-Type': 'application/json'}
        response = requests.post(url, headers=headers, data=json.dumps(config), timeout=10)

        if response.status_code == 200:
            msg = f"🔧 System config updated on {ip} (ID/hostname/AP SSID)"
            logger.info(msg)
            results[ip] += f" | {msg}"
        else:
            raise Exception(f"HTTP {response.status_code}")
    except Exception as e:
        msg = f"❌ Failed to update config.json on {ip}: {e}"
        logger.error(msg)
        results[ip] += f" | {msg}"

def write_log(results, project_dir: Path, logger):
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = project_dir / LOG_DIR_NAME / f"{SUMMARY_PREFIX}{timestamp}.txt"
    try:
        with open(filename, "w", encoding="utf-8") as f:
            f.write("Firmware Update Summary\n")
            f.write(f"Timestamp: {timestamp}\n\n")
            for ip, result in results.items():
                f.write(f"{ip}: {result}\n")
        logger.info(f"📝 Log written to: {filename}")
    except Exception as e:
        logger.error(f"Failed writing summary log: {e}")

def main():
    project_dir = Path(os.getcwd())

    parser = argparse.ArgumentParser(
        description="Batch firmware uploader + post-config (uses FullBuild-style logging)"
    )
    parser.add_argument(
        "-f", "--firmware",
        help="Firmware filename (default: fw.efu)"
    )
    parser.add_argument(
        "-x", "--xlights",
        default=XLIGHTS_XML_PATH,
        help="Path to xLights networks XML"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose/DEBUG console output"
    )
    args = parser.parse_args()

    logger = setup_logging(project_dir, verbose=args.verbose)
    logger.info("=== Firmware Upload Session Started ===")

    firmware = args.firmware or input("Enter firmware filename [default: fw.efu]: ").strip() or "fw.efu"
    if not os.path.isfile(firmware):
        logger.error(f"❌ File not found: {firmware}")
        return

    if not os.path.isfile(args.xlights):
        logger.error(f"❌ xLights XML not found: {args.xlights}")
        return

    targets = get_target_controllers(args.xlights, logger)
    if not targets:
        logger.warning("🚫 No non-matrix controllers found.")
        return

    logger.info(f"🎯 Found {len(targets)} controller(s):")
    for idx, (ip, name) in enumerate(targets, 1):
        logger.info(f" {idx:2d}) {ip}  [{name}]")

    selection = (input(
        "\nSelect controllers to update (e.g. 1,3-5) or 'all' [default: all]: "
    ).strip() or "all")
    indices = parse_selection(selection, len(targets))
    if not indices:
        logger.error("❌ No valid selection made. Aborting.")
        return

    chosen = [targets[i] for i in indices]
    logger.info("🔧 You chose to update:")
    for ip, name in chosen:
        logger.info(f" - {ip} [{name}]")

    confirm = input("\n⚠️ Type 'yes' to proceed with these updates: ").strip().lower()
    if confirm != "yes":
        logger.warning("❌ Aborted by user.")
        return

    results = {}
    for ip, name in chosen:
        success = upload_firmware(ip, firmware, results, logger)
        if success:
            if wait_for_device(ip, logger):
                enable_fpp(ip, results, logger)
                update_system_config(ip, name, results, logger)
            else:
                results[ip] = results.get(ip, "") + " | ❌ Device never came back online."

    logger.info("\n📋 Summary of firmware updates and FPP/config.json changes:")
    for ip, status in results.items():
        logger.info(f" - {ip}: {status}")

    write_log(results, project_dir, logger)
    input("\n✅ All done. Press Enter to acknowledge and exit...")
    logger.info("=== Firmware Upload Session Finished ===")

if __name__ == "__main__":
    main()
