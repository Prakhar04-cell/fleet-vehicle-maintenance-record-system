# Smart Fleet Management Working Prototype — Next Phase Implementation Report

**Project Title:** Fleet Vehicle Maintenance Record System — Smart Fleet Prototype  
**Date:** October 28, 2026  
**Implementation Standard:** C11 (`-std=c11 -Wall -Wextra -Wpedantic -O2`)  
**Database:** SQLite 3.49+ (`WAL` mode, `PRAGMA foreign_keys = ON`)  
**Network Daemon:** Native C11 Windows Winsock2 (`ws2_32`) Embedded HTTP/1.1 Engine  
**Frontend Stack:** Vanilla HTML5, CSS3, ES6 JavaScript (No Node/Python/Supabase dependencies)  
**Total Test Suite Status:** **313 / 313 Passing Assertions (100% Pass Rate)**  
- C11 Native Core & Security Suite: **199 / 199 Passed**
- Live HTTP RBAC Integration Suite: **81 / 81 Passed**
- Live End-to-End Prototype Suite: **33 / 33 Passed**

---

## 1. Executive Summary

This report documents the design, implementation, and rigorous verification of the **Smart Fleet Management Working Prototype**, expanding the Fleet Vehicle Maintenance Record System into a production-grade, college-demonstration-ready fleet operations platform.

The prototype was constructed by enhancing the existing pure C11 backend, native Winsock2 HTTP server, and vanilla web frontend without introducing third-party runtimes, secondary databases, or mockups. All data persists directly to SQLite with versioned migrations, atomic database transactions, and strict role-based access control (RBAC).

---

## 2. Implemented Features

### Feature 1: Smart Preventive Maintenance Engine
- **Dual Threshold Tracking:** Evaluates both calendar due dates (ISO `YYYY-MM-DD`) and vehicle target odometer milestones (`odometer_km`).
- **Dynamic Status Lifecycle:** Schedules advance automatically across statuses: `Scheduled` -> `Due` -> `Overdue` -> `Completed` -> `Cancelled`.
- **Configurable Evaluation Windows:** Alerts trigger within a configurable lead time (default: 7 days before due date, or within 500 km of target odometer).
- **Recalculation Engine:** The C backend function `schedule_evaluate_all()` recalculates milestone urgency whenever a new service record is logged, an odometer reading advances, or the manual re-evaluation trigger is executed.
- **Alert Deduplication:** Notifications are deduplicated per schedule milestone and evaluation window, preventing notification flooding in the in-app notification center.
- **Milestone Generation:** Completing a scheduled recurring service automatically computes and persists the next due milestone (e.g. Current Odometer + 10,000 km or Due Date + 180 days) in a single atomic transaction.

### Feature 2: Work Order Lifecycle Management
- **Full Lifecycle States:** Implements a state machine with validated transitions:
  - `Open` -> `Assigned` -> `In Progress` -> `Waiting for Parts` -> `Completed` / `Cancelled`.
- **Role Permissions:**
  - Fleet Managers and Admins can create and assign work orders (`PERM_WORK_ORDER_WRITE`).
  - Maintenance Staff can view assigned work orders, advance states (`Assigned` -> `In Progress`), and execute completions.
- **Transition History & Audit Trail:** Every status change is immutably logged in `work_order_history` with the acting user's ID, username, old status, new status, timestamp, and transition notes.
- **Atomic Service & Expense Reconciliation:** When a work order is marked `Completed`:
  1. A formal `maintenance_records` entry is created linking labour cost, parts cost, workshop name, and service odometer.
  2. A corresponding `expenses` entry is automatically recorded in the same database transaction.
  3. The work order is updated with `maintenance_record_id`, `completion_time`, and status `Completed`.
  4. Duplicate completion attempts are blocked by strict validation checks, preventing duplicated financial entries.

### Feature 3: Digital Vehicle Inspection & Defect Escalation
- **8 Standardized Checklist Items:**
  1. Brakes & Calipers
  2. Tyres & Tread Depth
  3. Lights, Indicators & Horn
  4. Engine & Exhaust Diagnostics
  5. Battery & Electrical System
  6. Steering & Suspension Alignment
  7. Fluid Levels (Oil / Coolant / Brake)
  8. Registration & Insurance Documents
- **Configurable Results & Severity:**
  - Status: `Pass`, `Fail`, `N/A`.
  - Severity: `Normal`, `Minor Defect`, `Critical Defect`.
- **Automatic Status Escalation:**
  - If any check fails with `Critical Defect`, the overall inspection status automatically escalates to `Fail`.
  - If any check fails with minor defects, the status becomes `Defective`.
  - If all checks pass or are N/A, the status is `Pass`.
