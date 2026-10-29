import re

with open('web/app.js', encoding='utf-8') as f:
    js = f.read()

def show_func(name):
    m = re.search(r'(async\s+function\s+' + name + r'\s*\([^)]*\)\s*\{.*?\n\})', js, re.DOTALL)
    if not m:
        m = re.search(r'(function\s+' + name + r'\s*\([^)]*\)\s*\{.*?\n\})', js, re.DOTALL)
    if m:
        print(f'=== {name} ===')
        print(m.group(1)[:1200])
    else:
        print(f'{name} not found')

show_func('runDsaBenchmarks')
show_func('loadNotifications')
show_func('renderNotifications')
