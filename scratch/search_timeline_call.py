with open('web/app.js', encoding='utf-8') as f:
    for i, line in enumerate(f, 1):
        if 'renderRecentActivityTimeline' in line:
            print(f"Line {i}: {line.strip()[:100]}")
