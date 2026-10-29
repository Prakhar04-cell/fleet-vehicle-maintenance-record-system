# Fleet Vehicle Maintenance Record System - Developer & Operations Guide

## 1. Prerequisites & Toolchain

### 1.1 Toolchain Requirements
- **Operating System**: Windows 10/11 (x64)
- **Compiler**: GCC with C11 standard support (MinGW-w64 / MSYS2 UCRT64 recommended)
- **Build Tool**: GNU Make (`mingw32-make` or `make`)
- **Libraries**:
  - `libsqlite3` (SQLite 3.30+)
  - `libssl` & `libcrypto` (OpenSSL 1.1.1 or 3.x)
  - `ws2_32` (Windows Sockets 2 - included with Windows SDK / MinGW)
- **Scripting & Test Runners**: Python 3.8+ (standard library only; no external pip dependencies required)

### 1.2 MSYS2 / UCRT64 Setup
If configuring from scratch on Windows using MSYS2:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-make \
          mingw-w64-ucrt-x86_64-sqlite3 \
          mingw-w64-ucrt-x86_64-openssl
```

Ensure the toolchain binaries are available on your system `PATH`:
```cmd
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%
```

---

## 2. Build Instructions

### 2.1 Available Makefile Targets
| Target | Description |
| :--- | :--- |
| `all` | Compiles all source files and produces `fleet.exe` |
| `check` | Runs syntax check and compiler diagnostic verification without linking |
| `test` | Builds `test_runner.exe` and runs the complete 199-test C unit test suite |
| `format` | Formats all C source and header files using `clang-format` (fails gracefully if uninstalled) |
| `format-check`| Verifies formatting conformance without altering source files |
| `clean` | Safely removes `*.o` files, `test_runner.exe`, and temporary `data/test_*.db` databases (**never touches `data/fleet.db`**) |
| `help` | Prints a summary of all targets |

### 2.2 Compilation
```cmd
mingw32-make all
```
The executable `fleet.exe` is placed directly in the repository root.

---

## 3. Running the Application

### 3.1 Production Web Server Mode
```cmd
fleet.exe --web 8080
```
- Binds to `127.0.0.1:8080`
- Uses production database: `data/fleet.db`
- Accessible via browser at `http://localhost:8080`

### 3.2 Safe Demonstration Mode (College / Evaluation)
```cmd
fleet.exe --demo --web 8080
```
- Binds to `127.0.0.1:8080`
- Uses isolated database: `data/demo_fleet.db`
- Bootstraps sample records automatically if `demo_fleet.db` is empty
- Allows safe, sandboxed database resets via `/api/demo/reset` without risking `data/fleet.db`

### 3.3 Interactive Console Mode
```cmd
fleet.exe
```
Provides an interactive text-based terminal interface for administrative operations.

### 3.4 Operational DSA Demo
```cmd
fleet.exe --dsa
```
Executes memory benchmarks and verification for all 8 C11 data structures and algorithms.

### 3.5 Headless Reminders Engine (Task Scheduler / Cron)
```cmd
fleet.exe --check-reminders
```
Evaluates all preventive maintenance milestones against calendar dates and vehicle odometers without launching a UI or server. Useful for automated OS background scheduling:
- **Windows Task Scheduler**: Register a daily task invoking `fleet.exe --check-reminders` at 08:00 AM.
- **Linux / cron**: `0 6 * * * /path/to/fleet --check-reminders --db /path/to/fleet.db >> /var/log/fleet_reminders.log 2>&1`

---

## 4. Test Suites & Verification

The system includes a 4-tier automated test battery with 366 total automated assertions:

### 4.1 C Unit Test Suite (199 Tests)
```cmd
mingw32-make test
```
Tests core modules in isolation using temporary databases:
- Database CRUD & foreign-key enforcement
- Authentication, PBKDF2 hashing & session tokens
- Preventive maintenance schedule recalculation & odometer thresholds
- Work order state machine & atomic completion
- Digital vehicle inspection & defect escalation
- CSV/PDF report generators with CSV formula injection protection
- In-memory C11 DSA data structures (Hash Table, BST, Min-Heap, Queue, Stack, Merge Sort, Dijkstra, Knapsack, TSP)

