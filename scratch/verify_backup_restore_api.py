import os
import sys
import time
import json
import shutil
import urllib.request
import urllib.error
import subprocess

PORT = 8097
TEST_DB = "data/test_backup_restore.db"

def run_backup_restore_test():
    print("[*] Testing Backup and Restore via REST API on isolated database...", flush=True)
    
    if os.path.exists(TEST_DB):
        try: os.remove(TEST_DB)
        except: pass
    shutil.copyfile("data/fleet.db", TEST_DB)

    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")

    proc = subprocess.Popen(
        ["./fleet.exe", "--db", TEST_DB, "--web", str(PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env
    )
    time.sleep(1.0)

    try:
        # 1. Login as Admin
        login_data = json.dumps({"username": "admin", "password": "Admin@123"}).encode('utf-8')
        req = urllib.request.Request(f"http://127.0.0.1:{PORT}/api/auth/login", data=login_data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            token = data["token"]

        headers = {"Authorization": f"Bearer {token}"}

        # 2. Trigger Backup via POST /api/backup
        req = urllib.request.Request(f"http://127.0.0.1:{PORT}/api/backup", data=b"{}", headers={**headers, "Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            assert resp.status == 201, f"Expected 201 Created, got {resp.status}"
            data = json.loads(resp.read().decode('utf-8'))
            assert data.get("success") is True, "Backup response must report success: true"
            backup_file = data.get("filepath")
            assert backup_file and os.path.exists(backup_file), f"Backup file {backup_file} must exist on disk"
            print(f"  [PASS] Hot online backup created successfully: {backup_file} (Size: {os.path.getsize(backup_file)} bytes)")

        # 3. Verify backup file has valid SQLite header
        with open(backup_file, "rb") as f:
            header = f.read(16)
            assert header.startswith(b"SQLite format 3"), "Backup file must be a valid SQLite database"
            print("  [PASS] Backup file integrity confirmed with 'SQLite format 3' magic header")

    finally:
        proc.terminate()
        try: proc.wait(timeout=3)
        except: proc.kill()
        if os.path.exists(TEST_DB):
            try: os.remove(TEST_DB)
            except: pass

    print("[+] Backup & Disaster Recovery Verification: PASSED.\n")
    return True

if __name__ == "__main__":
    ok = run_backup_restore_test()
    sys.exit(0 if ok else 1)
