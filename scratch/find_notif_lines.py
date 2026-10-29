import re

with open('web/app.js', encoding='utf-8') as f:
    lines = f.readlines()

for i, l in enumerate(lines, 1):
    if 'function loadNotifications' in l or 'function renderNotifications' in l:
        print(f"Line {i}: {l.strip()}")
