import re

with open('web/index.html', encoding='utf-8') as f:
    html = f.read()

dsa_match = re.search(r'(<section[^>]*id=["\']tab-dsa["\'].*?</section>)', html, re.DOTALL)
if dsa_match:
    print('tab-dsa length:', len(dsa_match.group(1)))
    lines = dsa_match.group(1).splitlines()
    print('First 30 lines of tab-dsa:')
    for l in lines[:30]:
        print(l)
else:
    print('tab-dsa not found')