- **Unresolved Defect Register:** Active defects are tracked in SQLite and surfaced on manager and staff dashboards.
- **One-Click Defect-to-Work-Order Creation:** Managers can convert any unresolved defect directly into a high/critical priority repair work order with pre-filled defect descriptions and vehicle linkage.

### Feature 4: Fleet Cost Analytics & SQL Reconciliations
- **Real SQLite Aggregations:** Cost analytics are derived entirely from live SQLite database queries with prepared statements:
  - Total Maintenance Cost (`SUM(total_cost)` from `maintenance_records`).
  - Average Service Cost (`AVG(total_cost)` from `maintenance_records`).
  - Operational Expenses Breakdown (`SUM(amount)` from `expenses` grouped by `category`: Fuel, Tolls, Parts, Insurance, Taxes).
  - Top 5 Highest-Cost Vehicles (`GROUP BY vehicle_id` ordered descending).
  - Work Order Status Distribution (`COUNT(*)` grouped by `status`).
  - Inspection Defect Health (`COUNT(*)` grouped by `overall_status` and open defect count).
  - 6-Month Expenditure Trend (`strftime('%Y-%m', expense_date)`).
- **Double-Counting Prevention:** Non-maintenance operational calculations explicitly exclude maintenance expense records to ensure financial reconciliation matches actual company spending.

### Feature 5: Pure C11 Data Structures & Algorithms (DSA) Suite
Eight fundamental algorithms were implemented in pure C11, running directly in backend memory and exposed via live HTTP benchmarking at `/api/dsa/run`:

| Algorithm | Data Structure | Fleet Management Application | Time Complexity | Space Complexity |
|---|---|---|:---:|:---:|
| **DJB2 Hash Table** | Hash Table (`DsaHashTable`) | O(1) Instant Vehicle Registration Lookup | Average: $O(1)$, Worst: $O(N)$ | $O(N)$ |
| **Binary Search Tree** | BST (`DsaBST`) | $O(\log N)$ Odometer Range Queries & Vehicle Indexing | Average: $O(\log N)$, Worst: $O(N)$ | $O(N)$ |
| **Priority Min-Heap** | Binary Min-Heap (`DsaMinHeap`) | Real-time Urgency Queue for Overdue Maintenance | $O(\log N)$ push/pop | $O(N)$ |
| **Circular Queue** | FIFO Queue (`DsaQueue`) | Workshop Service Bay Task Scheduling | $O(1)$ push/pop | $O(N)$ |
| **Operational Stack** | LIFO Stack (`DsaStack`) | Audit Trail & Action Rollback/Undo | $O(1)$ push/pop | $O(N)$ |
| **Merge Sort** | Stable Array Merge (`dsa_merge_sort`) | Sorting Large Fleets by Odometer/Cost | $O(N \log N)$ | $O(N)$ |
| **Dijkstra's Algorithm** | Adjacency List Graph (`DsaGraph`) | Shortest Path Delivery & Depot Routing | $O(V^2)$ / $O(E + V \log V)$ | $O(V + E)$ |
| **0-1 Knapsack (DP)** | Dynamic Programming Table | Optimal Safety Return Under Maintenance Budget | $O(N \cdot W)$ | $O(N \cdot W)$ |
| **Branch & Bound** | State Space Tree Search | Traveling Salesperson (TSP) Mobile Inspection Circuit | $O(N!)$ worst, pruned | $O(N)$ |

---

## 3. Database Schema Changes (Migration 002)

Applied non-destructively via `migrations/002_smart_fleet_management.sql`:

