import os
import subprocess
import time
import urllib.request
import json

env = os.environ.copy()
env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")

proc = subprocess.Popen(
    ["fleet.exe", "--db", "data/test_live_dsa.db", "--web", "8099"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=env
)
time.sleep(1)

def req(path, method="GET", body=None, token=None):
    headers = {}
    if token: headers["Authorization"] = f"Bearer {token}"
    data = json.dumps(body).encode() if body else None
    if data: headers["Content-Type"] = "application/json"
    r = urllib.request.Request(f"http://127.0.0.1:8099{path}", data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(r) as resp:
            return resp.status, json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode())

try:
    st, res = req("/api/auth/login", "POST", {"username": "admin", "password": "Admin@123"})
    print("Login:", st, res.get("token", "")[:10])
    token = res["token"]

    st, res = req("/api/live-status", token=token)
    print("live-status:", st, res)

    st, res = req("/api/vehicles/lookup?reg=MH-01-AB-1001", token=token)
    print("lookup MH-01-AB-1001:", st, res)

    st, res = req("/api/routes/plan?from=Central%20Depot&to=Port%20Facility", token=token)
    print("routes/plan:", st, res)

    st, res = req("/api/demo/status", token=token)
    print("demo/status:", st, res)

    st, res = req("/api/demo/reset", "POST", token=token)
    print("demo/reset:", st, res)

finally:
    proc.terminate()
