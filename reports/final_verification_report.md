# Final Verification & Systems Delivery Report
## Fleet Vehicle Maintenance Record System

**Project Name:** Fleet Vehicle Maintenance Record System  
**Evaluation Standard:** Final Academic & Industrial Verification Standard  
**Date of Audit & Certification:** October 28, 2026  
**Primary Programming Language:** C11 (ISO/IEC 9899:2011)  
**Database Engine:** SQLite 3 (v3.54.0) with WAL Mode & PRAGMA Foreign Keys  
**Cryptography:** OpenSSL 3.6.5 (PBKDF2-HMAC-SHA-256)  
**Networking / Web Server:** Windows Winsock2 (`ws2_32`) / Native Embedded C HTTP/1.1  
**Test Pass Rate:** **150 / 150 Tests Passed (100%)**  

---

### 1. Final Repository Structure

```
fleet vehicle maintenance record system/
├── Makefile                               # Build automation (UCRT64 auto-detection, test, web, dsa)
├── README.md                              # Comprehensive user guide, architecture & API docs
├── backups/                               # Online hot database backups generated via SQLite Backup API
│   └── fleet_backup_*.db
├── data/
│   ├── fleet.db                           # Persistent production/development SQLite database
│   ├── seed_sample_data.sql               # Seed dataset for vehicles, drivers, schedules, expenses
│   └── test_fleet.db                      # Temporary disposable database created during automated tests
├── include/                               # Public C header specifications
│   ├── audit.h                            # Audit trail logger interface
│   ├── auth.h                             # PBKDF2 authentication, sessions, and RBAC matrix
│   ├── backup.h                           # Online SQLite backup, restore, and integrity checks
│   ├── common.h                           # String, date/time, and currency utility routines
│   ├── dashboard.h                        # Fleet KPI metrics and aggregation calculations
│   ├── database.h                         # SQLite connection management and migration runner
│   ├── driver.h                           # Driver CRUD, search, unassign, and delete API
│   ├── dsa.h                              # Full Academic DSA suite (9 core structures & algorithms)
│   ├── expense.h                          # Operating & maintenance financial accounting
│   ├── http_server.h                      # Embedded C HTTP/1.1 server and REST API dispatcher
│   ├── input.h                            # Secure console input with masking and validation
│   ├── maintenance.h                      # Service records, odometer tracking, and history
│   ├── notification.h                     # In-app alerts and duplicate-reminder prevention
│   ├── pdf_gen.h                          # Binary PDF 1.4 low-level vector graphics/text generator
│   ├── reports.h                          # Reporting pipeline headers (CSV & PDF)
│   ├── schedule.h                         # Preventive maintenance schedules and intervals
│   ├── ui.h                               # ANSI console interface and interactive menus
│   └── vehicle.h                          # Vehicle CRUD, search, filter, and relational deletion
├── migrations/                            # Versioned schema migrations
│   └── 001_initial_schema.sql             # Relational DDL tables, indexes, and triggers
├── reports/                               # Generated reports and project audit documentation
│   ├── dsa_implementation_report.md       # Comprehensive DSA implementation & complexity analysis
│   ├── final_gap_audit.md                 # Complete repository gap audit and remediation matrix
│   ├── final_verification_report.md       # This comprehensive delivery report
│   └── test_*.csv / test_*.pdf            # Verified automated export artifacts
├── src/                                   # C11 implementation sources
│   ├── audit.c
│   ├── auth.c
│   ├── backup.c
│   ├── common.c
│   ├── dashboard.c
│   ├── database.c
│   ├── driver.c
│   ├── dsa.c                              # Pure C implementations of all 9 DSA algorithms
│   ├── expense.c
│   ├── http_server.c                      # Winsock2 socket server, static asset & JSON router
│   ├── input.c
│   ├── main.c                             # CLI entry point, argument parsing, headless checks
│   ├── maintenance.c
│   ├── notification.c
│   ├── pdf_gen.c
│   ├── reports_csv.c
│   ├── reports_pdf.c
│   ├── schedule.c
│   ├── ui.c
│   └── vehicle.c
├── tests/
│   └── test_main.c                        # Automated verification suite (150 test assertions)
└── web/                                   # Modern White/Grey Browser Dashboard Frontend
    ├── app.js                             # Vanilla ES6 client: live REST API fetches & modals
    ├── dashboard.css                      # Modern professional white-and-grey design tokens
    └── index.html                         # Semantic HTML5 dashboard, KPI cards, and data tables
```

---

### 2. Files Changed and Rationale

