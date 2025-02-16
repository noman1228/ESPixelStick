import json
import os

# Path to your settings.json file
SETTINGS_PATH = os.path.expanduser(r"C:\Users\jay\AppData\Roaming\Code\User\settings.json")

# Folders to toggle
folders_to_toggle = [
    "**/node_modules",
    "**/dist",
    "**/.pio",
    "**/data",
    "**/debug",
    "**/.doxygen",
    "**/.ci",
    "**/.github"
]

# Load the settings file
try:
    with open(SETTINGS_PATH, "r") as f:
        settings = json.load(f)
except FileNotFoundError:
    print(f"Error: The file {SETTINGS_PATH} does not exist.")
    exit(1)
except json.JSONDecodeError:
    print(f"Error: The file {SETTINGS_PATH} contains invalid JSON.")
    exit(1)

# Ensure "files.exclude" exists in settings
if "files.exclude" not in settings:
    settings["files.exclude"] = {}

# Toggle the visibility of each folder
for folder in folders_to_toggle:
    if folder in settings["files.exclude"]:
        settings["files.exclude"][folder] = not settings["files.exclude"][folder]
    else:
        settings["files.exclude"][folder] = True
    print(f"Toggled visibility of {folder}: {settings['files.exclude'][folder]}")

# Save the updated settings
try:
    with open(SETTINGS_PATH, "w") as f:
        json.dump(settings, f, indent=4)
    print("Settings updated successfully.")
except Exception as e:
    print(f"Error saving settings: {e}")