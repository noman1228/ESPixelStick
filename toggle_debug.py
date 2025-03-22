import re
import sys

def toggle_debug(text):
    """
    Toggles debug print statements:
    - `// DEBUG_` → `DEBUG_` (enable debugging)
    - `DEBUG_` → `// DEBUG_` (disable debugging)
    - Tracks state using `// DEBUG_TOGGLE_ENABLED` or `// DEBUG_TOGGLE_DISABLED`
    - Ensures no double comments (`// // DEBUG_`)
    """
    debug_enabled = "// DEBUG_TOGGLE_ENABLED" in text
    debug_disabled = "// DEBUG_TOGGLE_DISABLED" in text

    # Ensure we track the toggle state correctly
    if not debug_enabled and not debug_disabled:
        text += "\n// DEBUG_TOGGLE_ENABLED\n"  # Default state

    if debug_enabled:
        # Enable debugging: Remove one level of `//` if it exists
        text = re.sub(r'(^|\s)// (DEBUG_)', r'\1\2', text)  # `// DEBUG_` → `DEBUG_`
        text = text.replace("// DEBUG_TOGGLE_ENABLED", "// DEBUG_TOGGLE_DISABLED")  # Update toggle state
    else:
        # Disable debugging: Ensure only a single `//` is added
        text = re.sub(r'(^|\s)(?<!// )(DEBUG_)', r'\1// \2', text)  # `DEBUG_` → `// DEBUG_`
        text = text.replace("// DEBUG_TOGGLE_DISABLED", "// DEBUG_TOGGLE_ENABLED")  # Update toggle state

    return text

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python toggle_debug.py <file>")
        sys.exit(1)

    file_path = sys.argv[1]

    with open(file_path, "r", encoding="utf-8") as file:
        content = file.read()

    new_content = toggle_debug(content)

    with open(file_path, "w", encoding="utf-8") as file:
        file.write(new_content)

    print(f"Debug toggled in {file_path}")