| File Path | Nature of Modification | Engineering Rationale |
|---|---|---|
| [include/vehicle.h](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/include/vehicle.h) & [src/vehicle.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/vehicle.c) | Added `vehicle_delete` | Implemented vehicle deletion with strict relational checks preventing removal when dependent records exist. |
| [include/driver.h](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/include/driver.h) & [src/driver.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/driver.c) | Added `driver_search`, `driver_delete`, and unassign cascade | Supports substring search, deletion of unassigned drivers, and automatic unassignment of active vehicles upon driver deactivation. |
| [include/auth.h](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/include/auth.h) & [src/auth.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/auth.c) | Added `auth_get_users`, `auth_free_users` | Enabled programmatic user retrieval for web dashboard and administrative inspection without exposing password salts/hashes. |
| [src/schedule.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/schedule.c) | Fixed reminder deduplication key | Odometer schedules now use deterministic `ODOM:<target_km>` keys, eliminating volatile daily reminder duplication. |
| [include/dsa.h](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/include/dsa.h) & [src/dsa.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/dsa.c) | **New** DSA Core Modules | Pure C11 implementations of Hash Table, BST, Min-Heap, FIFO Queue, LIFO Stack, Merge Sort, Dijkstra Graph, DP Knapsack, and Branch & Bound TSP. |
| [include/http_server.h](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/include/http_server.h) & [src/http_server.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/http_server.c) | **New** Embedded HTTP Server | Lightweight embedded Winsock2 HTTP/1.1 server serving static files and live REST APIs (`/api/dashboard`, `/api/vehicles`, `/api/dsa/run`, etc.). |
| [web/index.html](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/web/index.html), [web/dashboard.css](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/web/dashboard.css), [web/app.js](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/web/app.js) | **New** White/Grey UI Dashboard | Responsive white/grey UI with live KPI cards, CRUD tables, live DSA visualization, and backup triggers. Zero external dependencies. |
| [src/main.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/main.c) & [src/ui.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/ui.c) | Updated CLI & Interactive Menus | Added CLI flags `--web [port]`, `--server [port]`, and `--dsa`. Added menu items for vehicle deletion, driver search, driver deletion, DSA demo, and Web UI launch. |
| [Makefile](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/Makefile) | Updated Build Automation | Auto-detects UCRT64 MSYS2 layout, links `-lws2_32`, adds `web` and `dsa` targets, compiles `dsa.o` and `http_server.o`. |
| [tests/test_main.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/tests/test_main.c) | Extended Test Suite | Added 35 new test assertions covering driver search/delete, vehicle deletion constraints, user retrieval, and all 9 DSA algorithms (expanded to 150 total tests). |

---

### 3. Build & Test Verification Commands Executed

#### Compilation:
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
mingw32-make all
```
*Result:* Executable `fleet.exe` created successfully with zero compiler or linker errors.

#### Automated Test Suite Execution:
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
mingw32-make test
```
*Result:* Built `test_runner.exe` against temporary database `data/test_fleet.db`.  
**Passed: 150 / 150 Tests (100% Pass Rate). Exit Code: 0.**

#### Academic DSA Demonstration:
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
.\fleet.exe --dsa
```
*Result:* All 9 DSA modules executed with live calculation outputs (Hash table, BST range [20,000–50,000 km], Min-heap urgencies, Dijkstra 34 km shortest path, DP Knapsack 175 points for Rs. 13,000 spend, TSP 80 km tour). Exit Code: 0.

#### Headless Reminder Evaluation:
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
.\fleet.exe --check-reminders
```
*Result:* Headless scheduler ran and cleanly updated zero pending reminders without database locking or corruption. Exit Code: 0.

