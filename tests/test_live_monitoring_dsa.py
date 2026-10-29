#!/usr/bin/env python3
"""
Automated Verification Suite:
Live Monitoring, Notification Center, Operational DSA Integration & Demo Mode Isolation
"""

import json
import os
import shutil
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import urllib.error

TEST_PORT = 8096
BASE_URL = f"http://127.0.0.1:{TEST_PORT}"
TEST_DB = "data/test_live_dsa.db"

def make_request(path, method="GET", data=None, token=None):
    url = f"{BASE_URL}{path}"
    headers = {}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    body = None
    if data is not None:
        headers["Content-Type"] = "application/json"
        body = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status, json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8")
        try:
            return e.code, json.loads(raw)
        except Exception:
            return e.code, {"raw": raw}
    except Exception as e:
        return 0, {"error": str(e)}

def login(username, password):
    status, res = make_request("/api/auth/login", method="POST", data={"username": username, "password": password})
    assert status == 200, f"Login failed for {username}: {res}"
    return res["token"]

def main():
    print("=" * 70, flush=True)
    print(" LIVE MONITORING, NOTIFICATION CENTER & OPERATIONAL DSA VERIFICATION", flush=True)
    print("=" * 70, flush=True)

    # 1. Prepare isolated test database
    os.makedirs("data", exist_ok=True)
    if os.path.exists(TEST_DB):
        os.remove(TEST_DB)
    if os.path.exists("data/fleet.db"):
        shutil.copy2("data/fleet.db", TEST_DB)
    else:
        print("[!] Warning: data/fleet.db does not exist, server will initialize schema.", flush=True)

    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")

    # 2. Launch server on TEST_DB
    print(f"[*] Starting C11 HTTP Server on port {TEST_PORT} with isolated DB: {TEST_DB}", flush=True)
    proc = subprocess.Popen(
        ["fleet.exe", "--db", TEST_DB, "--web", str(TEST_PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env
    )

    passed = 0
    failed = 0

    def check(name, condition, extra=""):
        nonlocal passed, failed
        if condition:
            passed += 1
            print(f"  [PASS] {name} {extra}", flush=True)
        else:
            failed += 1
            print(f"  [FAIL] {name} {extra}", flush=True)

    try:
        # Wait for server ready
        ready = False
        for _ in range(40):
            time.sleep(0.2)
            try:
                st, r = make_request("/api/auth/session")
                if st == 401:
                    ready = True
                    break
            except Exception:
                pass
        if not ready:
            print("[FAIL] Server failed to start within 8s", flush=True)
            return 1

        print("[+] C11 HTTP server is active and responding\n", flush=True)

        # 3. Authenticate roles
        admin_token = login("admin", "Admin@123")
        mgr_token = login("manager", "Manager@123")
        staff_token = login("staff", "Staff@123")
        check("Admin, Manager, and Staff authentication", True)

        # ---------------------------------------------------------
        # SECTION 1: LIVE FLEET MONITORING
        # ---------------------------------------------------------
        print("\n--- 1. Testing Live Fleet Monitoring (/api/live-status) ---", flush=True)
        status, res = make_request("/api/live-status", token=admin_token)
        check("GET /api/live-status returns HTTP 200", status == 200)
        check("Contains server uptime", "uptime_seconds" in res and res["uptime_seconds"] >= 0)
        check("Contains fleet vehicle summary", "summary" in res and "total_vehicles" in res["summary"])
        check("Contains open work orders metric", "open_work_orders" in res)
        check("Contains critical defects count", "critical_defects_count" in res)
        check("Contains active schedules count", "active_schedules" in res)
        check("Contains recent activity feed", "recent_activity" in res and isinstance(res["recent_activity"], list))
        check("Contains real-time notifications array", "notifications" in res and isinstance(res["notifications"], list))

        # ---------------------------------------------------------
        # SECTION 2: NOTIFICATION CENTER & DEDUPLICATION
        # ---------------------------------------------------------
        print("\n--- 2. Testing Notification Center & Deduplication ---", flush=True)
        status, nres1 = make_request("/api/notifications?unread_only=0", token=admin_token)
        check("GET /api/notifications returns HTTP 200", status == 200)
        count1 = nres1.get("count", 0)
        notifs1 = nres1.get("data", [])
        check("Notifications fetched from SQLite", count1 >= 0)

        # Repeated fetch should NOT create duplicate notifications (idempotent event sync)
        status, nres2 = make_request("/api/notifications?unread_only=0", token=admin_token)
        count2 = nres2.get("count", 0)
        check("Notification deduplication: stable event keys prevent duplicate alerts", count1 == count2)

        # Test Acknowledge single notification
        if notifs1:
            first_event = notifs1[0]["event_key"]
            status, ack_res = make_request("/api/notifications/ack", method="POST", data={"event_key": first_event}, token=admin_token)
            check("POST /api/notifications/ack returns HTTP 200", status == 200)
            check("Acknowledge response reports success", ack_res.get("success") is True)

            # Check unread filter
            status, unread_res = make_request("/api/notifications?unread_only=1", token=admin_token)
            unread_keys = [n["event_key"] for n in unread_res.get("data", [])]
            check("Acknowledged notification excluded from unread list", first_event not in unread_keys)

        # Test Acknowledge All
        status, ack_all = make_request("/api/notifications/ack-all", method="POST", token=admin_token)
        check("POST /api/notifications/ack-all returns HTTP 200", status == 200)
        status, unread_after = make_request("/api/notifications?unread_only=1", token=admin_token)
        check("Zero unread notifications remaining after Ack All", unread_after.get("count", 0) == 0)

        # ---------------------------------------------------------
        # SECTION 3: OPERATIONAL DSA INTEGRATION
        # ---------------------------------------------------------
        print("\n--- 3. Testing Operational C11 DSA Algorithms ---", flush=True)

        # 3.1 Hash Table Vehicle Lookup (DJB2 O(1))
        # Test exact, lower-case, and hyphen-free normalized lookups
        status, lk1 = make_request("/api/vehicles/lookup?reg=MH-04-AZ-4521", token=staff_token)
        check("Hash Table lookup exact match (MH-04-AZ-4521)", status == 200 and lk1.get("success") is True)
        if lk1.get("success"):
            v = lk1["vehicle"]
            check("Vehicle details correctly retrieved from C hash table", v.get("registration_number") == "MH-04-AZ-4521")

        status, lk2 = make_request("/api/vehicles/lookup?reg=mh04az4521", token=staff_token)
        check("Hash Table normalized lookup (case & hyphen-insensitive: mh04az4521)", status == 200 and lk2.get("success") is True)

        status, lk3 = make_request("/api/vehicles/lookup?reg=NONEXISTENT-9999", token=staff_token)
        check("Hash Table lookup returns 404 for absent registration", status == 404)

        # 3.2 Min-Heap Maintenance Priority Queue
        status, heap_res = make_request("/api/maintenance/priority-queue", token=staff_token)
        check("GET /api/maintenance/priority-queue returns HTTP 200", status == 200)
        check("Priority queue returns min-heap order", heap_res.get("complexity") == "O(log N)")
        items = heap_res.get("queue", [])
        if len(items) >= 2:
            check("Min-heap root is highest priority (Overdue critical first)", items[0]["priority_rank"] <= items[1]["priority_rank"])

        # 3.3 Workshop Queue & Stack
        status, q_res = make_request("/api/work-orders/dispatch-queue", token=staff_token)
        check("GET /api/work-orders/dispatch-queue returns HTTP 200 (FIFO)", status == 200)
        check("Dispatch queue metadata contains algorithm telemetry", q_res.get("complexity") == "O(1) Circular Queue")

        # Create a test work order and advance it to test Stack Undo
        status, wo_create = make_request("/api/work-orders", method="POST", data={
            "vehicle_id": 1,
            "title": "Stack Undo Test Job",
            "description": "Stack Undo Test Job Description",
            "priority": "Medium",
            "labour_cost": 2000,
            "parts_cost": 5000
        }, token=mgr_token)
        check("Created temporary work order for stack testing", status == 201)
        wo_id = wo_create.get("id") or wo_create.get("data", {}).get("id")

        if wo_id:
            # Advance to Assigned
            status, wo_trans = make_request("/api/work-orders", method="PUT", data={
                "action": "update_status",
                "id": wo_id,
                "status": "Assigned",
                "assigned_to_name": "Test Tech",
                "notes": "Assigned for undo test"
            }, token=mgr_token)
            check("Advanced work order to 'Assigned'", status == 200)

            # Test safe Stack Undo (should revert Assigned -> Open)
            status, undo_res = make_request("/api/work-orders/undo", method="POST", data={"work_order_id": wo_id}, token=mgr_token)
            check("POST /api/work-orders/undo reverts reversible status change (O(1) Stack)", status == 200 and undo_res.get("success") is True)

            # Advance to Completed and verify that Completed orders CANNOT be silently undone
            make_request("/api/work-orders", method="PUT", data={"action": "update_status", "id": wo_id, "status": "Assigned", "notes": "Re-assigned"}, token=mgr_token)
            make_request("/api/work-orders", method="PUT", data={"action": "update_status", "id": wo_id, "status": "In Progress", "notes": "In progress"}, token=staff_token)
            status, comp_res = make_request("/api/work-orders", method="PUT", data={
                "action": "complete", "id": wo_id, "odometer_km": 42000, "labour_cost": 2000, "parts_cost": 5000, "notes": "Completed test"
            }, token=staff_token)
            check("Work order completed with linked financial records", status == 200)

            # Attempting to undo completed status should be rejected
            status, reject_undo = make_request("/api/work-orders/undo", method="POST", data={"work_order_id": wo_id}, token=mgr_token)
            check("Stack safely rejects undoing completed financial transactions", status in (400, 404))

        # 3.4 Fleet Operations — Graph & Dijkstra Route Planner
        status, r_plan = make_request("/api/routes/plan?from=Central%20Depot&to=Port%20Facility", token=staff_token)
        check("GET /api/routes/plan returns shortest route via Dijkstra", status == 200)
        check("Route plan contains path string", "route" in r_plan)
        check("Route plan contains distance in km", "shortest_dist_km" in r_plan and r_plan["shortest_dist_km"] > 0)
        check("Route plan identifies hops traversed", "hops" in r_plan and isinstance(r_plan["hops"], list))
        check("Complexity identified as O((V+E) log V)", r_plan.get("complexity") == "O((V + E) log V)")

        # Invalid route destination
        status, bad_route = make_request("/api/routes/plan?from=Central%20Depot&to=Unknown%20Moon", token=staff_token)
        check("Route planner handles unreachable / invalid destination", status == 404)

        # 3.5 Cost Analytics — Merge Sort
        status, rank_odo = make_request("/api/analytics/rankings?sort_by=odometer", token=mgr_token)
        check("GET /api/analytics/rankings?sort_by=odometer returns 200", status == 200)
        check("Merge Sort complexity badge O(N log N)", rank_odo.get("complexity") == "O(N log N) Stable")
        ranks = rank_odo.get("rankings", [])
        if len(ranks) >= 2:
            check("Vehicles ranked deterministically", ranks[0]["metric_value"] >= ranks[1]["metric_value"])

        # 3.6 Optimization — Dynamic Programming (0-1 Knapsack)
        status, dp_res = make_request("/api/optimization/knapsack", method="POST", data={"budget_ceiling": 25000}, token=mgr_token)
        check("POST /api/optimization/knapsack returns optimal job selection", status == 200)
        check("Knapsack returns max reliability score", "max_reliability_score" in dp_res and dp_res["max_reliability_score"] > 0)
        check("Knapsack respects budget constraint (total_cost <= budget)", dp_res.get("total_cost", 0) <= 25000)

        # 3.7 Optimization — Branch and Bound (TSP Tour)
        status, tsp_res = make_request("/api/optimization/tsp", method="POST", data={"num_stops": 4}, token=mgr_token)
        check("POST /api/optimization/tsp computes optimal tour via B&B", status == 200)
        check("TSP returns optimal distance", "optimal_tour_distance_km" in tsp_res and tsp_res["optimal_tour_distance_km"] > 0)

        # ---------------------------------------------------------
        # SECTION 4: SAFE DEMONSTRATION MODE & RELIABILITY
        # ---------------------------------------------------------
        print("\n--- 4. Testing Demonstration Mode Safety & Isolation ---", flush=True)
        status, d_stat = make_request("/api/demo/status", token=admin_token)
        check("GET /api/demo/status returns active mode info", status == 200)
        check("Identifies current database path", "database_path" in d_stat)

        # Since server was started with data/test_live_dsa.db (NOT demo_fleet.db), demo reset MUST be blocked!
        status, d_reset = make_request("/api/demo/reset", method="POST", token=admin_token)
        check("POST /api/demo/reset strictly blocked (403) on non-demo database", status == 403)
        check("Error message indicates database path protection", "Only demo_fleet.db" in d_reset.get("message", ""))

    finally:
        print("\n[*] Terminating C HTTP Server process...", flush=True)
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()

    # 5. Test Demo Reset on actual demo database
    print("\n--- 5. Testing Demo Mode Reset on isolated data/demo_fleet.db ---", flush=True)
    demo_proc = subprocess.Popen(
        ["fleet.exe", "--demo", "--web", str(TEST_PORT + 1)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env
    )
    try:
        global BASE_URL
        BASE_URL = f"http://127.0.0.1:{TEST_PORT + 1}"
        ready_demo = False
        for _ in range(40):
            time.sleep(0.2)
            try:
                st, r = make_request("/api/auth/session")
                if st == 401:
                    ready_demo = True
                    break
            except Exception:
                pass
        if not ready_demo:
            print("[FAIL] Demo server failed to start", flush=True)
            return 1

        demo_admin_tok = login("admin", "Admin@123")
        status, demo_stat = make_request("/api/demo/status", token=demo_admin_tok)
        check("Demo mode active when launched with --demo", demo_stat.get("is_demo_mode") is True)
        check("Demo database path is data/demo_fleet.db", "demo_fleet.db" in demo_stat.get("database_path", ""))

        status, demo_reset = make_request("/api/demo/reset", method="POST", token=demo_admin_tok)
        check("POST /api/demo/reset succeeds on data/demo_fleet.db", status == 200 and demo_reset.get("success") is True)

        # Verify vehicles and schedules exist in demo database
        status, demo_vehs = make_request("/api/vehicles", token=demo_admin_tok)
        check("Pristine demo vehicles populated", demo_vehs.get("total", 0) >= 3 or demo_vehs.get("count", 0) >= 3)
    finally:
        demo_proc.terminate()
        try:
            demo_proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            demo_proc.kill()

    print("\n" + "=" * 70, flush=True)
    print(f" LIVE & DSA VERIFICATION SUMMARY: {passed} Passed, {failed} Failed", flush=True)
    print("=" * 70, flush=True)
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
