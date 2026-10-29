# Fleet Vehicle Maintenance Record System - Architecture Specification

## 1. System Overview & Technology Stack

The Fleet Vehicle Maintenance Record System is a high-performance, native enterprise fleet management platform built with:

- **Core Backend**: Pure C11 (`-std=c11 -Wall -Wextra -Wpedantic -O2`)
- **Networking & Transport**: Native Windows Winsock2 HTTP/1.1 micro-server (`ws2_32`)
- **Database Engine**: Embedded SQLite 3.x with foreign key enforcement and transactional integrity
- **Cryptography & Security**: OpenSSL `libcrypto` / `libssl` (PBKDF2-HMAC-SHA256, CSPRNG session tokens)
- **Frontend Layer**: Responsive Vanilla JavaScript (ES6+), semantic HTML5, and vanilla CSS3 (no external frameworks or CDNs)
- **Build System**: GNU Makefile with MinGW-w64 / MSYS2 UCRT64 toolchain

---

## 2. Layered Architecture

```
+-----------------------------------------------------------------------+
|                         Web Browser Client                            |
|             (Vanilla JS, Semantic HTML5, CSS Glassmorphism)           |
+-----------------------------------------------------------------------+
                                  |
                           HTTP/1.1 (JSON/REST)
                                  v
+-----------------------------------------------------------------------+
|                    Native HTTP Server & Router                        |
|   (src/http_server.c: Winsock2 socket loop, request parser, CORS,     |
|    Content-Length limit guards, static file & REST API dispatch)       |
+-----------------------------------------------------------------------+
                                  |
               +------------------+------------------+
               v                                     v
+-------------------------------+   +-----------------------------------+
|      Security & RBAC          |   |   Operational DSA Engine (C11)   |
| (src/auth.c: PBKDF2-SHA256,   |   | (src/dsa.c: Hash Table, Min-Heap, |
|  CSPRNG 64-char hex tokens,   |   |  Circular Queue, LIFO Undo Stack, |
|  server-side session storage, |   |  Dijkstra Pathfinding, Merge Sort,|
|  bitmask permission checks)   |   |  0-1 Knapsack, B&B TSP Tour)      |
+-------------------------------+   +-----------------------------------+
               |                                     |
               +------------------+------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                         Business Logic Layer                          |
|  - Vehicle & Driver Assets (src/vehicle.c, src/driver.c)             |
|  - Preventive Maintenance & Reminders (src/maintenance.c, schedule.c)|
|  - Smart Work Orders & Atomic Lifecycle (src/work_order.c)            |
|  - Digital Vehicle Inspection & Defect Escalation (src/inspection.c)  |
|  - Financial Ledgers & Expense Analytics (src/expense.c, dashboard.c) |
|  - Persistent Notification Engine & Deduplication (src/notification.c)|
|  - Export Engines (src/reports_csv.c, src/reports_pdf.c, pdf_gen.c)   |
|  - Security Audit Logging & Hot Backups (src/audit.c, src/backup.c)   |
+-----------------------------------------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                    SQLite Persistence & Migrations                    |
|   (src/database.c: Prepared statements, transactions, PRAGMA checks,  |
|    migration runner [migrations/*.sql], backup API, strict isolation) |
+-----------------------------------------------------------------------+
                                  |
               +------------------+------------------+
               v                                     v
+-------------------------------+   +-----------------------------------+
|   Production DB: data/fleet.db|   | Isolated Demo DB: demo_fleet.db   |
|   (Protected against reset)   |   | (Disposable college demo mode)    |
+-------------------------------+   +-----------------------------------+
```

---

## 3. Component Breakdown

### 3.1 Network & HTTP Transport (`src/http_server.c`)
- **Socket Lifecycle**: Uses Windows Winsock2 (`WSAStartup`, `socket`, `bind`, `listen`, `accept`, `send`, `recv`, `closesocket`, `WSACleanup`).
- **Buffer Safety & DoS Defense**: Enforces a strict 16 KB request buffer limit. Request bodies exceeding 15 KB are rejected with `413 Payload Too Large`. Negative `Content-Length` headers are rejected.
- **Path Traversal Protection**: Static routes (`/`, `/index.html`, `/dashboard.css`, `/app.js`, `/favicon.ico`) map to fixed, hardcoded filesystem locations without user-controlled paths.
- **RESTful Routing**: Dispatches authenticated API endpoints under `/api/*`.