#### Database Integrity & Foreign Key Verification:
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
sqlite3 data/fleet.db "PRAGMA integrity_check; PRAGMA foreign_key_check;"
```
*Result:* Output: `ok` (zero corruption, zero foreign-key violations). Exit Code: 0.

---

### 4. Summary of Verified Features

1. **Authentication & Role-Based Access Control (RBAC):**
   - First-run detection with zero hardcoded default credentials.
   - PBKDF2-HMAC-SHA-256 password hashing with 100,000 iterations and 16-byte cryptographically secure random salts.
   - Constant-time password comparison preventing timing attacks.
   - 4 Role Tiers enforced: Administrator, Fleet Manager, Maintenance Staff, Viewer.
   - Administrator password resets and account deactivation/reactivation.
   - Programmatic user retrieval via `auth_get_users` (omitting secret hashes).

2. **Vehicle Management:**
   - Full CRUD lifecycle with registration uniqueness and VIN checks.
   - Monotonic odometer progression with explicit audited override for speedometer replacements.
   - Fuel percentage tracking (0–100%).
   - Soft-delete archival (`vehicle_archive`) preserving historical integrity.
   - Relational deletion (`vehicle_delete`) allowing clean deletion of unreferenced vehicles while blocking deletion when maintenance, expenses, or schedules exist.

3. **Driver Management:**
   - Driver registration with license uniqueness validation.
   - Substring search across driver name, license number, and phone number.
   - Double-booking prevention across active fleet vehicles.
   - Automatic vehicle unassignment cascade when a driver is deactivated.
   - Safe deletion preventing removal of drivers actively assigned to vehicles.

4. **Maintenance & Service History:**
   - Atomic recording of service date, odometer reading, workshop, mechanic, parts cost, labour cost, and tax.
   - Automatic vehicle odometer advancement to the service odometer.
   - Atomic creation of linked financial expense records within the same transaction.
   - Chronological service history retrieval per vehicle.

5. **Preventive Maintenance Schedules & Reminders:**
   - Dual-interval scheduling by calendar date, odometer threshold, or whichever occurs first.
   - Status transitions: `Scheduled` $\to$ `Due` $\to$ `Overdue` $\to$ `Completed`.
   - In-app notification generation with deduplication keys (`ODOM:<km>` / `SCHED:<id>:<window>`).
   - Automatic calculation of next recurring service interval upon completion.

6. **Financial Accounting & Dashboard KPIs:**
   - Exact integer minor-unit arithmetic (paise/cents) preventing floating-point rounding errors.
   - Dynamic monthly cost rollups and vehicle-by-vehicle cost summaries.
   - Live KPI cards: Total Active Vehicles, Vehicles in Maintenance, Overdue Services, Total Monthly Fleet Spend.

7. **Reports & Exports:**
   - CSV export for Vehicles, Maintenance History, Schedules, Expenses, Monthly Spend, and Vehicle Costs.
   - CSV formula injection protection: neutralizes `=,+,-,@` payloads by prefixing `'` and enclosing in quotes.
   - Pure C binary PDF 1.4 generator: creates genuine vector PDFs with headers, trailers, xref tables, and typography (Fleet Executive Summary, Vehicle Maintenance History, Expense Summary).

8. **Audit Trail & Hot Backups:**
   - Tamper-evident audit logging for all user actions, logins, updates, and exports.
   - Online hot backup creation using the non-blocking `sqlite3_backup_*` API.
   - Pre-restore safety snapshot creation to guarantee zero data loss during recovery.
   - Verified `PRAGMA integrity_check` after every restore operation.

9. **Data Structures & Algorithms (Academic Suite):**
   - 9 complete pure C11 algorithms: Hash Table, BST, Min-Heap, FIFO Queue, LIFO Stack, Merge Sort, Dijkstra's Algorithm, 0-1 Knapsack Dynamic Programming, Branch & Bound TSP.
   - Live demonstration runner via CLI (`--dsa`) and web endpoint (`/api/dsa/run`).

10. **Modern White/Grey Web Dashboard:**
    - Embedded pure C HTTP/1.1 server running on Windows Winsock2 (`-lws2_32`).
    - Responsive, accessible white-and-grey design palette with live KPI cards, interactive CRUD modals, live DSA optimization visualizations, and backup triggers.

---

### 5. Exact Instructions to Run and Operate

#### A. Launch the Professional Web Dashboard (Recommended)
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
.\fleet.exe --web 8080
```
Open your browser and navigate to:  
**`http://127.0.0.1:8080`**

#### B. Launch the Interactive Console Application
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
.\fleet.exe
```

#### C. Run the DSA Demonstration Suite
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
.\fleet.exe --dsa
```

#### D. Run the Automated 150-Test Suite
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
mingw32-make test
```

#### E. Run Headless Reminder Check (e.g. for Windows Task Scheduler)
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
.\fleet.exe --check-reminders
```

#### F. Seed a Disposable Database (Do not run on real fleet.db)
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
mingw32-make seed
```

---

### 6. Remaining Limitations & Security Notes

1. **Localhost Binding:** The embedded C HTTP server binds to `127.0.0.1` by default to ensure local development security and prevent unauthorized external network exposure.
2. **Relational Deletion Constraint:** Vehicles with existing financial or maintenance records cannot be hard deleted. This is an intentional compliance design; such vehicles must be archived (`is_archived = true`) to preserve historical accounting records.
3. **HTTP Development Server:** The embedded server handles sequential HTTP/1.1 requests suitable for single-user demonstrations, college evaluations, and local fleet management. For high-concurrency production deployments across thousands of concurrent web clients, place behind an NGINX reverse proxy with TLS termination.