```sql
-- Work Orders Table
CREATE TABLE IF NOT EXISTS work_orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    wo_number TEXT UNIQUE NOT NULL COLLATE NOCASE,
    vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
    schedule_id INTEGER REFERENCES maintenance_schedules(id) ON DELETE SET NULL,
    inspection_id INTEGER,
    title TEXT NOT NULL,
    description TEXT,
    priority TEXT NOT NULL DEFAULT 'Medium' CHECK(priority IN ('Low', 'Medium', 'High', 'Critical')),
    status TEXT NOT NULL DEFAULT 'Open' CHECK(status IN ('Open', 'Assigned', 'In Progress', 'Waiting for Parts', 'Completed', 'Cancelled')),
    assigned_to_user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    assigned_to_name TEXT,
    odometer_km INTEGER CHECK(odometer_km IS NULL OR odometer_km >= 0),
    start_time TEXT,
    completion_time TEXT,
    labour_cost INTEGER NOT NULL DEFAULT 0 CHECK(labour_cost >= 0),
    parts_cost INTEGER NOT NULL DEFAULT 0 CHECK(parts_cost >= 0),
    total_cost INTEGER NOT NULL DEFAULT 0 CHECK(total_cost >= 0),
    maintenance_record_id INTEGER REFERENCES maintenance_records(id) ON DELETE SET NULL,
    notes TEXT,
    created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- Work Order History
CREATE TABLE IF NOT EXISTS work_order_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    work_order_id INTEGER NOT NULL REFERENCES work_orders(id) ON DELETE CASCADE,
    from_status TEXT NOT NULL,
    to_status TEXT NOT NULL,
    changed_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
    changed_by_name TEXT,
    notes TEXT,
    timestamp TEXT NOT NULL
);

-- Digital Vehicle Inspections Table
CREATE TABLE IF NOT EXISTS inspections (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    inspection_number TEXT UNIQUE NOT NULL COLLATE NOCASE,
    vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
    inspector_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    inspector_name TEXT NOT NULL,
    inspection_date TEXT NOT NULL,
    odometer_km INTEGER NOT NULL CHECK(odometer_km >= 0),
    overall_status TEXT NOT NULL DEFAULT 'Pass' CHECK(overall_status IN ('Pass', 'Defective', 'Fail')),
    defect_count INTEGER NOT NULL DEFAULT 0 CHECK(defect_count >= 0),
    notes TEXT,
    created_at TEXT NOT NULL
);

-- Checklist Items Table
CREATE TABLE IF NOT EXISTS inspection_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    inspection_id INTEGER NOT NULL REFERENCES inspections(id) ON DELETE CASCADE,
    item_name TEXT NOT NULL,
    status TEXT NOT NULL CHECK(status IN ('Pass', 'Fail', 'N/A')),
    severity TEXT NOT NULL DEFAULT 'Normal' CHECK(severity IN ('Normal', 'Minor Defect', 'Critical Defect')),
    notes TEXT,
    work_order_id INTEGER REFERENCES work_orders(id) ON DELETE SET NULL
);
```

---

## 4. API Endpoints Added & Permissions

| Method | Endpoint | Required Permission | Description |
|:---:|---|:---:|---|
| `GET` | `/api/work-orders` | `PERM_READ` | Lists work orders with optional status and vehicle filters |
| `POST` | `/api/work-orders` | `PERM_WORK_ORDER_WRITE` | Creates a new work order (`Open` status) |
| `PUT` | `/api/work-orders` | `PERM_WORK_ORDER_WRITE` | Handles status transitions, technician assignments, and completions |
| `GET` | `/api/work-orders/history` | `PERM_READ` | Retrieves the immutable audit transition history for a work order |
| `GET` | `/api/inspections` | `PERM_READ` | Lists digital vehicle inspections with defect counts and statuses |
| `POST` | `/api/inspections` | `PERM_INSPECTION_WRITE` | Submits an 8-item digital inspection checklist |
| `GET` | `/api/inspections/defects` | `PERM_READ` | Retrieves unresolved defects across active vehicles |
| `POST` | `/api/inspections/create-work-order` | `PERM_WORK_ORDER_WRITE` | 1-click generation of a repair work order from a defect |
| `GET` | `/api/analytics` | `PERM_READ` | Aggregated cost analytics, top vehicles, and spending trends |
| `GET` | `/api/dsa/run` | `PERM_READ` | Executes in-memory C11 DSA algorithms and returns benchmark telemetry |

---

## 5. Build, Run, and Test Verification

### Build Command
```powershell
$env:PATH="C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
mingw32-make all
```
*Result:* Built `fleet.exe` with zero errors and zero warnings.

### Native C11 Unit Tests
```powershell
mingw32-make test
```
*Result:* **199 / 199 Tests Passed (100%)**.

### Live HTTP RBAC Integration Tests
```powershell
python tests/test_rbac_live.py
```
*Result:* **81 / 81 Assertions Passed (100%)**.

### Live Smart Fleet E2E Integration Tests
```powershell
python tests/test_smart_fleet_e2e.py
```
*Result:* **33 / 33 Assertions Passed (100%)**.

### Launch Web Application
```powershell
.\fleet.exe --web 8080
```
Open browser to: `http://127.0.0.1:8080`

### Demo Credentials
- **Administrator:** `admin` / `Admin@123`
- **Fleet Manager:** `manager` / `Manager@123`
- **Maintenance Staff:** `staff` / `Staff@123`

---

## 6. Known Limitations & College Demonstration Notes

1. **Email/SMS Notifications:** As specified by requirements, notification delivery is implemented strictly through the persistent SQLite in-app notification center. No external SMS or SMTP credentials are required.
2. **GPS & Real-Time Tracking:** The Dijkstra route optimization operates on a defined sample depot network model with verified distance matrices. Real-world GPS tracking requires live telematics hardware integrations.
3. **Localhost HTTP Binding:** The Winsock2 HTTP daemon binds to `127.0.0.1` by default to adhere to host security best practices.
