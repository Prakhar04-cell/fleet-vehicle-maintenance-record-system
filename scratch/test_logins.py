import urllib.request, json

test_users = [
    ('admin', 'Admin@123', 200, True),
    ('manager', 'Manager@123', 200, True),
    ('staff', 'Staff@123', 200, True),
    ('admin', 'WrongPass123', 401, False),
    ('nonexistent', 'Random123', 401, False),
]

all_passed = True
for u, p, expected_status, should_succeed in test_users:
    req = urllib.request.Request(
        'http://127.0.0.1:8080/api/auth/login',
        data=json.dumps({'username': u, 'password': p}).encode('utf-8'),
        headers={'Content-Type': 'application/json'}
    )
    try:
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            ok = (resp.status == expected_status) and (data.get('success') == should_succeed)
            print(f"[{'PASS' if ok else 'FAIL'}] User '{u}' (pw='{p}'): status={resp.status}, success={data.get('success')}, role={data.get('user', {}).get('role')}")
            if not ok: all_passed = False
    except urllib.error.HTTPError as e:
        data = json.loads(e.read().decode('utf-8'))
        ok = (e.code == expected_status) and (data.get('success') == should_succeed)
        print(f"[{'PASS' if ok else 'FAIL'}] User '{u}' (pw='{p}'): status={e.code}, success={data.get('success')}, message='{data.get('message')}'")
        if not ok: all_passed = False

if all_passed:
    print("\nALL LOGIN & AUTHENTICATION TESTS PASSED 100%!")
else:
    print("\nSOME LOGIN TESTS FAILED!")
