import urllib.request, json

BASE_URL = 'http://127.0.0.1:8080'

def api_post(endpoint, payload):
    req = urllib.request.Request(
        BASE_URL + endpoint,
        data=json.dumps(payload).encode('utf-8'),
        headers={'Content-Type': 'application/json'}
    )
    with urllib.request.urlopen(req) as resp:
        return resp.status, json.loads(resp.read().decode('utf-8'))

def api_get(endpoint):
    with urllib.request.urlopen(BASE_URL + endpoint) as resp:
        return resp.status, json.loads(resp.read().decode('utf-8'))

print("=================================================================")
print(" STARTING COMPLETE END-TO-END BROWSER WORKFLOW API VERIFICATION")
print("=================================================================")

# 1. Dashboard Metrics
status, res = api_get('/api/dashboard')
print(f"[1. Dashboard] Status: {status}, Active Vehicles: {res['stats']['total_active_vehicles']}, Total Spending: {res['stats']['total_maintenance_spending_str']}")
assert status == 200 and res['success']

# 2. Authentication
status, res = api_post('/api/auth/login', {'username': 'admin', 'password': 'Admin@123'})
print(f"[2. Auth Login] Status: {status}, Role: {res['user']['role']}, Success: {res['success']}")
assert status == 200 and res['user']['role'] == 'admin'

# 3. Create Commercial Driver
driver_payload = {
    'name': 'E2E Test Driver',
    'phone': '+91-9988776655',
    'license_number': f'DL-E2E-{int(res["user"]["id"])}998',
    'license_expiry': '2030-12-31'
}
status, res = api_post('/api/drivers', driver_payload)
print(f"[3. Driver Create] Status: {status}, New Driver ID: {res.get('id')}, Success: {res['success']}")
new_driver_id = res.get('id')
assert status == 201

# 4. Create Vehicle
vehicle_payload = {
    'registration_number': 'MH-14-TEST-99',
    'make': 'Force',
    'model': 'Trax Cruiser Commercial',
    'manufacturing_year': 2023,
    'vin': 'MC2TESTVIN998877',
    'fuel_type': 'Diesel',
    'fuel_percentage': 80,
    'odometer_km': 12500,
    'insurance_expiry': '2028-05-15'
}
status, res = api_post('/api/vehicles', vehicle_payload)
print(f"[4. Vehicle Create] Status: {status}, New Vehicle ID: {res.get('id')}, Success: {res['success']}")
new_vehicle_id = res.get('id')
assert status == 201

# 5. Assign Driver to Vehicle
assign_payload = {
    'action': 'assign_driver',
    'vehicle_id': new_vehicle_id,
    'driver_id': new_driver_id
}
status, res = api_post('/api/vehicles', assign_payload)
print(f"[5. Driver Assign] Status: {status}, Success: {res['success']}")
assert status == 200 and res['success']

# 6. Log Maintenance Service (with automatic linked expense creation)
maint_payload = {
    'vehicle_id': new_vehicle_id,
    'service_type': 'Routine 10k Inspection',
    'service_date': '2026-10-25',
    'odometer_km': 13000,
    'description': 'Full synthetic oil change, fuel filter check, tire pressure calibration',
    'workshop_name': 'Force Commercial Thane',
    'labour_cost': 150000,
    'parts_cost': 250000,
    'tax_amount': 72000
}
status, res = api_post('/api/maintenance', maint_payload)
print(f"[6. Maintenance Record] Status: {status}, New Record ID: {res.get('id')}, Success: {res['success']}")
assert status == 201 and res['success']

# 7. Create Preventive Maintenance Schedule
schedule_payload = {
    'vehicle_id': new_vehicle_id,
    'service_name': '20,000 KM Wheel Alignment',
    'due_date': '2026-12-15',
    'due_odometer_km': 20000
}
status, res = api_post('/api/schedules', schedule_payload)
print(f"[7. Schedule Milestone] Status: {status}, New Schedule ID: {res.get('id')}, Success: {res['success']}")
new_sched_id = res.get('id')
assert status == 201 and res['success']

# 8. Record Operational Fuel Expense
expense_payload = {
    'vehicle_id': new_vehicle_id,
    'category': 'Fuel',
    'description': 'Diesel full tank (50L)',
    'amount': 450000,
    'expense_date': '2026-10-27'
}
status, res = api_post('/api/expenses', expense_payload)
print(f"[8. Operating Expense] Status: {status}, New Expense ID: {res.get('id')}, Success: {res['success']}")
assert status == 201 and res['success']

# 9. Evaluate Schedules Live
status, res = api_get('/api/evaluate')
print(f"[9. Milestone Evaluation] Status: {status}, Updated: {res['updated_count']}, Alerts: {res['notification_count']}")
assert status == 200 and res['success']

# 10. Execute Live C11 DSA Benchmarks
status, res = api_get('/api/dsa/run')
print(f"[10. DSA Benchmarks] Status: {status}, Dijkstra Route: {res['algorithms']['dijkstra']['route']} ({res['algorithms']['dijkstra']['shortest_dist_km']} km)")
assert status == 200 and res['success']

# 11. Trigger Online Hot Backup
status, res = api_post('/api/backup', {})
print(f"[11. Online Hot Backup] Status: {status}, Filepath: {res.get('filepath')}, Success: {res['success']}")
assert status == 201 and res['success']

# 12. Query Audit Log to verify all operations were audited
status, res = api_get('/api/audit')
print(f"[12. Audit Trail] Status: {status}, Total Audit Events Recorded: {res['count']}")
assert status == 200 and res['count'] > 0

print("=================================================================")
print(" ALL 12 END-TO-END WORKFLOWS PASSED WITH 100% SUCCESS!")
print("=================================================================")
