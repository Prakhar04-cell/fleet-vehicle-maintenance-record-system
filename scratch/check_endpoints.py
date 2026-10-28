import re

with open('web/app.js', 'r', encoding='utf-8') as f:
    content = f.read()

endpoints = re.findall(r'fetch\([\'"`]([^\'"`]+)[\'"`]', content)
print('Endpoints in app.js:')
for ep in sorted(set(endpoints)):
    print(' ', ep)
