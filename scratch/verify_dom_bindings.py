import re

with open('web/index.html', 'r', encoding='utf-8') as f:
    html = f.read()

with open('web/app.js', 'r', encoding='utf-8') as f:
    js = f.read()

# Extract all IDs from index.html
html_ids = set(re.findall(r'id=["\']([^"\']+)["\']', html))

# Extract all document.getElementById calls from app.js
js_get_ids = set(re.findall(r"document\.getElementById\(['\"]([^'\"]+)['\"]\)", js))

print(f"Total IDs defined in HTML: {len(html_ids)}")
print(f"Total IDs queried in JS  : {len(js_get_ids)}")

missing_ids = js_get_ids - html_ids
if missing_ids:
    print(f"\n[WARNING] Found {len(missing_ids)} IDs in app.js missing from index.html:")
    for mid in sorted(missing_ids):
        print(f"  - {mid}")
else:
    print("\n[PERFECT] 100% of IDs queried in app.js exist in index.html!")
