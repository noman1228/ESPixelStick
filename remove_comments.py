import re

def remove_comments(text):
    # Remove single-line comments (//)
    text = re.sub(r'//.*', '', text)
    # Remove multi-line comments (/* ... */)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    return text

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python remove_comments.py <file>")
        sys.exit(1)

    file_path = sys.argv[1]
    with open(file_path, 'r') as file:
        content = file.read()

    new_content = remove_comments(content)

    with open(file_path, 'w') as file:
        file.write(new_content)