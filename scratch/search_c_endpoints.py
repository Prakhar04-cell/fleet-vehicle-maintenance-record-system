with open('src/http_server.c', 'r', encoding='utf-8') as f:
    for i, line in enumerate(f, 1):
        if 'api/' in line or 'live' in line.lower() or 'demo' in line.lower():
            if 'void' in line or 'static' in line or 'strcmp' in line or 'strstr' in line:
                print(f"Line {i}: {line.strip()[:100]}")
