with open('web/app.js', encoding='utf-8') as f:
    for i, line in enumerate(f, 1):
        for mid in ['btn-mgr-quick', 'timeline-recent-activity']:
            if mid in line:
                print(f"Line {i}: {line.strip()[:100]}")
