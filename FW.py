import xml.etree.ElementTree as ET
import requests
import os
import json
import time
from datetime import datetime
from requests_toolbelt.multipart.encoder import MultipartEncoder

XLIGHTS_XML_PATH = r"C:\Users\jay\desktop\Lights 2024\xlights_networks.xml"

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

def get_target_controllers(xml_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()
    controllers = root.findall(".//Controller")

    targets = []
    for ctrl in controllers:
        ip = ctrl.get("IP")
        if not ip:
            continue
        if is_matrix_controller(ctrl):
            print(f"🚫 Skipping matrix/FPP controller: {ip} [{ctrl.get('Name','')}]")
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

def upload_firmware(ip, firmware_path, results):
    url = f"http://{ip}/updatefw"
    try:
        encoder = MultipartEncoder(
            fields={'file': (os.path.basename(firmware_path),
                             open(firmware_path, 'rb'),
                             'application/octet-stream')}
        )
        print(f"📤 Uploading '{firmware_path}' to {ip}...")
        resp = requests.post(
            url, data=encoder,
            headers={'Content-Type': encoder.content_type},
            timeout=15
        )
        if resp.status_code == 200:
            msg = f"✅ Update success on {ip}. Waiting for device to reboot..."
            print(msg)
            results[ip] = msg
            return True
        else:
            msg = f"⚠️ Update failed on {ip}. HTTP {resp.status_code}: {resp.text.strip()}"
            print(msg)
            results[ip] = msg
            return False
    except Exception as e:
        msg = f"❌ {ip} error: {e}"
        print(msg)
        results[ip] = msg
        return False

def wait_for_device(ip, timeout=180, delay=5):
    """Poll until device responds or timeout expires."""
    url = f"http://{ip}/conf/input_config.json"
    print(f"🔄 Waiting for {ip} to come back online...")
    start = time.time()
    while time.time() - start < timeout:
        try:
            resp = requests.get(url, timeout=5)
            if resp.status_code == 200:
                print(f"✅ {ip} is back online.")
                return True
        except requests.RequestException:
            pass
        time.sleep(delay)
    print(f"❌ Timeout waiting for {ip} to come back online.")
    return False

def enable_fpp(ip, results):
    """Set secondary input to FPP Remote and configure Marquee effect groups."""
    url = f"http://{ip}/conf/input_config.json"
    try:
        response = requests.get(url, timeout=10)
        if response.status_code != 200:
            raise Exception(f"HTTP {response.status_code}")

        config = response.json()
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
            print(msg)
            results[ip] += f" | {msg}"
        else:
            raise Exception(f"HTTP {response.status_code}")
    except Exception as e:
        msg = f"❌ Failed to configure FPP/Marquee on {ip}: {e}"
        print(msg)
        results[ip] += f" | {msg}"

def update_system_config(ip, device_name, results):
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
            print(msg)
            results[ip] += f" | {msg}"
        else:
            raise Exception(f"HTTP {response.status_code}")
    except Exception as e:
        msg = f"❌ Failed to update config.json on {ip}: {e}"
        print(msg)
        results[ip] += f" | {msg}"

def write_log(results):
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"upload_log_{timestamp}.txt"
    with open(filename, "w", encoding="utf-8") as f:
        f.write("Firmware Update Summary\n")
        f.write(f"Timestamp: {timestamp}\n\n")
        for ip, result in results.items():
            f.write(f"{ip}: {result}\n")
    print(f"\n📝 Log written to: {filename}")

def main():
    firmware = input("Enter firmware filename [default: fw.efu]: ").strip() or "fw.efu"
    if not os.path.isfile(firmware):
        print(f"❌ File not found: {firmware}")
        return

    targets = get_target_controllers(XLIGHTS_XML_PATH)
    if not targets:
        print("🚫 No non-matrix controllers found.")
        return

    print(f"\n🎯 Found {len(targets)} controller(s):")
    for idx, (ip, name) in enumerate(targets, 1):
        print(f" {idx:2d}) {ip}  [{name}]")

    selection = input(
        "\nSelect controllers to update (e.g. 1,3-5) or 'all' [default: all]: "
    ).strip() or "all"
    indices = parse_selection(selection, len(targets))
    if not indices:
        print("❌ No valid selection made. Aborting.")
        return

    chosen = [targets[i] for i in indices]
    print("\n🔧 You chose to update:")
    for ip, name in chosen:
        print(f" - {ip} [{name}]")

    confirm = input("\n⚠️ Type 'yes' to proceed with these updates: ").strip().lower()
    if confirm != "yes":
        print("❌ Aborted by user.")
        return

    results = {}
    for ip, name in chosen:
        success = upload_firmware(ip, firmware, results)
        if success:
            if wait_for_device(ip):
                enable_fpp(ip, results)
                update_system_config(ip, name, results)
            else:
                results[ip] += " | ❌ Device never came back online."

    print("\n📋 Summary of firmware updates and FPP/config.json changes:")
    for ip, status in results.items():
        print(f" - {ip}: {status}")

    write_log(results)
    input("\n✅ All done. Press Enter to acknowledge and exit...")

if __name__ == "__main__":
    main()
