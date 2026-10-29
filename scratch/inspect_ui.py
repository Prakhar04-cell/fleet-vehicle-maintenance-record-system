import re

with open('web/index.html', encoding='utf-8') as f:
    html = f.read()
print('HTML length:', len(html))
tabs = re.findall(r'id=["\']tab-([a-zA-Z0-9_-]+)["\']', html)
print('Tabs found:', tabs)

with open('web/app.js', encoding='utf-8') as f:
    js = f.read()
print('JS length:', len(js))

# Check existing DSA functions in app.js
dsa_funcs = re.findall(r'function\s+([a-zA-Z0-9_]*dsa[a-zA-Z0-9_]*)\s*\(', js, re.IGNORECASE)
print('DSA functions in JS:', dsa_funcs)

# Check notification functions in app.js
notif_funcs = re.findall(r'function\s+([a-zA-Z0-9_]*notif[a-zA-Z0-9_]*)\s*\(', js, re.IGNORECASE)
print('Notification functions in JS:', notif_funcs)
