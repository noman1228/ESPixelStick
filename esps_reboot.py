# esps_rebooter.py
# quick little read of your controller config and away we go!
import xml.etree.ElementTree as ET
import requests
import webbrowser

# Your xLights config path
XML_FILE = r"C:\Users\jay\Desktop\Lights 2024\xlights_networks.xml"
COMMON_PATH = "/X6"  # or whatever endpoint you're POSTing to
POST_PAYLOAD = {}  # Add your POST payload here if needed

def extract_controller_ips(xml_path):
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()

        ips = []
        for controller in root.findall("Controller"):
            ip = controller.get("IP")
            if ip:
                ips.append(ip)

        return ips

    except ET.ParseError as e:
        print(f"[ERROR] XML parse error: {e}")
    except FileNotFoundError:
        print("[ERROR] File not found.")
    except Exception as e:
        print(f"[ERROR] Unexpected exception: {e}")
    return []

def post_status_to_ips(ip_list, path, payload):
    for ip in ip_list:
        url = f"http://{ip}{path}"
        try:
            response = requests.post(url, json=payload, timeout=3)
            print(f"[OK] POST {url} -> {response.status_code}")
            try:
                print(response.json())
            except ValueError:
                print(response.text)
        except requests.RequestException as e:
            print(f"[FAIL] POST {url} -> {e}")

def open_controllers_in_browser(ip_list):
    print("🌐 Opening each controller in your default browser...")
    for ip in ip_list:
        web_url = f"http://{ip}/"
        print(f"🧭 Launching {web_url}")
        webbrowser.open(web_url)

if __name__ == "__main__":
    print("📦 Parsing xLights network config...")
    ips = extract_controller_ips(XML_FILE)
    
    if ips:
        print(f"📡 Found {len(ips)} controller IP(s): {ips}")
        print("📨 Sending POST to each controller...\n")
        post_status_to_ips(ips, COMMON_PATH, POST_PAYLOAD)
        print("\n🖥️ Spawning browser tabs for each controller...\n")
        open_controllers_in_browser(ips)
    else:
        print("⚠️ No controller IPs found.")
