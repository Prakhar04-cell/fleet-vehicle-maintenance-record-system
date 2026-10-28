import urllib.request

paths = [
    '/',
    '/dashboard.css',
    '/app.js',
    '/favicon.ico',
    '/api/dashboard',
    '/api/vehicles',
    '/api/drivers',
    '/api/maintenance',
    '/api/schedules',
    '/api/expenses',
    '/api/notifications',
    '/api/dsa/run',
    '/api/audit',
    '/api/reports/csv?type=vehicles',
    '/api/reports/csv?type=maintenance',
    '/api/reports/csv?type=expenses',
    '/api/reports/csv?type=schedules',
    '/api/reports/csv?type=spending',
    '/api/reports/csv?type=vehicle_costs',
    '/api/reports/pdf?type=fleet',
    '/api/reports/pdf?type=expenses',
    '/api/reports/pdf?type=maintenance&vehicle_id=1',
    '/api/users'
]

print(f"{'Endpoint':42} | {'Status'} | {'Bytes':>8} | {'Content-Type'}")
print("-" * 80)
for p in paths:
    try:
        req = urllib.request.Request('http://127.0.0.1:8080' + p)
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = resp.read()
            ct = resp.headers.get('Content-Type', '')
            print(f"{p:42} | {resp.status:6} | {len(data):8} | {ct}")
    except Exception as e:
        print(f"{p:42} | ERROR  | {str(e)}")
