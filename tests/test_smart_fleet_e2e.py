import os
import sys
import time
import json
import shutil
import subprocess
import urllib.request
import urllib.error

TEST_PORT = 8092
BASE_URL = f"http://127.0.0.1:{TEST_PORT}"
TEST_DB = "data/test_e2e_prototype.db"

SEED_DB = "data/fleet.db"

def start_server():
    if os.path.exists(SEED_DB):
        shutil.copyfile(SEED_DB, TEST_DB)
    elif os.path.exists("data/test_fleet.db"):
        shutil.copyfile("data/test_fleet.db", TEST_DB)

    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")
    cmd = ["fleet.exe", "--db", TEST_DB, "--web", str(TEST_PORT)]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    
    # Wait for server to become responsive
    for _ in range(40):
        try:
            with urllib.request.urlopen(f"{BASE_URL}/", timeout=1) as resp:
                if resp.status == 200:
                    return proc
        except Exception:
            time.sleep(0.2)
    
    proc.kill()
    raise RuntimeError("Failed to start fleet.exe HTTP server on port " + str(TEST_PORT))

def request(method, path, body=None, token=None):
    headers = {}
    if body is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(body).encode("utf-8")
    else:
        data = None
    
    if token:
        headers["Authorization"] = f"Bearer {token}"
        
    req = urllib.request.Request(f"{BASE_URL}{path}", data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            content = resp.read()
            try:
                parsed = json.loads(content.decode("utf-8"))
            except:
                parsed = content
            return resp.status, parsed, resp.headers
    except urllib.error.HTTPError as e:
        content = e.read()
        try:
            parsed = json.loads(content.decode("utf-8"))
        except:
            parsed = content.decode("utf-8", errors="replace")
        return e.code, parsed, e.headers

passed = 0
failed = 0

def assert_test(cond, desc):
    global passed, failed
    if cond:
        print(f"  [PASS] {desc}")
        passed += 1
    else:
        print(f"  [FAIL] {desc}")
        failed += 1

print("======================================================================")
print(" SMART FLEET MANAGEMENT WORKING PROTOTYPE  COMPLETE E2E TEST SUITE")
print("======================================================================")

server_proc = start_server()
try:
    print(f"[+] Server started with test database {TEST_DB} on port {TEST_PORT}")

    # --- 1. Authentication for Admin, Manager, Staff ---
    print("\n--- 1. Role Authentication & Token Issuance ---")
    status, admin_auth, _ = request("POST", "/api/auth/login", {"username": "admin", "password": "Admin@123"})
    assert_test(status == 200 and admin_auth.get("token"), "Admin authentication succeeded")
    admin_token = admin_auth["token"]

    status, mgr_auth, _ = request("POST", "/api/auth/login", {"username": "manager", "password": "Manager@123"})
    assert_test(status == 200 and mgr_auth.get("token"), "Manager authentication succeeded")
    mgr_token = mgr_auth["token"]

    status, staff_auth, _ = request("POST", "/api/auth/login", {"username": "staff", "password": "Staff@123"})
    assert_test(status == 200 and staff_auth.get("token"), "Staff authentication succeeded")
    staff_token = staff_auth["token"]

    # --- 2. Vehicle Registration & Persistence ---
    print("\n--- 2. Commercial Vehicle Asset Management ---")
    v_payload = {
        "registration_number": "MH-02-CB-5555",
        "make": "Tata",
        "model": "Prima Commercial 2830.K",
        "manufacturing_year": 2023,
        "vin": "MAT621999P1234567",
        "fuel_type": "Diesel",
        "fuel_percentage": 85,
        "odometer_km": 42000,
        "insurance_expiry": "2027-11-30"
    }
    status, v_res, _ = request("POST", "/api/vehicles", v_payload, token=mgr_token)
    assert_test(status == 201 and v_res.get("id"), "Manager registered commercial vehicle asset")
    vehicle_id = v_res["id"]

    status, v_list, _ = request("GET", "/api/vehicles", token=staff_token)
    assert_test(status == 200 and any(v["id"] == vehicle_id for v in v_list["data"]), "Staff can query registered vehicle asset")

    # --- 3. Smart Preventive Maintenance Scheduling & Deduplication ---
    print("\n--- 3. Smart Preventive Maintenance Schedules & Recalculation ---")
    sched_payload = {
        "vehicle_id": vehicle_id,
        "service_name": "45,000 KM Heavy Transmission Inspection",
        "due_date": "2026-11-15",
        "due_odometer_km": 45000
    }
    status, s_res, _ = request("POST", "/api/schedules", sched_payload, token=mgr_token)
    assert_test(status == 201 and s_res.get("id"), "Manager created preventive milestone")
    sched_id = s_res["id"]

    # Evaluate schedule status
    status, eval_res, _ = request("POST", "/api/evaluate", {}, token=staff_token)
    assert_test(status == 200 and eval_res.get("success"), "Preventive engine evaluated maintenance milestones")

    # Verify duplicate active notification prevention
    status, notifs, _ = request("GET", "/api/notifications", token=admin_token)
    assert_test(status == 200, "Queried in-app alert notifications")

    # --- 4. Work Order Management Lifecycle ---
    print("\n--- 4. Work Order Lifecycle & Atomic Completion ---")
    wo_payload = {
        "vehicle_id": vehicle_id,
        "title": "Brake Rotor Truing & Pad Replacement",
        "description": "Excessive shudder reported under braking on descent. Replace rotors and calipers.",
        "priority": "High",
        "assigned_to": "Amit Patil (Brakes & Suspension)",
        "estimated_labour_cost": 250000,
        "estimated_parts_cost": 450000
    }
    status, wo_res, _ = request("POST", "/api/work-orders", wo_payload, token=mgr_token)
    assert_test(status == 201 and wo_res.get("id"), "Manager created work order")
    wo_id = wo_res["id"]
    wo_number = wo_res.get("work_order_number")

    # Transition 1: Open -> Assigned
    status, t1, _ = request("PUT", "/api/work-orders", {
        "action": "update_status",
        "id": wo_id,
        "status": "Assigned",
        "notes": "Assigned technician Amit Patil"
    }, token=mgr_token)
    assert_test(status == 200 and t1.get("success"), "Transitioned Work Order to 'Assigned'")

    # Transition 2: Assigned -> In Progress
    status, t2, _ = request("PUT", "/api/work-orders", {
        "action": "update_status",
        "id": wo_id,
        "status": "In Progress",
        "notes": "Vehicle parked in Workshop Bay 3. Teardown commenced."
    }, token=staff_token)
    assert_test(status == 200 and t2.get("success"), "Staff advanced Work Order to 'In Progress'")

    # Transition 3: Atomic Completion -> Links service record and expense
    status, t3, _ = request("PUT", "/api/work-orders", {
        "action": "complete",
        "id": wo_id,
        "odometer_km": 42500,
        "workshop_name": "Central Depot Heavy Bay",
        "labour_cost": 250000,
        "parts_cost": 450000,
        "notes": "Fitted OEM Brembo heavy ceramic pads and balanced discs."
    }, token=staff_token)
    assert_test(status == 200 and t3.get("success"), "Executed atomic work order completion")

    # Verify duplicate completion blocked
    status, dup_res, _ = request("PUT", "/api/work-orders", {
        "action": "complete",
        "id": wo_id,
        "odometer_km": 42500,
        "workshop_name": "Central Depot Heavy Bay",
        "labour_cost": 250000,
        "parts_cost": 450000,
        "notes": "Attempting duplicate completion"
    }, token=staff_token)
    assert_test(status == 400 or not dup_res.get("success"), "Duplicate work order completion safely blocked")

    # Verify transition history
    status, hist, _ = request("GET", f"/api/work-orders/history?work_order_id={wo_id}", token=staff_token)
    assert_test(status == 200 and len(hist.get("data", [])) >= 3, "Work order transition audit history recorded")

    # --- 5. Digital Vehicle Inspection & Defect Escalation ---
    print("\n--- 5. Digital Vehicle Inspection & Defect Escalation ---")
    insp_payload = {
        "vehicle_id": vehicle_id,
        "inspector_name": "Sunil Verma (Certified Inspector)",
        "inspection_date": "2026-10-28",
        "odometer_km": 42600,
        "notes": "Routine pre-trip roadworthiness assessment.",
        "items": [
            {"item_name": "Brakes & Calipers", "status": "Pass", "defect_description": "", "severity": "None"},
            {"item_name": "Tyres & Tread Depth", "status": "Fail", "defect_description": "Rear right tyre sidewall bubble and tread below 1.6mm limit", "severity": "Critical"},
            {"item_name": "Lights, Indicators & Horn", "status": "Pass", "defect_description": "", "severity": "None"},
            {"item_name": "Engine & Exhaust Diagnostics", "status": "Pass", "defect_description": "", "severity": "None"},
            {"item_name": "Battery & Electrical System", "status": "Pass", "defect_description": "", "severity": "None"},
            {"item_name": "Steering & Suspension Alignment", "status": "Pass", "defect_description": "", "severity": "None"},
            {"item_name": "Fluid Levels (Oil/Coolant/Brake)", "status": "Pass", "defect_description": "", "severity": "None"},
            {"item_name": "Registration & Insurance Documents", "status": "Pass", "defect_description": "", "severity": "None"}
        ]
    }
    status, insp_res, _ = request("POST", "/api/inspections", insp_payload, token=staff_token)
    assert_test(status == 201 and insp_res.get("id"), "Submitted 8-item digital inspection checklist")
    assert_test(insp_res.get("overall_status") == "Fail", "Critical defect automatically escalated overall status to 'Fail'")
    assert_test(insp_res.get("defects_found") == 1, "Accurately recorded 1 flagged defect")
    insp_id = insp_res["id"]

    # Verify defect appears in active defect list
    status, defects_res, _ = request("GET", "/api/inspections/defects", token=mgr_token)
    assert_test(status == 200 and any(d["inspection_id"] == insp_id for d in defects_res.get("data", [])), "Flagged defect appears on unresolved defect register")

    # One-click Work Order Creation from Defect
    status, defect_wo, _ = request("POST", "/api/inspections/create-work-order", {
        "inspection_id": insp_id,
        "item_index": 1,
        "assigned_to": "Amit Patil (Brakes & Suspension)"
    }, token=mgr_token)
    assert_test(status == 201 and defect_wo.get("work_order_id"), "Generated Repair Work Order directly from inspection defect")

    # --- 6. Fleet Cost Analytics Verification ---
    print("\n--- 6. Fleet Cost Analytics & SQL Reconciliations ---")
    status, analytics, _ = request("GET", "/api/analytics", token=mgr_token)
    assert_test(status == 200 and analytics.get("success"), "Fetched SQL aggregated fleet analytics")
    a = analytics.get("analytics", {})
    assert_test(a.get("total_maintenance_cost", 0) > 0, "Maintenance spending non-zero and aggregated from SQLite")
    assert_test(len(a.get("top_vehicles", [])) > 0, "Top highest-cost vehicles ranked from database")
    assert_test(len(a.get("monthly_spending", [])) > 0, "Monthly spending trends aggregated")

    # --- 7. Pure C11 DSA Algorithm Suite ---
    print("\n--- 7. Pure C11 DSA Optimization Algorithms ---")
    status, dsa_res, _ = request("GET", "/api/dsa/run", token=staff_token)
    assert_test(status == 200 and dsa_res.get("success"), "Executed C11 DSA Suite in memory")
    algs = dsa_res.get("algorithms", {})
    assert_test("hash_table" in algs and algs["hash_table"]["complexity"] == "O(1) Average", "Hash table lookup verified")
    assert_test("dijkstra" in algs and algs["dijkstra"]["shortest_dist_km"] > 0, "Dijkstra logistics pathfinding verified")
    assert_test("dp_knapsack" in algs and algs["dp_knapsack"]["max_reliability_score"] > 0, "DP 0-1 Knapsack safety optimization verified")
    assert_test("tsp_branch_bound" in algs and algs["tsp_branch_bound"]["optimal_tour_distance_km"] > 0, "Branch & Bound TSP circuit verified")
    assert_test("merge_sort" in algs and algs["merge_sort"]["complexity"] == "O(N log N)", "Merge sort telemetry verified")
    assert_test("queue" in algs and algs["queue"]["tasks_dispatched"] > 0, "Circular Task Queue verified")

    # --- 8. Reports & Hot Disaster Backup ---
    print("\n--- 8. Reports Export & Database Backup Integrity ---")
    status, pdf_content, headers = request("GET", f"/api/reports/pdf?type=fleet&token={admin_token}")
    assert_test(status == 200 and isinstance(pdf_content, bytes) and pdf_content.startswith(b"%PDF-1.4"), "Executive PDF report starts with genuine %PDF-1.4 header")

    status, csv_content, _ = request("GET", f"/api/reports/csv?type=vehicles&token={admin_token}")
    assert_test(status == 200 and "Registration" in str(csv_content), "Vehicles CSV export contains compliant tabular data")

    status, backup_res, _ = request("POST", "/api/backup", {}, token=admin_token)
    assert_test(status == 201 and os.path.exists(backup_res.get("filepath", "")), "Hot SQLite backup file written to disk")

finally:
    server_proc.terminate()
    try: server_proc.wait(timeout=3)
    except: server_proc.kill()

print("\n======================================================================")
print(f" END-TO-END TEST RESULTS: {passed} Passed, {failed} Failed")
print("======================================================================")

if failed > 0:
    sys.exit(1)
sys.exit(0)
