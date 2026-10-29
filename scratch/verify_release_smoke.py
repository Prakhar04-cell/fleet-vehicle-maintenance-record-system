import os
import sys
import time
import json
import shutil
import urllib.request
import urllib.error
import subprocess

PORT = 8098
BASE_URL = f"http://127.0.0.1:{PORT}"
TEST_DB = "data/test_smoke.db"

def prepare_db():
    if os.path.exists(TEST_DB):
        try: os.remove(TEST_DB)
        except: pass
    shutil.copyfile("data/fleet.db", TEST_DB)

def run_smoke():
    prepare_db()
    
    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + env.get("PATH", "")
    
    proc = subprocess.Popen(
        ["./fleet.exe", "--db", TEST_DB, "--web", str(PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env
    )
    
    time.sleep(1.0)
    
    results = []
    
    def log_result(name, ok, detail=""):
        results.append((name, ok, detail))
        mark = "[PASS]" if ok else "[FAIL]"
        print(f"  {mark} {name}: {detail}")

    try:
        # 1. Login
        login_data = json.dumps({"username": "admin", "password": "Admin@123"}).encode('utf-8')
        req = urllib.request.Request(f"{BASE_URL}/api/auth/login", data=login_data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            token = data.get("token")
            log_result("Login Endpoint (/api/auth/login)", resp.status == 200 and bool(token), f"token: {token[:8]}...")

        headers = {"Authorization": f"Bearer {token}"}

        # 2. RBAC session check
        req = urllib.request.Request(f"{BASE_URL}/api/auth/session", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            role = data.get("user", {}).get("role")
            log_result("RBAC Session Profile (/api/auth/session)", resp.status == 200 and role == "admin", f"role: {role}")

        # 3. Dashboard
        req = urllib.request.Request(f"{BASE_URL}/api/dashboard", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            stats = data.get("stats", {})
            log_result("Dashboard KPIs (/api/dashboard)", resp.status == 200 and "total_active_vehicles" in stats, f"active vehicles: {stats.get('total_active_vehicles')}")

        # 4. Vehicle lookup (DJB2 Hash Table)
        req = urllib.request.Request(f"{BASE_URL}/api/vehicles/lookup?reg=MH-04-AZ-4521", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            vehicle = data.get("vehicle", {})
            log_result("Vehicle Lookup via C Hash Table (/api/vehicles/lookup)", resp.status == 200 and data.get("success"), f"found: {vehicle.get('registration_number')} ({vehicle.get('make')} {vehicle.get('model')})")

        # 5. Maintenance Schedules
        req = urllib.request.Request(f"{BASE_URL}/api/schedules", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            count = data.get("count", len(data.get("data", [])))
            log_result("Maintenance Schedules (/api/schedules)", resp.status == 200 and data.get("success"), f"total schedules: {count}")

        # 6. Work Orders
        req = urllib.request.Request(f"{BASE_URL}/api/work-orders", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            count = data.get("count", len(data.get("data", [])))
            log_result("Work Orders (/api/work-orders)", resp.status == 200 and data.get("success"), f"total work orders: {count}")

        # 7. Live Monitoring
        req = urllib.request.Request(f"{BASE_URL}/api/live-status", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            log_result("Live Monitoring (/api/live-status)", resp.status == 200 and data.get("success"), f"server uptime: {data.get('uptime_seconds')}s, open work orders: {data.get('open_work_orders')}")

        # 8. Notifications
        req = urllib.request.Request(f"{BASE_URL}/api/notifications", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            count = data.get("count", 0)
            log_result("Notification Center (/api/notifications)", resp.status == 200 and data.get("success"), f"total notifications: {count}")

        # 9. Reports (CSV)
        req = urllib.request.Request(f"{BASE_URL}/api/reports/csv?type=vehicles", headers=headers)
        with urllib.request.urlopen(req) as resp:
            content = resp.read().decode('utf-8')
            log_result("CSV Report Export (/api/reports/csv)", resp.status == 200 and "Registration" in content, f"bytes: {len(content)}")

        # 10. Reports (Vector PDF)
        req = urllib.request.Request(f"{BASE_URL}/api/reports/pdf?type=fleet", headers=headers)
        with urllib.request.urlopen(req) as resp:
            content = resp.read()
            log_result("Vector PDF Export (/api/reports/pdf)", resp.status == 200 and content.startswith(b"%PDF-1.4"), f"PDF header %PDF-1.4 verified, bytes: {len(content)}")

        # 11. Operational DSA: Dijkstra Route Planning
        req = urllib.request.Request(f"{BASE_URL}/api/routes/plan?from=0&to=3", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            dist = data.get("shortest_distance_km") or data.get("shortest_dist_km")
            log_result("DSA Dijkstra Route Planning (/api/routes/plan)", resp.status == 200 and data.get("success"), f"distance: {dist} km, summary: {data.get('route_summary')}")

        # 12. Operational DSA: Priority Queue (Min-Heap)
        req = urllib.request.Request(f"{BASE_URL}/api/maintenance/priority-queue", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            log_result("DSA Min-Heap Priority Queue (/api/maintenance/priority-queue)", resp.status == 200 and data.get("success"), f"heap count: {data.get('count')}, complexity: {data.get('complexity')}")

        # 13. Operational DSA: Dispatch Queue (FIFO)
        req = urllib.request.Request(f"{BASE_URL}/api/work-orders/dispatch-queue", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            log_result("DSA FIFO Dispatch Queue (/api/work-orders/dispatch-queue)", resp.status == 200 and data.get("success"), f"queue count: {data.get('count')}, complexity: {data.get('complexity')}")

        # 14. Operational DSA: Rankings (Merge Sort)
        req = urllib.request.Request(f"{BASE_URL}/api/analytics/rankings?sort_by=odometer", headers=headers)
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            log_result("DSA Merge Sort Rankings (/api/analytics/rankings)", resp.status == 200 and data.get("success"), f"algorithm: {data.get('algorithm')}, items: {len(data.get('rankings', []))}")

        # 15. Operational DSA: 0-1 Knapsack
        knapsack_req = json.dumps({"budget": 5000000}).encode('utf-8')
        req = urllib.request.Request(f"{BASE_URL}/api/optimization/knapsack", data=knapsack_req, headers={**headers, "Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            log_result("DSA 0-1 Knapsack Budget Optimizer (/api/optimization/knapsack)", resp.status == 200 and data.get("success"), f"jobs selected: {data.get('jobs_selected')}, total cost: Rs. {data.get('total_cost')}")

        # 16. Operational DSA: Branch & Bound TSP
        tsp_req = json.dumps({"num_stops": 4}).encode('utf-8')
        req = urllib.request.Request(f"{BASE_URL}/api/optimization/tsp", data=tsp_req, headers={**headers, "Content-Type": "application/json"})
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            dist = data.get("optimal_circuit_distance_km") or data.get("optimal_tour_distance_km")
            log_result("DSA Branch & Bound TSP Tour (/api/optimization/tsp)", resp.status == 200 and data.get("success"), f"optimal circuit distance: {dist} km, circuit: {data.get('circuit_summary')}")

    finally:
        proc.terminate()
        try: proc.wait(timeout=3)
        except: proc.kill()
        if os.path.exists(TEST_DB):
            try: os.remove(TEST_DB)
            except: pass

    failed = [r for r in results if not r[1]]
    print(f"\nSmoke Test Summary: {len(results) - len(failed)} / {len(results)} Passed.")
    return len(failed) == 0

if __name__ == "__main__":
    success = run_smoke()
    sys.exit(0 if success else 1)
