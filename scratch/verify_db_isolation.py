import os
import sys
import time
import json
import shutil
import hashlib
import urllib.request
import urllib.error
import subprocess

PORT_PROD = 8094
PORT_DEMO = 8095
TEST_PROD_DB = "data/test_prod_isolated.db"

def get_file_md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        while chunk := f.read(8192):
            h.update(chunk)
    return h.hexdigest()

def verify_isolation():
    print("[*] Verifying Database Separation & Isolation Protection...", flush=True)
    
    # Preserve production database MD5
    prod_md5_before = get_file_md5("data/fleet.db")
    
    # Prepare isolated test copy of production DB
    if os.path.exists(TEST_PROD_DB):
        os.remove(TEST_PROD_DB)
    shutil.copyfile("data/fleet.db", TEST_PROD_DB)
    
    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")

    # 1. Start Server on Isolated Production DB
    print(f"[*] Starting Server on Isolated Production DB ({TEST_PROD_DB}) on port {PORT_PROD}...", flush=True)
    p_prod = subprocess.Popen(
        ["./fleet.exe", "--db", TEST_PROD_DB, "--web", str(PORT_PROD)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env
    )
    time.sleep(1.0)

    try:
        # Login as Admin on Isolated Prod
        login_data = json.dumps({"username": "admin", "password": "Admin@123"}).encode('utf-8')
        req = urllib.request.Request(f"http://127.0.0.1:{PORT_PROD}/api/auth/login", data=login_data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            prod_token = data["token"]

        headers_prod = {"Authorization": f"Bearer {prod_token}"}

        # Check Demo Status on Prod server
        req = urllib.request.Request(f"http://127.0.0.1:{PORT_PROD}/api/demo/status", headers=headers_prod)
        with urllib.request.urlopen(req) as resp:
            stat = json.loads(resp.read().decode('utf-8'))
            assert stat.get("is_demo_mode") is False, "Production server must report is_demo_mode: false"
            assert stat.get("can_reset") is False, "Production server must report can_reset: false"
            print("  [PASS] /api/demo/status on production correctly reports is_demo_mode=False, can_reset=False")

        # Attempt POST /api/demo/reset on Prod server (MUST BE BLOCKED WITH 403)
        req = urllib.request.Request(f"http://127.0.0.1:{PORT_PROD}/api/demo/reset", data=b"{}", headers={**headers_prod, "Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req) as resp:
                assert False, "Resetting non-demo database must NEVER succeed!"
        except urllib.error.HTTPError as e:
            assert e.code == 403, f"Expected 403 Forbidden, got {e.code}"
            body = json.loads(e.read().decode('utf-8'))
            assert "Safety Violation: Only demo_fleet.db may be reset" in body.get("message", ""), "Violation message must be returned"
            print("  [PASS] POST /api/demo/reset on production copy strictly blocked with 403 Forbidden")

    finally:
        p_prod.terminate()
        try: p_prod.wait(timeout=3)
        except: p_prod.kill()
        if os.path.exists(TEST_PROD_DB):
            try: os.remove(TEST_PROD_DB)
            except: pass

    # Verify real production database remained completely untouched
    prod_md5_after = get_file_md5("data/fleet.db")
    assert prod_md5_before == prod_md5_after, "Production database MD5 hash MUST remain identical!"
    print("  [PASS] Production database data/fleet.db byte-for-byte untouched during production test")

    # 2. Start Server in Safe Demo Mode
    print(f"[*] Starting Server in Safe Demo Mode on port {PORT_DEMO}...", flush=True)
    p_demo = subprocess.Popen(
        ["./fleet.exe", "--demo", "--web", str(PORT_DEMO)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env
    )
    time.sleep(1.0)

    try:
        # Login as Admin on Demo
        login_data = json.dumps({"username": "admin", "password": "Admin@123"}).encode('utf-8')
        req = urllib.request.Request(f"http://127.0.0.1:{PORT_DEMO}/api/auth/login", data=login_data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            demo_token = data["token"]

        headers_demo = {"Authorization": f"Bearer {demo_token}"}

        # Check Demo Status on Demo server
        req = urllib.request.Request(f"http://127.0.0.1:{PORT_DEMO}/api/demo/status", headers=headers_demo)
        with urllib.request.urlopen(req) as resp:
            stat = json.loads(resp.read().decode('utf-8'))
            assert stat.get("is_demo_mode") is True, "Demo server must report is_demo_mode: true"
            assert stat.get("can_reset") is True, "Demo server must report can_reset: true"
            assert "demo_fleet.db" in stat.get("database_path", ""), "Active database must be demo_fleet.db"
            print("  [PASS] /api/demo/status in demo mode correctly reports is_demo_mode=True, can_reset=True, path=demo_fleet.db")

        # Reset Demo Database (MUST SUCCEED ON demo_fleet.db)
        req = urllib.request.Request(f"http://127.0.0.1:{PORT_DEMO}/api/demo/reset", data=b"{}", headers={**headers_demo, "Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            res = json.loads(resp.read().decode('utf-8'))
            assert resp.status == 200 and res.get("success") is True, "Reset on demo_fleet.db must succeed"
            print("  [PASS] POST /api/demo/reset succeeds on data/demo_fleet.db")

    finally:
        p_demo.terminate()
        try: p_demo.wait(timeout=3)
        except: p_demo.kill()

    # Re-verify production database remained completely untouched even after demo reset
    prod_md5_final = get_file_md5("data/fleet.db")
    assert prod_md5_before == prod_md5_final, "Production database MD5 MUST remain unchanged after demo reset!"
    print("  [PASS] Production database data/fleet.db completely isolated, intact and unharmed")
    print("\n[+] Database Separation & Isolation: 100% VERIFIED SUCCESSFUL.")
    return True

if __name__ == "__main__":
    ok = verify_isolation()
    sys.exit(0 if ok else 1)
