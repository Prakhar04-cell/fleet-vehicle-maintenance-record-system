with open('src/http_server.c', 'r', encoding='utf-8') as f:
    for i, line in enumerate(f, 1):
        if 'handle_api_notifications' in line or 'handle_api_vehicle_lookup' in line or 'handle_api_routes_plan' in line:
            print(f"Line {i}: {line.strip()[:100]}")