### 4.2 Live RBAC Integration Suite (81 Tests)
```cmd
python tests/test_rbac_live.py
```
Spawns an isolated C HTTP server on port 8089 with `data/test_rbac.db`:
- Verifies 401 Unauthorized on unauthenticated requests
- Verifies Staff role permissions (maintenance/inspections allowed, expenses/reports/deletions blocked)
- Verifies Manager role permissions (operations allowed, user admin/backups blocked)
- Verifies Admin role permissions (full management)
- Verifies server-side logout and token invalidation

### 4.3 Smart Fleet End-to-End Suite (33 Tests)
```cmd
python tests/test_smart_fleet_e2e.py
```
Spawns an isolated C HTTP server on port 8092 with `data/test_e2e_prototype.db`:
- Tests full lifecycle: vehicle registration -> inspection failure -> defect escalation -> repair work order -> atomic completion -> expense ledger reconciliation.

### 4.4 Live Monitoring & DSA Integration Suite (53 Tests)
```cmd
python -u tests/test_live_monitoring_dsa.py
```
Spawns an isolated C HTTP server on port 8096 with `data/test_live_dsa.db`:
- Live monitoring endpoint (`/api/live-status`)
- Notification deduplication with stable event keys
- C11 DSA REST endpoints (Hash Table vehicle lookup, Min-Heap priority queue, FIFO dispatch queue, LIFO undo stack, Dijkstra route planning, Merge Sort ranking, Knapsack budget optimizer, TSP circuit tour)
- Demonstration mode isolation and protection of `data/fleet.db`

---

## 5. Database Management, Migrations & Backups

### 5.1 Migrations
Schema changes are managed through sequential SQL migration scripts in `migrations/`:
- `001_initial_schema.sql`: Core schema (users, vehicles, drivers, maintenance, schedules, expenses, audit, sessions).
- `002_smart_fleet_management.sql`: Work orders, status transitions, digital inspections, and defect items.
- `003_live_monitoring_and_dsa.sql`: Persistent system notifications with stable deduplication keys.

Migrations are applied automatically upon startup if not already recorded in the `schema_migrations` table.

### 5.2 Hot Online Backup
The application utilizes the SQLite Online Backup API (`sqlite3_backup_*`), allowing continuous operations during backup creation without table locks:
- Backup directory: `backups/`
- Backup filename format: `fleet_backup_YYYYMMDD_HHMMSS.db`
- Triggered via API: `POST /api/backup` (Admin role required)

### 5.3 Safe Restore Procedure
Before restoring any database, the system automatically writes a pre-restore safety snapshot to `backups/fleet_safety_snapshot_*.db`. If the restore is interrupted or fails `PRAGMA integrity_check`, the pre-restore snapshot can be restored immediately.

---

## 6. Troubleshooting

### 6.1 "Missing DLL" Error on Windows
If `fleet.exe` fails to start with errors about `sqlite3.dll`, `libssl-3-x64.dll`, or `libcrypto-3-x64.dll`:
Ensure your MSYS2 / UCRT64 library path is present in `PATH`:
```cmd
set PATH=C:\msys64\ucrt64\bin;%PATH%
```

### 6.2 Port Already in Use (WSAEADDRINUSE 10048)
If the server fails to bind to port 8080:
1. Locate the occupying process:
   ```cmd
   netstat -ano | findstr :8080
   ```
2. Or run the server on a different port:
   ```cmd
   fleet.exe --web 8088
   ```

### 6.3 Database Locked (SQLITE_BUSY)
The application configures `sqlite3_busy_timeout` to 5,000 ms. If locks persist, verify that no external SQLite browser has opened `data/fleet.db` in exclusive locking mode.
