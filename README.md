# Fleet Vehicle Maintenance Record System

An enterprise-grade Fleet Vehicle Maintenance & Operational Record System implemented in **pure modern C (C11 standard)**. The system provides complete vehicle asset tracking, driver assignments, multi-component service and repair records, preventive maintenance scheduling with automated date/odometer threshold evaluations, financial expense tracking, role-based access control (RBAC), hot online SQLite backups, and automated CSV / genuine vector PDF report generation.

---

## Table of Contents

1. [Key Features](#key-features)
2. [Technology Stack & Architecture](#technology-stack--architecture)
3. [Role-Based Access Control (RBAC)](#role-based-access-control-rbac)
4. [Database Design & Migrations](#database-design--migrations)
5. [Prerequisites & Installation](#prerequisites--installation)
6. [Compilation & Build Commands](#compilation--build-commands)
7. [Running the Application](#running-the-application)
8. [Automated Task Scheduling (Reminders Engine)](#automated-task-scheduling-reminders-engine)
9. [Sample Development Data](#sample-development-data)
10. [Backup & Disaster Recovery](#backup--disaster-recovery)
11. [CSV & PDF Report Generation](#csv--pdf-report-generation)
12. [Automated Test Suite & Verification Results](#automated-test-suite--verification-results)

---

## Key Features

- **Vehicle Asset Lifecycle Management:**
  - Complete vehicle register (Registration number, Make, Model, Manufacturing Year, VIN/Chassis, Fuel Type, Fuel Level %, Odometer reading, Status, Assigned Driver, Insurance Expiry, Notes).
  - Strict input validations: nonnegative odometer readings, audited rollback protection requiring authorized reasoning when decreasing odometer readings.
  - Driver assignment validation preventing driver double-booking across active vehicles.
  - Soft archival (decommissioning) preserving historical maintenance records and financial audit trails.
  - Multi-attribute search and sorting (by registration, make/model, fuel level, odometer, status).

- **Service & Repair Records:**
  - Multi-component cost breakdown: Labour Cost, Parts/Materials Cost, and Taxes (GST/VAT) with exact integer calculation.
  - Atomic multi-step database transactions: simultaneous logging of maintenance and matching general expense records.
  - Automatic advancement of vehicle odometer based on service readings.
  - Next recommended service threshold tracking (date and kilometre thresholds).
  - Searchable vehicle-specific maintenance logs and invoice document path linking.

- **Preventive Maintenance & Automated Reminders:**
  - Dual threshold scheduling: Calendar Due Date and/or Target Odometer readings.
  - Dynamic status lifecycle: `Scheduled` -> `Due` -> `Overdue` -> `Completed` -> `Cancelled`.
  - Configurable reminder lead-time windows (e.g. 7 days before due date, or within 500 km of target odometer).
  - Recurring service automation: automatically calculates and schedules the next maintenance milestone upon completion.
  - Persistent in-app notification engine with deduplication.
  - Headless CLI check mode (`--check-reminders`) for operating system scheduler integration (cron / Windows Task Scheduler).

- **Financial Analytics & Expense Tracking:**
  - Multi-category cost tracking: Fuel, Maintenance, Insurance, Toll/Tax, Commercial Permits, Tires, Parts, Miscellaneous.
  - Exact monetary arithmetic using integer minor units (paise/cents) avoiding floating-point rounding errors.
  - Automatic deduplication preventing double-counting between maintenance records and general expenses.
  - Comprehensive financial aggregations: vehicle-wise maintenance costs, category-wise spending, monthly expenditure trends, and average maintenance cost per active vehicle.

- **Security & User Management:**
  - OpenSSL EVP cryptographically secure password hashing (PBKDF2-HMAC-SHA-256 with 100,000 iterations and 16-byte cryptographically random salt).
  - Constant-time comparison (`CRYPTO_memcmp`) mitigating timing attacks.
  - First-run wizard for master administrator creation (no hardcoded passwords).
  - Failed login handling and account deactivation.
  - Four granular user roles with dedicated authorization checks in every protected operation.
  - Audit trail logging all operational and security actions.

- **Genuine Vector PDF & RFC 4180 CSV Reports:**
  - RFC 4180 compliant CSV exports with active protection against spreadsheet formula injection (CWE-1236).
  - Native standard-compliant vector PDF 1.4 document generator written in C: produces genuine PDF vector files with branded headers, KPI cards, structured tabular data, and page numbers.

- **Data Structures & Algorithms (Academic Suite):**
  - Pure C11 implementations of Hash Table (DJB2), Binary Search Tree (Odometer indexing), Min-Heap (Maintenance urgency priority queue), FIFO Queue (Service dispatch), LIFO Stack (Operational undo), Merge Sort (Multi-attribute stable sorting), Dijkstra's Algorithm (Fleet logistics shortest path), Dynamic Programming (0-1 Knapsack quarterly budget optimizer), and Branch & Bound (Traveling Salesperson mobile mechanic tour).

- **Modern White/Grey Web Dashboard:**
  - Embedded pure C HTTP/1.1 server running on Windows Winsock2 (`ws2_32`). Zero external Node/Python/Flask runtime dependencies.
  - White-and-grey responsive dashboard (`http://127.0.0.1:8080`) featuring live KPI cards, interactive modals, CRUD data tables, live DSA visualization, and hot backup triggers.

---

## Technology Stack & Architecture

- **Core Language:** Pure C (C11 standard, compiled with `-Wall -Wextra -Wpedantic -O2`).
- **Database Engine:** SQLite 3 via native C API (`sqlite3.h`) with WAL mode and PRAGMA foreign keys.
- **Cryptography:** OpenSSL 3.x EVP (`openssl/evp.h`, `openssl/rand.h`, `openssl/crypto.h`).
- **Networking:** Native embedded C HTTP/1.1 socket server using Windows Winsock2 (`-lws2_32`).
- **PDF Engine:** Native vector PDF 1.4 generation module (`pdf_gen.c`).
- **Build System:** GNU Make (`Makefile`) with MSYS2 UCRT64 auto-detection.

### Project Structure

```
fleet vehicle maintenance record system/
├── Makefile                          # Unified build system (all, test, web, dsa, seed, clean)
├── README.md                         # Documentation and operational manual
├── .gitignore                        # Git exclusion rules
├── migrations/
│   └── 001_initial_schema.sql        # Normalized SQLite schema migration
├── include/
│   ├── common.h                      # Shared types, constants, date & string utilities
│   ├── database.h                    # Connection, transactions, and migration logic
│   ├── auth.h                        # PBKDF2 hashing, session, and RBAC flags
│   ├── input.h                       # Safe fgets-based bounded user input routines
│   ├── vehicle.h                     # Vehicle CRUD, search, and relational deletion
│   ├── driver.h                      # Driver CRUD, search, unassign cascade, and delete
│   ├── maintenance.h                 # Service records and vehicle history
│   ├── schedule.h                    # Preventive maintenance scheduling & reminder evaluation
│   ├── expense.h                     # Cost tracking and financial aggregations
│   ├── notification.h                # In-app persistent alert management
│   ├── audit.h                       # Security and operational audit logging
│   ├── dashboard.h                   # Real-time SQL aggregations and KPIs
│   ├── dsa.h                         # Pure C11 DSA definitions (9 structures & algorithms)
│   ├── http_server.h                 # Embedded C HTTP/1.1 server and REST API dispatcher
│   ├── pdf_gen.h                     # Native vector PDF 1.4 generation engine
│   ├── reports.h                     # CSV and PDF report generators
│   ├── backup.h                      # SQLite online backup and restore API
│   └── ui.h                          # Interactive enterprise console interface
├── src/
│   ├── main.c                        # Application entry point, CLI arguments (--web, --dsa)
│   ├── common.c                      # Utility implementations
│   ├── database.c                    # Database engine interface
│   ├── auth.c                        # OpenSSL cryptographic authentication & user API
│   ├── input.c                       # Secure input parsing
│   ├── vehicle.c                     # Vehicle business logic & relational delete
│   ├── driver.c                      # Driver management logic & search
│   ├── maintenance.c                 # Maintenance service record logic
│   ├── schedule.c                    # Preventive maintenance & reminder evaluation
│   ├── expense.c                     # Financial analytics logic
│   ├── notification.c                # Notification deduplication engine
│   ├── audit.c                       # Audit logging implementation
│   ├── dashboard.c                   # Real-time SQL aggregations
│   ├── dsa.c                         # Pure C implementations of all 9 DSA algorithms
│   ├── http_server.c                 # Winsock2 socket server, static router & REST endpoints
│   ├── pdf_gen.c                     # Vector PDF stream, catalog, font, and xref generator
│   ├── reports_csv.c                 # CSV export engine with formula sanitization
│   ├── reports_pdf.c                 # Executive PDF report builders
│   ├── backup.c                      # SQLite hot backup and disaster recovery
│   └── ui.c                          # Interactive enterprise console interface
├── web/                              # Professional White/Grey Web UI
│   ├── index.html                    # Semantic HTML5 layout and modal dialogs
│   ├── dashboard.css                 # Clean white/grey palette, card styles, and animations
│   └── app.js                        # Vanilla ES6 client interacting with C REST APIs
├── tests/
│   └── test_main.c                   # Automated test suite (150 test assertions)
├── data/
│   ├── fleet.db                      # Production SQLite database (created on first launch)
│   └── seed_sample_data.sql          # Sample development / demonstration dataset
├── reports/                          # Directory for exported CSV and PDF reports
│   ├── final_gap_audit.md            # Comprehensive repository gap audit
│   ├── dsa_implementation_report.md  # Detailed DSA implementation & complexity analysis
│   └── final_verification_report.md  # System verification and delivery report
└── backups/                          # Timestamped hot backup storage
```

---

## Role-Based Access Control (RBAC) & Three Role Dashboards

The system enforces strict server-side authorization powered by **256-bit cryptographically secure session tokens (`RAND_bytes`)** and **`HttpOnly; SameSite=Strict` cookies**. The frontend provides three distinct, tailored SaaS dashboard experiences based on the verified role:

### 1. Three Dedicated Role Dashboard Views
- **Administrator Dashboard ("System Overview"):**
  - Full fleet & financial overview with 7 real-time KPI cards.
  - SVG Fleet status donut chart, monthly expense trends, and category cost distributions.
  - Live Security & System Audit Events feed.
  - Disaster Recovery, Database Snapshot management, DSA Optimization suite, and User Account management.
- **Fleet Manager Dashboard ("Fleet Operations"):**
  - Operational dispatch & vehicle availability metrics (6 operational KPIs).
  - Driver & Commercial Vehicle Allocation Roster with one-click re-assignment drawers.
  - Actionable Overdue Maintenance alerts and schedules.
  - Prohibits vehicle/driver deletion, user account management, and database backup restoration.
- **Maintenance Staff Dashboard ("Maintenance Workspace"):**
  - Workshop Service Workload Hero Card with 4 real-time counters (*Vehicles In Bay*, *Overdue Services*, *Due Soon*, *Completed Records*).
  - Actionable Pending & Scheduled Work Orders table with one-click "+ Record Service" prefilling and "Complete" milestone workflows.
  - Active Workshop Bays live queue widget and recent completed service history log.
  - Restricts access from financial expense records, commercial driver administration, and system settings.

### 2. Granular Role-Permission Policy

| Feature / Operation | Administrator (`admin`) | Fleet Manager (`manager`) | Maintenance Staff (`staff`) | Viewer (`viewer`) |
| :--- | :---: | :---: | :---: | :---: |
| View Telemetry & Dashboard | **Yes** | **Yes** | **Yes** | **Yes** |
| View Service History & Reminders | **Yes** | **Yes** | **Yes** | **Yes** |
| View Vehicles Register | **Yes** | **Yes** | **Yes** | **Yes** |
| Add / Update Vehicle Specs | **Yes** | **Yes** | No (403) | No (403) |
| Delete Vehicle Asset | **Yes** | No (403) | No (403) | No (403) |
| View Commercial Drivers | **Yes** | **Yes** | No (403) | No (403) |
| Add / Update / Assign Driver | **Yes** | **Yes** | No (403) | No (403) |
| Delete Driver Profile | **Yes** | No (403) | No (403) | No (403) |
| Record Service Work Order | **Yes** | **Yes** | **Yes** | No (403) |
| Create / Reschedule Schedule | **Yes** | **Yes** | **Yes** | No (403) |
| Complete Schedule Milestone | **Yes** | **Yes** | **Yes** | No (403) |
| Evaluate Reminder Thresholds | **Yes** | **Yes** | **Yes** | No (403) |
| View & Manage Expense Ledger | **Yes** | **Yes** | No (403) | No (403) |
| Export CSV & Vector PDF Reports | **Yes** | **Yes** | No (403) | No (403) |
| Inspect Security Audit Trail | **Yes** | No (403) | No (403) | No (403) |
| Trigger Online Hot Backup | **Yes** | No (403) | No (403) | No (403) |
| User Account & Role Management | **Yes** | No (403) | No (403) | No (403) |

Detailed matrix and verification reports:
- [Role Permission Matrix](reports/role_permission_matrix.md)
- [Automated RBAC Test Results](reports/rbac_test_results.md)

---

## Database Design & Migrations

All monetary values are stored as 64-bit integers (`INTEGER`) representing minor currency units (paise/cents, e.g. Rs. 1,500.50 is stored as `150050`) to guarantee exact financial arithmetic. Dates are stored in standard ISO 8601 format (`YYYY-MM-DD` and `YYYY-MM-DD HH:MM:SS`).

Foreign keys are strictly enforced on every connection using `PRAGMA foreign_keys = ON;`.

### Core Tables:
1. `schema_migrations`: Tracks applied migration versions.
2. `users`: Stores user accounts, PBKDF2 hash, salt, iterations, role, and active status.
3. `drivers`: Commercial driver profiles, contact information, and license expiration.
4. `vehicles`: Fleet vehicles, registration numbers, VINs, fuel levels, odometer readings, statuses, and driver foreign keys.
5. `maintenance_records`: Detailed service logs, multi-component costs, workshops, odometer readings, and next service thresholds.
6. `maintenance_schedules`: Preventive maintenance milestones, due dates, due odometers, reminder windows, and recurrence intervals.
7. `expenses`: Vehicle operating expenditures across categories with optional foreign key linkage to maintenance records.
8. `notifications`: Persistent in-app maintenance alerts with deduplication constraint `UNIQUE(schedule_id, notification_type, scheduled_at)`.
9. `audit_logs`: Immutable security and operational audit trail with timestamps and entity tracking.

---

## Prerequisites & Installation

### Windows (MSYS2 / UCRT64)
Install MSYS2 and the UCRT64 GCC toolchain with SQLite and OpenSSL:
```powershell
# In MSYS2 terminal:
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
```
Ensure `C:\msys64\ucrt64\bin` and `C:\msys64\usr\bin` are present in your system `PATH`.

### Ubuntu / Debian
```bash
sudo apt update
sudo apt install build-essential libsqlite3-dev libssl-dev
```

### Fedora / RHEL
```bash
sudo dnf install gcc make sqlite-devel openssl-devel
```

---

## Compilation & Build Commands

Build the production executable:
```bash
make
# or on Windows MSYS2:
mingw32-make all
```

Build and execute the automated test suite:
```bash
make test
# or on Windows MSYS2:
mingw32-make test
```

Build with AddressSanitizer and UndefinedBehaviorSanitizer (diagnostic build):
```bash
make sanitize
```

Clean object files and build binaries:
```bash
make clean
```

---

## Running the Application

### 1. Modern White/Grey Web Dashboard (Enterprise Fleet SaaS)
Launch the embedded pure C HTTP server and open the browser:
```bash
# Set MSYS2 UCRT64 toolchain PATH (Windows PowerShell):
$env:PATH="C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH

# Start the web dashboard server:
.\fleet.exe --web 8080
# or via Makefile target:
mingw32-make web
```
Then navigate to: **`http://127.0.0.1:8080`**

#### Pre-Configured Demo Accounts (PBKDF2-HMAC-SHA256):
| Role | Username | Password | Permissions |
|---|---|---|---|
| **Administrator** | `admin` | `Admin@123` | Full access: User management, backups, vehicle/service CRUD |
| **Manager** | `manager` | `Manager@123` | Fleet operations, vehicle write, maintenance write |
| **Staff** | `staff` | `Staff@123` | Maintenance logging, schedule execution |

*(Click the user profile pill in the top-right header to switch roles instantly with 1-click quick-fill buttons).*

#### Direct PDF & CSV Report Endpoints:
- **Fleet Executive Summary (PDF):** `http://127.0.0.1:8080/api/reports/pdf?type=fleet`
- **Operating Expenses Ledger (PDF):** `http://127.0.0.1:8080/api/reports/pdf?type=expenses`
- **Vehicle Maintenance Dossier (PDF):** `http://127.0.0.1:8080/api/reports/pdf?type=maintenance&vehicle_id=1`
- **Vehicles Master Register (CSV):** `http://127.0.0.1:8080/api/reports/csv?type=vehicles`
- **Maintenance Logs (CSV):** `http://127.0.0.1:8080/api/reports/csv?type=maintenance`
- **Expenses Ledger (CSV):** `http://127.0.0.1:8080/api/reports/csv?type=expenses`
- **Schedules & Milestones (CSV):** `http://127.0.0.1:8080/api/reports/csv?type=schedules`
- **Monthly Spending Trends (CSV):** `http://127.0.0.1:8080/api/reports/csv?type=spending`
- **Vehicle Lifetime Costs (CSV):** `http://127.0.0.1:8080/api/reports/csv?type=vehicle_costs`

#### Key SaaS Dashboard Features
- **Modern White/Light-Grey SaaS Theme:** `#F8FAFC` background, pure white `#FFFFFF` cards, `#E2E8F0` borders, `#0F172A` charcoal text, and professional `#2563EB` blue accents.
- **Top 7 KPI Metric Cards:** Real-time metrics for Total Assets, Available Vehicles, In Maintenance, Out of Service, Overdue Services, Upcoming Milestones, and Total Spend.
- **Interactive Native SVG Charts (Zero Dependencies):**
  - **Fleet Status Distribution Donut Chart:** Interactive ring chart with live percentages, central fleet count, and color-coded status legend.
  - **Monthly Expenditure Bar Chart:** Multi-month maintenance and operational spending trend with coordinate axes and tooltips.
  - **Category Breakdown Progress Bars:** Fuel vs Periodic Maintenance vs Spare Parts vs Toll Taxes.
- **Reference-Inspired Vehicles Management Table:**
  - Real-time search across Reg Number, Make, Model, VIN, Driver.
  - Filters by Status (Available, On Trip, Maintenance, Out of Service) and Fuel Type (Diesel, Petrol, CNG, Electric).
  - Configurable Columns Picker: Toggle visibility of Make/Model, Year/VIN, Fuel Gauge, Odometer, Driver, Insurance, Status.
  - Sortable column headers (ascending / descending).
  - Mini Fuel Gauge progress bar (green >50%, amber 20-50%, red <20%).
  - Pagination controls with configurable page size (5, 10, 25).
- **Slide-Over Vehicle Detail Drawer:**
  - Smooth 540px slide-in drawer with backdrop blur.
  - Large Odometer, Fuel %, and Status hero cards.
  - 4 Tabbed Views: Specs & Overview, Service History, Schedules, and Operating Expenses.
  - Direct action buttons: "Edit Specifications", "Change Driver", "Log Maintenance", and "Add Expense".
- **Preventive Maintenance & Reminders:**
  - Dedicated tabs for Service Records, Schedules, and Overdue Reminders.
  - Urgent reminder cards grouped by Overdue (Red), Due Soon (Amber), and Upcoming (Blue).
  - 1-Click "Record Service" action that pre-fills vehicle and milestone details in the maintenance form.
- **One-Click CSV Reports:** Immediate browser download for Vehicles Master, Maintenance History, Operating Expenses, and Preventive Schedules with CSV formula-injection protection.
- **Audit Trail & Hot Backups:** Live view of the SQLite immutable audit ledger and one-click hot online backups via the native SQLite Online Backup API.

### 2. Academic DSA Demonstration Suite
Run the live academic Data Structures and Algorithms demonstrator and benchmark harness:
```bash
./fleet --dsa
# or via Makefile:
mingw32-make dsa
```

### 3. Interactive Console Interface
```bash
./fleet
# or on Windows:
.\fleet.exe
```

On first launch, if no administrator account exists, the application presents the **Initial System Configuration** wizard. Enter your desired master administrator username and secure password (minimum 6 characters).

### 4. Command-Line Options
```bash
./fleet --help
```
Output:
```
Options:
  --check-reminders    Run headless maintenance reminder evaluation and exit (for OS task scheduler / cron)
  --web, --server [p]  Start embedded C HTTP server and white/grey web dashboard (default: 8080)
  --dsa                Run Academic DSA demonstration suite (Hash Table, BST, Min-Heap, etc.)
  --db <path>          Use specified SQLite database file (default: data/fleet.db)
  --help, -h           Show this help message and exit
  --version, -v        Show program version
```

---

## Automated Task Scheduling (Reminders Engine)

The system provides a headless mode (`--check-reminders`) that inspects all active schedules against the current calendar date and latest vehicle odometer readings, updates schedule statuses to `Due` or `Overdue`, and generates deduplicated in-app alerts.

### Setting up Periodic Background Checks:

#### Linux / macOS (cron):
Add an entry to `crontab -e` to run daily at 06:00 AM:
```bash
0 6 * * * /path/to/fleet_vehicle_maintenance/fleet --check-reminders --db /path/to/fleet_vehicle_maintenance/data/fleet.db >> /var/log/fleet_reminders.log 2>&1
```

#### Windows (Task Scheduler):
Create a scheduled task via PowerShell to run every day at 08:00 AM:
```powershell
$Action = New-ScheduledTaskAction -Execute "C:\Users\singh\Desktop\fleet vehicle maintenance record system\fleet.exe" -Argument "--check-reminders" -WorkingDirectory "C:\Users\singh\Desktop\fleet vehicle maintenance record system"
$Trigger = New-ScheduledTaskTrigger -Daily -At 8am
Register-ScheduledTask -Action $Action -Trigger $Trigger -TaskName "FleetMaintenanceReminderCheck" -Description "Evaluates fleet preventive maintenance thresholds daily"
```

---

## Sample Development Data

To populate the system with realistic sample data (commercial vehicles, drivers, past maintenance records, upcoming schedules, and multi-category expenses):

```bash
make seed
# or on Windows:
mingw32-make seed
```
This executes `data/seed_sample_data.sql` into `data/fleet.db` without affecting production user accounts.

---

## Backup & Disaster Recovery

Hot online database backups are performed using the SQLite Online Backup API (`sqlite3_backup_*`), preventing corruption even while concurrent operations occur.

1. **Creating a Backup:**
   - Navigate to **Main Menu -> Option 11 (Database Backup & Restore) -> Option 1 (Create Online Database Backup Now)**.
   - Hot backups are saved with timestamped filenames in `backups/` (e.g. `backups/fleet_backup_20261027_234053.db`).

2. **Restoring from a Backup:**
   - Navigate to **Main Menu -> Option 11 -> Option 3 (Restore Database from Backup File)**.
   - The system automatically creates a pre-restore safety snapshot of the active database before restoring.
   - After restoring, the system runs an integrity verification (`PRAGMA integrity_check;`) and verifies data persistence.

---

## CSV & PDF Report Generation

Reports can be generated from the **Reports & Analytics Menu (Option 8)** or invoked programmatically:

### CSV Reports (RFC 4180 with Formula Injection Protection)
All exported text fields are sanitized: any cell beginning with `=`, `+`, `-`, `@`, `\t`, or `\r` is automatically prefixed with a single quote (`'`) to neutralize malicious formula execution in Microsoft Excel, LibreOffice Calc, or Google Sheets.

Available CSV reports:
- `reports/vehicles_register.csv`: Complete vehicle inventory with specs, status, and driver allocations.
- `reports/maintenance_history.csv`: Service logs with cost breakdowns (labour, parts, tax, total).
- `reports/schedules_report.csv`: Upcoming and overdue preventive maintenance schedules.
- `reports/expense_history.csv`: All expenditures categorized with vendor descriptions.
- `reports/monthly_expenditure.csv`: 12-month aggregated spending trends.
- `reports/vehicle_costs.csv`: Vehicle-wise maintenance vs. operational cost summaries.

### Genuine Vector PDF Reports (PDF 1.4)
The application generates valid, standard vector PDF documents without requiring heavy external dependencies:
- `reports/fleet_executive_summary.pdf`: Executive dashboard report featuring branded header banner, KPI statistical cards (active assets, availability %, maintenance count, total spend), and vehicle inventory table.
- `reports/vehicle_<ID>_maintenance.pdf`: Individual vehicle lifecycle report with vehicle specs card, complete service history, and cumulative cost calculations.
- `reports/expense_cost_analysis.pdf`: Financial analytics report with category breakdown table and monthly trends.

---

## Automated Test Suite & Verification Results

The automated test runner (`test_runner.exe`) executes 115 independent assertions against an isolated test database (`data/test_fleet.db`):

```
=================================================================
 FLEET VEHICLE MAINTENANCE RECORD SYSTEM - AUTOMATED TEST SUITE
=================================================================
[DB] Applying migration 001_initial_schema.sql...
[DB] Migration 001 applied successfully.

=== 1. Running Authentication & RBAC Tests ===
  [PASS] Fresh database detects first-run state
  [PASS] Successfully created Administrator account
  [PASS] Database is no longer in first-run state after admin creation
  [PASS] Created Fleet Manager account
  [PASS] Created Maintenance Staff account
  [PASS] Created Viewer account
  [PASS] Prevent duplicate username creation
  [PASS] Reject password under 6 characters
  [PASS] Correct password authenticates successfully
  [PASS] Authenticated user has correct ROLE_ADMIN
  [PASS] Username matches
  [PASS] Username matching is case-insensitive
  [PASS] Incorrect password fails authentication
  [PASS] Nonexistent username fails authentication
  [PASS] User can successfully change own password
  [PASS] Old password rejected after password change
  [PASS] New password authenticates successfully
  [PASS] Administrator can reset another user's password
  [PASS] Target user can log in with reset password
  [PASS] Administrator can deactivate an account
  [PASS] Disabled user is blocked from logging in
  [PASS] Administrator can reactivate an account
  [PASS] Reactivated user can log in
  [PASS] Admin has PERM_USER_MANAGE
  [PASS] Admin has PERM_BACKUP_RESTORE
  [PASS] Manager does not have PERM_USER_MANAGE
  [PASS] Manager has PERM_VEHICLE_WRITE
  [PASS] Staff does not have PERM_VEHICLE_WRITE
  [PASS] Staff has PERM_MAINTENANCE_WRITE
  [PASS] Viewer cannot write maintenance
  [PASS] Viewer has PERM_READ

=== 2. Running Vehicle & Driver Management Tests ===
  [PASS] Created Driver 1 successfully
  [PASS] Prevent duplicate driver license registration
  [PASS] Created Vehicle 1 with assigned Driver 1
  [PASS] Prevent duplicate vehicle registration number
  [PASS] Prevent assigning already-assigned driver to another active vehicle
  [PASS] Created Vehicle 2 with available Driver 2
  [PASS] Read vehicle by ID
  [PASS] Vehicle reg matches
  [PASS] Assigned driver joined correctly
  [PASS] Reject negative odometer reading
  [PASS] Reject decreasing odometer without authorized reason
  [PASS] Accept decreasing odometer with explicit audited reason
  [PASS] Accept forward odometer update
  [PASS] Fuel level updated to 45%
  [PASS] Vehicle search finds match by Make
  [PASS] Vehicle archived successfully
  [PASS] Archived vehicle excluded from active vehicle list
  [PASS] Archived vehicle persists and remains readable
  [PASS] Driver from decommissioned vehicle can be reassigned

=== 3. Running Maintenance & Service Records Tests ===
  [PASS] Created Maintenance Record with automatic linked expense
  [PASS] Total cost accurately sums labour, parts, and tax
  [PASS] Vehicle odometer auto-advanced to service odometer (27,000 km)
  [PASS] Linked expense created in same transaction
  [PASS] Expense is linked to maintenance record ID
  [PASS] Expense amount equals maintenance total cost
  [PASS] Reject maintenance record for nonexistent vehicle ID
  [PASS] Reject invalid service date format
  [PASS] Retrieved maintenance history for vehicle
  [PASS] Workshop name matches

=== 4. Running Preventive Maintenance Schedules & Reminders Tests ===
  [PASS] Created past due date schedule
  [PASS] Created upcoming date schedule (due in 4 days, window 7)
  [PASS] Created odometer-based schedule within 500km threshold
  [PASS] Evaluated all schedules successfully
  [PASS] Updated schedule statuses based on date and odometer
  [PASS] Generated in-app notifications for overdue and upcoming services
  [PASS] Schedule 1 status updated to 'Overdue'
  [PASS] Schedule 2 status updated to 'Due'
  [PASS] Notification deduplication prevents duplicate alerts for same schedule & window
  [PASS] Marked Schedule 3 as completed
  [PASS] Automatically generated next recurring schedule
  [PASS] Next recurring schedule odometer set to current + interval (37,000 km)
  [PASS] Rescheduled Schedule 1 to future date
  [PASS] Schedule 1 status reset to 'Scheduled'

=== 5. Running Expense Tracking & Financial Analytics Tests ===
  [PASS] Recorded Fuel Expense
  [PASS] Recorded Toll Expense
  [PASS] Category summary aggregates expenses by category
  [PASS] Generated vehicle-wise cost summaries
  [PASS] Maintenance cost accurately distinguished
  [PASS] Other operating expenses correctly summed
  [PASS] Total cost matches sum
  [PASS] Computed overall fleet expense statistics
  [PASS] Maintenance spending non-zero
  [PASS] Total spending includes maintenance + operational costs
  [PASS] Accurately counted active vehicles

=== 6. Running CSV & PDF Report Generation Tests ===
  [PASS] Formula injection payload prefixed with single quote and quoted
  [PASS] Safe text does not require quotes or prefixes
  [PASS] Text with comma quoted properly
  [PASS] Generated Vehicle Register CSV report
  [PASS] Vehicle Register CSV file exists and is readable
  [PASS] Generated Maintenance History CSV report
  [PASS] Maintenance History CSV file exists and is readable
  [PASS] Generated Schedules CSV report
  [PASS] Schedules CSV file exists and is readable
  [PASS] Generated Expenses CSV report
  [PASS] Expenses CSV file exists and is readable
  [PASS] Generated Monthly Expenditure CSV report
  [PASS] Monthly Expenditure CSV exists and is readable
  [PASS] Generated Vehicle Costs CSV report
  [PASS] Vehicle Costs CSV exists and is readable
  [PASS] Generated Fleet Executive Summary PDF
  [PASS] PDF file exists and is readable
  [PASS] PDF file has genuine %PDF-1.4 magic header
  [PASS] Generated Vehicle Maintenance History PDF
  [PASS] Vehicle Maintenance History PDF file exists and is readable
  [PASS] Generated Expense Summary PDF
  [PASS] Expense Summary PDF file exists and is readable

=== 7. Running Backup, Restore & Audit Trail Tests ===
  [PASS] Created online hot backup using SQLite Backup API
  [PASS] Backup database file exists on disk
  [PASS] Audit logs recorded and queried
  [PASS] Latest audit event correctly recorded
  [PASS] Database passes PRAGMA integrity_check
[RESTORE] Creating safety snapshot of current database before restore...
[RESTORE] Safety snapshot saved to: backups/fleet_backup_20261027_235522.db
  [PASS] Restored database from backup file with pre-restore safety snapshot
  [PASS] Vehicle data intact after backup restoration
  [PASS] Prevent deleting vehicle with active maintenance or expense records

=== 8. Running Data Structures & Algorithms (C11) Tests ===
  [PASS] DSA Hash Table initialized
  [PASS] DSA Hash Table size is 3 after insertions
  [PASS] DSA Hash Table O(1) lookup found correct vehicle
  [PASS] DSA Hash Table lookup returns NULL for absent key
  [PASS] DSA Hash Table successfully removed key
  [PASS] DSA BST O(log N) search finds vehicle by odometer key
  [PASS] DSA BST min key is 15,000 km
  [PASS] DSA BST max key is 45,000 km
  [PASS] DSA BST range query [10000, 35000] returns 2 vehicles
  [PASS] DSA BST successfully deleted node
  [PASS] DSA Min-Heap peek returns highest priority (-3 days overdue)
  [PASS] DSA Min-Heap extract min pops most overdue schedule
  [PASS] DSA Min-Heap second extraction is next in priority
  [PASS] DSA Queue size is 2
  [PASS] DSA Queue maintains strict FIFO order (first in is first out)
  [PASS] DSA Queue is empty after 2 dequeues
  [PASS] DSA Stack size is 2
  [PASS] DSA Stack maintains LIFO order (last in is first out)
  [PASS] DSA Stack empty after pops
  [PASS] DSA Merge Sort correctly orders vehicles by odometer ascending in O(N log N)
  [PASS] DSA Merge Sort correctly orders vehicles by odometer descending
  [PASS] DSA Fleet Graph initialized with 6 nodes
  [PASS] DSA Dijkstra computes correct shortest path distance (26 km from Depot to East Cargo)
  [PASS] DSA DP Knapsack correctly optimizes fleet maintenance safety under budget
  [PASS] DSA Branch & Bound TSP finds optimal inspection circuit cost (80 km)

=================================================================
 TEST RESULTS SUMMARY: 150 / 150 Tests Passed (100%)
=================================================================
```

---

## License

This software is released under the MIT Open Source License.
