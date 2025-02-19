import json
import os
import sys

# Path to your settings.json file
SETTINGS_PATH = os.path.expanduser(r"C:\Users\jay\AppData\Roaming\Code\User\settings.json")

# Folders and file patterns to toggle
folders_to_toggle = [
    "**/.git",
    "**/node_modules",
    "**/dist",
    "**/.pio",
    "**/.data",
    "**/.debug",
    "**/data",
    "**/debug",
    "**/.doxygen",
    "**/.ci",
    "**/.github",
    "**/SupportingDocs",
    "**/tools",
    "*README*",
    "*.md",
    "*.json"
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

# Determine the current state of the folders
# Check if any folder is currently visible (i.e., not excluded or explicitly set to false)
any_visible = any(
    folder not in settings["files.exclude"] or settings["files.exclude"].get(folder) is False
    for folder in folders_to_toggle
)

# Toggle all folders based on the current state
new_state = any_visible  # If any folder is visible, hide all; otherwise, show all
for folder in folders_to_toggle:
    settings["files.exclude"][folder] = new_state
    print(f"Set visibility of {folder} to {'hidden' if new_state else 'visible'}")

# Save the updated settings
try:
    with open(SETTINGS_PATH, "w") as f:
        json.dump(settings, f, indent=4)
    print("Settings updated successfully.")
except Exception as e:
    print(f"Error saving settings: {e}")

