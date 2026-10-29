with open('web/index.html', encoding='utf-8') as f:
    for i, line in enumerate(f, 1):
        if 'timeline' in line or 'quick' in line or 'recent' in line.lower():
            if 'id=' in line:
                print(f"Line {i}: {line.strip()[:100]}")
