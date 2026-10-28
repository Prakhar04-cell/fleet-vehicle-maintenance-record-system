#!/usr/bin/env python3
"""
Automated RBAC & Endpoint Authorization Verification Suite
Tests all role permissions against the live C11 HTTP Winsock2 server.
Uses a dedicated copy of the test database (data/test_rbac.db) to ensure zero pollution.
"""

import os
import sys
import time
import shutil
import urllib.request
import urllib.error
import json
import subprocess

PORT = 8089
BASE_URL = f"http://127.0.0.1:{PORT}"
TEST_DB = "data/test_rbac.db"
SEED_DB = "data/fleet.db"

passed_assertions = 0
failed_assertions = 0

def assert_true(condition, message):
    global passed_assertions, failed_assertions
    if condition:
        print(f"  [PASS] {message}")
        passed_assertions += 1
    else:
        print(f"  [FAIL] {message}")
        failed_assertions += 1

def request(path, method="GET", body=None, token=None, cookie=None):
    url = f"{BASE_URL}{path}"
    headers = {}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    if cookie:
        headers["Cookie"] = cookie
    if body is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(body).encode("utf-8")
    else:
        data = None

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            content_type = resp.headers.get("Content-Type", "")
            raw = resp.read()
            if "application/json" in content_type:
                try:
                    parsed = json.loads(raw.decode("utf-8"))
                except Exception:
                    parsed = raw.decode("utf-8")
            else:
                parsed = raw
            return resp.status, parsed, resp.headers
    except urllib.error.HTTPError as e:
        raw = e.read()
        try:
            parsed = json.loads(raw.decode("utf-8"))
        except Exception:
            parsed = raw.decode("utf-8")
        return e.code, parsed, e.headers
    except Exception as e:
        return 0, str(e), {}