### 3.2 Security, Cryptography & RBAC (`src/auth.c`, `include/auth.h`)
- **Password Storage**: Uses OpenSSL `PKCS5_PBKDF2_HMAC` with SHA-256, 16-byte random salt, and 100,000 iterations.
- **Session Tokens**: Generates 64-character hexadecimal tokens via `RAND_bytes`. Tokens are stored server-side in the `sessions` table with an expiration timestamp (default 24 hours).
- **Authentication**: Validates sessions via `Authorization: Bearer <token>`, `Cookie: session_token=<token>`, or `?token=<token>` (query string used for direct file downloads).
- **Role-Based Access Control (RBAC)**:
  - `Admin`: Full system access, audit trails, user management, hot backups.
  - `Manager`: Vehicle & driver registry, work orders, expense ledgers, schedule management, report exports. Forbidden from permanent asset deletions, user administration, or audit log manipulation.
  - `Staff`: Viewing assigned vehicles, recording maintenance, updating work orders, submitting digital inspections. Forbidden from financial ledgers, report exports, backups, or user management.
  - `Viewer`: Read-only access to basic dashboards.

### 3.3 Core Fleet Management Modules
- **Vehicles & Drivers (`src/vehicle.c`, `src/driver.c`)**: Manages commercial vehicle assets and driver assignments. Employs soft-delete archiving to preserve relational integrity with historical service records.
- **Preventive Maintenance (`src/maintenance.c`, `src/schedule.c`)**: Tracks service milestones by calendar date and odometer threshold (within 500 km). Completing a recurring service automatically schedules the next milestone.
- **Smart Work Orders (`src/work_order.c`)**: Manages state transitions: `Open -> Assigned -> In Progress -> Completed` (or `Cancelled`). Completing a work order atomically writes the service record and linked expense ledger entry in a single transaction.
- **Digital Vehicle Inspections (`src/inspection.c`)**: Standardized 8-point inspection checklist. Automatically flags defects; any critical defect immediately escalates the overall inspection result to `Fail` and enables direct generation of a high-priority repair work order.
- **Notification Engine (`src/notification.c`)**: Evaluates overdue schedules and critical defects. Deduplicates alerts using stable event keys (`type:entity_id:window_marker`) so repeat polling never creates duplicate alerts.

### 3.4 Operational Data Structures & Algorithms (`src/dsa.c`, `include/dsa.h`)
All algorithms execute in the compiled C11 backend and serve actual operational workflows:
1. **DJB2 Hash Table**: `O(1)` vehicle lookup by registration with case- and punctuation-insensitive key normalization.
2. **Binary Min-Heap**: Priority queue prioritizing urgent maintenance tasks (most overdue days first).
3. **Circular FIFO Queue**: Fair technician dispatch queue.
4. **LIFO Undo Stack**: Safe rollback for non-terminal work order transitions (`Assigned -> Open`, `In Progress -> Assigned`). Blocks undo on completed work orders to preserve financial records.
5. **Dijkstra's Algorithm**: `O((V+E) log V)` logistics depot routing over a 6-node regional network.
6. **Merge Sort**: `O(N log N)` stable vehicle sorting by odometer, fuel efficiency, or registration.
7. **0-1 Knapsack (Dynamic Programming)**: Optimizes safety ROI under a fixed budgetary constraint.
8. **Branch & Bound TSP**: Computes optimal inspection circuits for multi-depot tours.

### 3.5 Database Layer & Isolation (`src/database.c`, `src/main.c`)
- **SQLite Engine**: Prepared statements for all dynamic inputs, parameterized queries (`sqlite3_bind_*`), and transaction management (`BEGIN TRANSACTION`, `COMMIT`, `ROLLBACK`).
- **Migrations**: Incremental schema migrations tracked in `schema_migrations` (`001_initial_schema.sql`, `002_smart_fleet_management.sql`, `003_live_monitoring_and_dsa.sql`).
- **Hot Online Backup**: Online zero-downtime backups via the SQLite Backup API (`sqlite3_backup_*`), saving pre-restore safety snapshots before any restore operation.
- **Database Isolation**:
  - Production database: `data/fleet.db` (never reset or overwritten).
  - Demonstration database: `data/demo_fleet.db` (used when `--demo` flag is provided).
  - Reset guard: `/api/demo/reset` strictly validates that the active database is `demo_fleet.db` and rejects any attempt to reset `fleet.db` with HTTP 403 Forbidden.
