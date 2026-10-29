# Fleet Vehicle Maintenance Record System

[![Build & Test](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![C Standard](https://img.shields.io/badge/standard-C11-blue.svg)]()
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Winsock2-lightgrey.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-382%20passing-success.svg)](reports/final_release_checklist.md)

An enterprise-grade fleet management and preventive maintenance system built in **pure C11** with an embedded **native Windows Winsock2 HTTP server**, an **SQLite3** persistence layer, and a **Vanilla JavaScript/CSS** dashboard. Designed for high reliability, zero external framework dependencies, and operational safety.

---

## Key Features

- **Commercial Asset & Driver Management**: Complete vehicle lifecycle register with driver allocations, non-negative odometer enforcement, and soft-archive preservation.
- **Preventive Maintenance Engine**: Dual calendar-date and odometer-threshold milestone tracking with automated recurring service scheduling and reminder notifications.
- **Smart Work Orders & Defect Tracking**: Multi-stage state machine (`Open` -> `Assigned` -> `In Progress` -> `Completed`) with 8-point digital vehicle inspections, critical defect escalation, and atomic financial reconciliation.
- **Financial Analytics & Minor-Unit Math**: Accurate multi-category expense ledgers calculated in 64-bit integer minor currency units (paise/cents) to eliminate floating-point drift.
- **Server-Side RBAC & PBKDF2 Hashing**: Cryptographic password hashing (OpenSSL PBKDF2-HMAC-SHA256, 100,000 rounds) and high-entropy 64-character session tokens with role segregation for Admin, Fleet Manager, and Maintenance Staff.
- **Zero-Downtime Hot Backups & PDF/CSV Export**: Online database hot backups via SQLite Backup API and genuine vector PDF 1.4 document generation with RFC 4180 CSV formula-injection protection.
- **Operational C11 Data Structures & Algorithms**: Real-world backend execution of DJB2 Hash Tables ($O(1)$ vehicle lookup), Binary Min-Heaps (urgency priority queues), FIFO Queues, LIFO Undo Stacks, Dijkstra's algorithm (depot pathfinding), Merge Sort, 0-1 Knapsack budget optimizer, and Branch & Bound TSP.
- **Isolated Demonstration Mode**: Safe sandboxed evaluation mode (`--demo`) for college demonstrations that strictly protects the production database from modifications or resets.

---

## Technology Stack

| Layer | Component | Description |
| :--- | :--- | :--- |
| **Language** | C11 Standard | Compiled with `-Wall -Wextra -Wpedantic -O2` |
| **Networking** | Native Winsock2 | Non-blocking HTTP/1.1 micro-server bound to `127.0.0.1` |
| **Database** | SQLite 3.x | Parameterized prepared statements, WAL transactions, foreign keys |
| **Security** | OpenSSL 3.x / EVP | PBKDF2-HMAC-SHA256 (100k rounds), CSPRNG session tokens |
| **Frontend** | Vanilla JS / CSS3 / HTML5 | Responsive SaaS dashboard, SVG charts, zero framework bloat |
| **Build System** | GNU Make | Unified targets for builds, tests, checks, and formatting |

---

## Architecture Overview

```
+-------------------------------------------------------------------------+
|                  Modern Web Browser (Vanilla JS / CSS)                  |
+-------------------------------------------------------------------------+
                                    |
                             HTTP/1.1 (JSON)
                                    v
+-------------------------------------------------------------------------+
|           Embedded C11 Winsock2 HTTP Server (src/http_server.c)         |
|         Request Parser | CORS | Content-Length Guards | REST Router     |
+-------------------------------------------------------------------------+
           |                                              |
           v                                              v
+-----------------------+                      +--------------------------+
|  RBAC & Session Auth  |                      | Operational DSA Engine   |
| (PBKDF2, CSPRNG Token)|                      | (Hash, Heap, Dijkstra)  |
+-----------------------+                      +--------------------------+
           |                                              |
           +----------------------+-----------------------+
                                  v
+-------------------------------------------------------------------------+
|                          Business Logic Layer                           |
|   Vehicles | Schedules | Work Orders | Inspections | Expenses | Alerts  |
+-------------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------------+
|                 SQLite Persistence (src/database.c)                     |
|            Prepared Statements | Hot Online Backups | Migrations         |
+-------------------------------------------------------------------------+
           |                                              |
           v                                              v
+-----------------------+                      +--------------------------+
| data/fleet.db (Prod)  |                      | data/demo_fleet.db (Demo)|
+-----------------------+                      +--------------------------+
```

---

## Quick Start

### 1. Prerequisites (Windows / MSYS2 UCRT64)
Ensure GCC (C11), GNU Make, SQLite3, and OpenSSL are installed via MSYS2:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
```
Add toolchain to your system `PATH`:
```cmd
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%
```

### 2. Build the Application
```cmd
mingw32-make all
```

### 3. Launch the Server

- **Standard Production Server** (binds to `127.0.0.1:8080`, uses `data/fleet.db`):
  ```cmd
  fleet.exe --web 8080
  ```
- **Isolated College Demonstration Mode** (uses sandboxed `data/demo_fleet.db`):
  ```cmd
  fleet.exe --demo --web 8080
  ```
- **Interactive Terminal Console**:
  ```cmd
  fleet.exe
  ```

Open **`http://localhost:8080`** in your browser.

---

## Automated Test Suites

The codebase includes an extensive 4-tier automated test suite:

```cmd
# 1. C Unit Tests (199 assertions)
mingw32-make test

# 2. Live HTTP RBAC Security Suite (81 assertions)
python tests/test_rbac_live.py

# 3. Smart Fleet End-to-End Suite (33 assertions)
python tests/test_smart_fleet_e2e.py

# 4. Live Monitoring & Operational DSA Suite (53 assertions)
python -u tests/test_live_monitoring_dsa.py
```

**Grand Total**: **382 / 382 Automated Assertions Passing (100% Pass Rate)**. See [reports/final_release_checklist.md](reports/final_release_checklist.md) for full execution logs and exit codes.

---

## Documentation Directory

| Document | Purpose |
| :--- | :--- |
| [docs/architecture.md](docs/architecture.md) | Technical architecture, layered subsystems, and database isolation |
| [docs/development.md](docs/development.md) | Development setup, toolchain commands, backup/restore, and background tasks |
| [reports/role_permission_matrix.md](reports/role_permission_matrix.md) | Comprehensive server-side RBAC policy table |
| [reports/production_readiness_audit.md](reports/production_readiness_audit.md) | Security audit, vulnerability remediations, and reverse-proxy guidelines |
| [reports/final_release_checklist.md](reports/final_release_checklist.md) | Independent verification scorecard and Go/No-Go release decisions |
| [reports/codebase_cleanup_report.md](reports/codebase_cleanup_report.md) | Refactoring history and file inventory |

---

## Security & Deployment Notes

- **Credential Safety**: Initial production startup prompts the operator to configure custom administrator credentials via an interactive wizard. Demonstration accounts are strictly confined to isolated demonstration databases.
- **Localhost Binding**: The HTTP server binds exclusively to `INADDR_LOOPBACK` (`127.0.0.1`) for local security.
- **Public Deployment Requirement**: For public internet hosting, the server must be deployed behind a TLS-terminating reverse proxy (e.g., Nginx, Caddy, or Cloudflare Tunnel) configured with HTTPS certificates.

---

## License

This project is licensed under the [MIT License](LICENSE).