def main():
    print("=" * 70)
    print(" FLEET VEHICLE MAINTENANCE RECORD SYSTEM — LIVE RBAC SECURITY TEST")
    print("=" * 70)

    # 1. Setup isolated database
    if not os.path.exists(SEED_DB):
        print(f"Error: {SEED_DB} does not exist. Run 'mingw32-make test' first.")
        sys.exit(1)
    shutil.copyfile(SEED_DB, TEST_DB)
    print(f"[*] Prepared isolated test database: {TEST_DB}")

    # 2. Launch fleet.exe on test port with MSYS2 environment PATH
    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")
    cmd = ["fleet.exe", "--db", TEST_DB, "--web", str(PORT)]
    print(f"[*] Spawning C HTTP Server: {' '.join(cmd)}")
    server_proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)

    try:
        # Wait for server to bind
        ready = False
        for _ in range(40):
            time.sleep(0.2)
            code, _, _ = request("/")
            if code == 200:
                ready = True
                break
        if not ready:
            poll = server_proc.poll()
            print(f"Fatal: C HTTP Server failed to start on port {PORT}. Poll code: {poll}")
            stdout, stderr = server_proc.communicate(timeout=2)
            print("Server stdout:", stdout.decode(errors="replace"))
            print("Server stderr:", stderr.decode(errors="replace"))
            sys.exit(1)
        print("[+] C11 HTTP server is active and responding on port", PORT)

        # ===================================================================
        # SUITE 1: ANONYMOUS / UNAUTHENTICATED REQUESTS
        # ===================================================================
        print("\n--- 1. Testing Unauthenticated Request Rejection (401 Unauthorized) ---")
        unauth_endpoints = [
            ("GET", "/api/dashboard"),
            ("GET", "/api/vehicles"),
            ("GET", "/api/drivers"),
            ("GET", "/api/maintenance"),
            ("GET", "/api/schedules"),
            ("GET", "/api/expenses"),
            ("GET", "/api/audit"),
            ("POST", "/api/backup"),
            ("GET", "/api/reports/csv?type=vehicles"),
            ("GET", "/api/reports/pdf?type=fleet"),
            ("GET", "/api/auth/session"),
            ("GET", "/api/auth/users"),
        ]
        for m, ep in unauth_endpoints:
            code, body, _ = request(ep, method=m)
            assert_true(code == 401, f"Unauthenticated {m} {ep} rejected with 401 Unauthorized (got {code})")
            if isinstance(body, dict):
                assert_true(body.get("code") == 401 or body.get("success") is False, f"  -> Response contains code 401 / success false")

        # Test fake token
        code, _, _ = request("/api/dashboard", token="deadbeef" * 8)
        assert_true(code == 401, "Forged token rejected with 401 Unauthorized")

        # ===================================================================
        # SUITE 2: MAINTENANCE STAFF ROLE VERIFICATION
        # ===================================================================
        print("\n--- 2. Testing Maintenance Staff Permissions (Role: Staff) ---")
        code, body, hdrs = request("/api/auth/login", method="POST", body={"username": "staff", "password": "Staff@123"})
        assert_true(code == 200 and body.get("success") is True, "Staff login succeeds with PBKDF2 verification")
        staff_token = body.get("token")
        assert_true(len(staff_token) == 64, "Server returned 64-character cryptographic token for Staff")
        assert_true(body.get("user", {}).get("role", "").lower() == "staff", "User profile identifies role as Staff")

        # Staff Allowed
        code, _, _ = request("/api/dashboard", token=staff_token)
        assert_true(code == 200, "Staff permitted to view Dashboard (200 OK)")
        code, _, _ = request("/api/vehicles", token=staff_token)
        assert_true(code == 200, "Staff permitted to view Vehicles register (200 OK)")
        code, _, _ = request("/api/maintenance", token=staff_token)
        assert_true(code == 200, "Staff permitted to view Maintenance history (200 OK)")
        code, _, _ = request("/api/schedules", token=staff_token)
        assert_true(code == 200, "Staff permitted to view Schedules (200 OK)")

        maint_payload = {
            "vehicle_id": 1,
            "service_type": "Brake Pad Replacement",
            "service_date": "2026-10-28",
            "odometer_km": 28500,
            "description": "Routine front axle pad replacement",
            "workshop_name": "Bay 1 In-House",
            "labour_cost": 1500,
            "parts_cost": 3200,
            "tax_amount": 0
        }
        code, _, _ = request("/api/maintenance", method="POST", body=maint_payload, token=staff_token)
        assert_true(code == 201, "Staff permitted to record workshop service work order (201 Created)")

        # Staff Forbidden (Must return 403)
        code, _, _ = request("/api/vehicles?id=1", method="DELETE", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from deleting vehicle assets (403 Forbidden)")

        code, _, _ = request("/api/drivers", method="POST", body={"name": "Test", "phone": "123", "license_number": "DL99", "license_expiry": "2028-01-01"}, token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from managing driver registrations (403 Forbidden)")

        code, _, _ = request("/api/drivers?id=1", method="DELETE", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from deleting driver accounts (403 Forbidden)")

        code, _, _ = request("/api/expenses", method="GET", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from accessing operating expenses ledger (403 Forbidden)")

        code, _, _ = request("/api/expenses", method="POST", body={"vehicle_id": 1, "category": "Fuel", "amount": 2000, "expense_date": "2026-10-28", "description": "Fuel"}, token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from creating arbitrary financial expenses (403 Forbidden)")

        code, _, _ = request("/api/audit", method="GET", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from viewing system audit history (403 Forbidden)")

        code, _, _ = request("/api/backup", method="POST", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from triggering database disaster backups (403 Forbidden)")

        code, _, _ = request("/api/reports/csv?type=vehicles", method="GET", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from downloading CSV fleet reports (403 Forbidden)")

        code, _, _ = request("/api/reports/pdf?type=fleet", method="GET", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from generating executive PDF reports (403 Forbidden)")

        code, _, _ = request("/api/auth/users", method="GET", token=staff_token)
        assert_true(code == 403, "Staff FORBIDDEN from viewing user security accounts (403 Forbidden)")

        # ===================================================================
        # SUITE 3: FLEET MANAGER ROLE VERIFICATION
        # ===================================================================
        print("\n--- 3. Testing Fleet Manager Permissions (Role: Manager) ---")
        code, body, _ = request("/api/auth/login", method="POST", body={"username": "manager", "password": "Manager@123"})
        assert_true(code == 200 and body.get("success") is True, "Manager login succeeds with PBKDF2 verification")
        mgr_token = body.get("token")
        assert_true(body.get("user", {}).get("role", "").lower() == "manager", "User profile identifies role as Manager")

        # Manager Allowed Operational
        code, _, _ = request("/api/dashboard", token=mgr_token)
        assert_true(code == 200, "Manager permitted to view Dashboard (200 OK)")

        code, _, _ = request("/api/vehicles", token=mgr_token)
        assert_true(code == 200, "Manager permitted to view Vehicles register (200 OK)")

        update_v_payload = {
            "action": "update",
            "id": 1,
            "make": "Tata",
            "model": "Prima 5530.S",
            "manufacturing_year": 2023,
            "status": "Available",
            "odometer_km": 28500,
            "fuel_percentage": 85,
            "insurance_expiry": "2027-12-31"
        }
        code, _, _ = request("/api/vehicles", method="POST", body=update_v_payload, token=mgr_token)
        assert_true(code == 200, "Manager permitted to update vehicle operational details (200 OK)")

        code, _, _ = request("/api/drivers", token=mgr_token)
        assert_true(code == 200, "Manager permitted to view Commercial Drivers (200 OK)")

        driver_payload = {
            "name": "Vikram Rathore",
            "phone": "+91 98765 43210",
            "license_number": "DL-0420230099881",
            "license_expiry": "2029-05-15"
        }
        code, _, _ = request("/api/drivers", method="POST", body=driver_payload, token=mgr_token)
        assert_true(code == 201, "Manager permitted to add new commercial driver (201 Created)")

        code, _, _ = request("/api/expenses", token=mgr_token)
        assert_true(code == 200, "Manager permitted to view Expenses (200 OK)")

        exp_payload = {
            "vehicle_id": 1,
            "category": "Toll/Tax",
            "amount": 450,
            "expense_date": "2026-10-28",
            "description": "Expressway Toll Plaza"
        }
        code, _, _ = request("/api/expenses", method="POST", body=exp_payload, token=mgr_token)
        assert_true(code == 201, "Manager permitted to record operational expenditures (201 Created)")

        code, _, _ = request("/api/reports/csv?type=vehicles", token=mgr_token)
        assert_true(code == 200, "Manager permitted to export Vehicles CSV report (200 OK)")

        code, _, _ = request("/api/reports/pdf?type=fleet", token=mgr_token)
        assert_true(code == 200, "Manager permitted to export Fleet Summary PDF report (200 OK)")

        # Manager Forbidden (Must return 403)
        code, _, _ = request("/api/vehicles?id=1", method="DELETE", token=mgr_token)
        assert_true(code == 403, "Manager FORBIDDEN from permanently deleting vehicle assets (403 Forbidden)")

        code, _, _ = request("/api/drivers?id=1", method="DELETE", token=mgr_token)
        assert_true(code == 403, "Manager FORBIDDEN from permanently deleting driver profiles (403 Forbidden)")

        code, _, _ = request("/api/audit", method="GET", token=mgr_token)
        assert_true(code == 403, "Manager FORBIDDEN from viewing system security audit trail (403 Forbidden)")

        code, _, _ = request("/api/backup", method="POST", token=mgr_token)
        assert_true(code == 403, "Manager FORBIDDEN from triggering backup/restore operations (403 Forbidden)")

        code, _, _ = request("/api/auth/users", method="GET", token=mgr_token)
        assert_true(code == 403, "Manager FORBIDDEN from managing user security accounts (403 Forbidden)")

        # ===================================================================
        # SUITE 4: ADMINISTRATOR ROLE VERIFICATION
        # ===================================================================
        print("\n--- 4. Testing Administrator Permissions (Role: Admin) ---")
        code, body, _ = request("/api/auth/login", method="POST", body={"username": "admin", "password": "Admin@123"})
        assert_true(code == 200 and body.get("success") is True, "Admin login succeeds with PBKDF2 verification")
        admin_token = body.get("token")
        assert_true(body.get("user", {}).get("role", "").lower() == "admin", "User profile identifies role as Admin")

        # Admin Allowed System & Security
        code, body, _ = request("/api/audit", token=admin_token)
        assert_true(code == 200 and isinstance(body.get("data"), list), "Admin permitted to inspect live Security Audit trail (200 OK)")

        code, body, _ = request("/api/backup", method="POST", token=admin_token)
        assert_true(code == 201 and "filepath" in body, "Admin permitted to trigger SQLite hot backup (201 Created)")

        code, body, _ = request("/api/auth/users", token=admin_token)
        assert_true(code == 200 and len(body.get("data", [])) >= 3, "Admin permitted to list and manage system user accounts (200 OK)")

        # ===================================================================
        # SUITE 5: LOGOUT & SESSION INVALIDATION
        # ===================================================================
        print("\n--- 5. Testing Logout & Server-Side Session Invalidation ---")
        code, body, hdrs = request("/api/auth/logout", method="POST", token=admin_token)
        assert_true(code == 200 and body.get("success") is True, "Logout endpoint invalidates session on server")
        assert_true("Max-Age=0" in hdrs.get("Set-Cookie", ""), "Set-Cookie header expires session_token cookie")

        code, _, _ = request("/api/dashboard", token=admin_token)
        assert_true(code == 401, "Destroyed token cannot access protected endpoints (401 Unauthorized)")

        code, _, _ = request("/api/auth/session", token=admin_token)
        assert_true(code == 401, "Destroyed token is rejected by session validation endpoint (401 Unauthorized)")

    finally:
        print("\n[*] Terminating C HTTP Server process...")
        server_proc.terminate()
        server_proc.wait(timeout=5)
        if os.path.exists(TEST_DB):
            try:
                os.remove(TEST_DB)
            except Exception:
                pass

    print("\n" + "=" * 70)
    print(f" RBAC TEST RESULTS: {passed_assertions} Passed, {failed_assertions} Failed")
    print("=" * 70)
    if failed_assertions > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
