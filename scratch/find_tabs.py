import re

with open('web/index.html', encoding='utf-8') as f:
    lines = f.readlines()

for i, line in enumerate(lines, 1):
    m = re.search(r'id=["\'](tab-[a-zA-Z0-9_-]+)["\']', line)
    if m:
        print(f"Line {i}: {m.group(1)} --> {line.strip()[:80]}")
