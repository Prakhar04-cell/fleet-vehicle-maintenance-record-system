# Fleet Vehicle Maintenance Record System — Central Role-Permission Matrix

## 1. Executive Summary

This document defines the central Role-Based Access Control (RBAC) architecture, permission matrix, and enforcement mechanisms implemented across the native **C11 Winsock2 HTTP backend**, **SQLite persistent data engine**, and **Enterprise SaaS Vanilla JavaScript Web Client**.

Authorization is strictly resolved and verified on the server side via **256-bit cryptographically secure session tokens (`RAND_bytes`)** and **`HttpOnly; SameSite=Strict` session cookies**. Client-supplied roles in request bodies, query strings, or localStorage are never trusted. All protected endpoints strictly return **HTTP 401 Unauthorized** for unauthenticated requests and **HTTP 403 Forbidden** for authenticated users attempting unauthorized operations.

---

## 2. Granular Permission Definitions (`include/auth.h`)

Permissions are modeled as bitwise flags in C11:

```c
typedef enum {
    PERM_READ              = 1 << 0,  /* 0x0001: Read vehicles, maintenance, schedules, telemetry */
    PERM_MAINTENANCE_WRITE = 1 << 1,  /* 0x0002: Create and update maintenance records */
    PERM_SCHEDULE_WRITE    = 1 << 2,  /* 0x0004: Create, evaluate, reschedule, and complete schedules */
    PERM_VEHICLE_WRITE     = 1 << 3,  /* 0x0008: Register new vehicles, edit operational specifications */
    PERM_EXPENSE_WRITE     = 1 << 4,  /* 0x0010: Record and manage operating expense ledgers */
    PERM_REPORTS           = 1 << 5,  /* 0x0020: Export audit-grade CSV and binary PDF dossiers */
    PERM_USER_MANAGE       = 1 << 6,  /* 0x0040: User administration, account provisioning, role changes */
    PERM_BACKUP_RESTORE    = 1 << 7,  /* 0x0080: Hot online database snapshots and disaster recovery */
    PERM_VEHICLE_DELETE    = 1 << 8,  /* 0x0100: Destructive permanent vehicle asset deletion */
    PERM_DRIVER_WRITE      = 1 << 9,  /* 0x0200: Commercial driver registration, profile update, assignment */
    PERM_AUDIT_VIEW        = 1 << 10  /* 0x0400: Inspection of immutable security and system audit trail */
} UserPermission;
```

---

## 3. Comprehensive Role-Permission Matrix

| Functional Capability | C11 Permission Flag | Administrator (`admin`) | Fleet Manager (`manager`) | Maintenance Staff (`staff`) | Viewer / Read-Only (`viewer`) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **View Fleet Telemetry & KPIs** | `PERM_READ` | **ALLOWED** | **ALLOWED** | **ALLOWED** | **ALLOWED** |
| **View Vehicles Register** | `PERM_READ` | **ALLOWED** | **ALLOWED** | **ALLOWED** | **ALLOWED** |
| **Add / Edit Vehicle Specs** | `PERM_VEHICLE_WRITE` | **ALLOWED** | **ALLOWED** | DENIED (403) | DENIED (403) |
| **Delete Vehicle Asset** | `PERM_VEHICLE_DELETE` | **ALLOWED** | DENIED (403) | DENIED (403) | DENIED (403) |
| **View Commercial Drivers** | `PERM_READ` | **ALLOWED** | **ALLOWED** | DENIED (403) | DENIED (403) |
| **Add / Update / Assign Driver** | `PERM_DRIVER_WRITE` | **ALLOWED** | **ALLOWED** | DENIED (403) | DENIED (403) |
| **Delete Driver Account** | `PERM_USER_MANAGE` | **ALLOWED** | DENIED (403) | DENIED (403) | DENIED (403) |
| **View Service History** | `PERM_READ` | **ALLOWED** | **ALLOWED** | **ALLOWED** | **ALLOWED** |
| **Record Maintenance Work Order** | `PERM_MAINTENANCE_WRITE` | **ALLOWED** | **ALLOWED** | **ALLOWED** | DENIED (403) |
| **View Preventive Schedules** | `PERM_READ` | **ALLOWED** | **ALLOWED** | **ALLOWED** | **ALLOWED** |
| **Create / Reschedule Milestone** | `PERM_SCHEDULE_WRITE` | **ALLOWED** | **ALLOWED** | **ALLOWED** | DENIED (403) |
| **Complete Service Milestone** | `PERM_SCHEDULE_WRITE` | **ALLOWED** | **ALLOWED** | **ALLOWED** | DENIED (403) |
| **Evaluate Schedule Thresholds**| `PERM_SCHEDULE_WRITE` | **ALLOWED** | **ALLOWED** | **ALLOWED** | DENIED (403) |
| **View Operating Expenses** | `PERM_EXPENSE_WRITE` | **ALLOWED** | **ALLOWED** | DENIED (403) | DENIED (403) |
| **Record Operating Expense** | `PERM_EXPENSE_WRITE` | **ALLOWED** | **ALLOWED** | DENIED (403) | DENIED (403) |
| **Export CSV Reports** | `PERM_REPORTS` | **ALLOWED** | **ALLOWED** | DENIED (403) | DENIED (403) |
| **Generate PDF Executive Dossiers** | `PERM_REPORTS` | **ALLOWED** | **ALLOWED** | DENIED (403) | DENIED (403) |
| **Inspect System Audit Trail** | `PERM_AUDIT_VIEW` | **ALLOWED** | DENIED (403) | DENIED (403) | DENIED (403) |
| **Trigger Online Hot Backup** | `PERM_BACKUP_RESTORE` | **ALLOWED** | DENIED (403) | DENIED (403) | DENIED (403) |
| **Restore Database Snapshot** | `PERM_BACKUP_RESTORE` | **ALLOWED** | DENIED (403) | DENIED (403) | DENIED (403) |
| **List & Manage User Accounts** | `PERM_USER_MANAGE` | **ALLOWED** | DENIED (403) | DENIED (403) | DENIED (403) |
| **Run DSA Benchmarks** | `PERM_USER_MANAGE` | **ALLOWED** | DENIED (403) | DENIED (403) | DENIED (403) |

---

## 4. Protected Endpoints & C11 Authorization Mapping

| HTTP Verb | API Endpoint | Required Permission Flag | Enforcing Function | Unauthorized Behavior |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/dashboard` | `PERM_READ` | `handle_api_dashboard` | `401 Unauthorized` |
| `GET` | `/api/vehicles` | `PERM_READ` | `handle_api_vehicles` | `401 Unauthorized` |
| `POST` | `/api/vehicles` (insert/update) | `PERM_VEHICLE_WRITE` | `handle_api_vehicles` | `403 Forbidden` |
| `DELETE` | `/api/vehicles` | `PERM_VEHICLE_DELETE` | `handle_api_vehicles` | `403 Forbidden` |
| `GET` | `/api/drivers` | `PERM_READ` | `handle_api_drivers` | `401 Unauthorized` |
| `POST` | `/api/drivers` | `PERM_DRIVER_WRITE` | `handle_api_drivers` | `403 Forbidden` |
| `DELETE` | `/api/drivers` | `PERM_USER_MANAGE` | `handle_api_drivers` | `403 Forbidden` |
| `GET` | `/api/maintenance` | `PERM_READ` | `handle_api_maintenance` | `401 Unauthorized` |
| `POST` | `/api/maintenance` | `PERM_MAINTENANCE_WRITE` | `handle_api_maintenance` | `403 Forbidden` |
| `GET` | `/api/schedules` | `PERM_READ` | `handle_api_schedules` | `401 Unauthorized` |
| `POST` | `/api/schedules` | `PERM_SCHEDULE_WRITE` | `handle_api_schedules` | `403 Forbidden` |
| `POST` | `/api/evaluate` | `PERM_SCHEDULE_WRITE` | `handle_api_evaluate` | `403 Forbidden` |
| `GET` | `/api/expenses` | `PERM_EXPENSE_WRITE` | `handle_api_expenses` | `403 Forbidden` |
| `POST` | `/api/expenses` | `PERM_EXPENSE_WRITE` | `handle_api_expenses` | `403 Forbidden` |
| `GET` | `/api/notifications` | `PERM_READ` | `handle_api_notifications` | `401 Unauthorized` |
| `GET` | `/api/reports/csv` | `PERM_REPORTS` | `handle_api_reports_csv` | `403 Forbidden` |
| `GET` | `/api/reports/pdf` | `PERM_REPORTS` | `handle_api_reports_pdf` | `403 Forbidden` |
| `GET` | `/api/audit` | `PERM_AUDIT_VIEW` | `handle_api_audit` | `403 Forbidden` |
| `POST` | `/api/backup` | `PERM_BACKUP_RESTORE` | `handle_api_backup` | `403 Forbidden` |
| `GET` | `/api/auth/session` | Valid Session Token | `handle_api_auth` | `401 Unauthorized` |
| `POST` | `/api/auth/login` | None (Public Auth) | `handle_api_auth` | `401 Unauthorized` on bad credentials |
| `POST` | `/api/auth/logout` | None (Public Invalidation) | `handle_api_auth` | `200 OK` (token deleted, cookie cleared) |
| `GET` | `/api/auth/users` | `PERM_USER_MANAGE` | `handle_api_auth` | `403 Forbidden` |
| `GET` | `/api/dsa/run` | `PERM_USER_MANAGE` | `handle_api_dsa_run` | `403 Forbidden` |

---

## 5. Distinct Role Dashboard Experiences

### A. Administrator Dashboard ("System Overview")
- **Header Banner**: "System Overview" with royal-blue Administrator pill.
- **Widgets**:
  - 7 System-wide telemetry KPI cards (Total Active Assets, Available, On Trip, In Bay, Out of Service, Overdue Milestones, Total Spending).
  - SVG Fleet Donut Chart, Monthly Operating Expenses Trend SVG chart, Service Category Cost SVG chart.
  - Urgent Reminders & Actions table (`#table-urgent-schedules`).
  - Live Security & System Audit Events table (`#table-admin-recent-audit`).
  - System WAL & Engine Readiness card.
- **Navigation**: Full navigation containing Fleet Register, Commercial Drivers, Service History, Schedules, Reminders, Financials, Reports & Analytics, DSA Algorithms, Audit Trail, and Disaster Recovery.
- **Special Capabilities**: Permanent vehicle deletion, driver account deletion, hot online backups, database restoration, user account management.

### B. Fleet Manager Dashboard ("Fleet Operations")
- **Header Banner**: "Fleet Operations" with Fleet Manager pill.
- **Widgets**:
  - 6 Operational KPI cards (Total Fleet, Available for Dispatch, In Service Bay, Overdue Milestones, Due Soon, Current Month Spending).
  - Operations Quick Actions: + Add Vehicle, + Log Service, + Schedule Milestone, Assign Driver.
  - Driver & Commercial Vehicle Allocation Roster (`#table-mgr-allocations`): Lists driver name, commercial license, assigned vehicle, license expiry, readiness status, and one-click "Reassign Driver" drawer.
  - Actionable Overdue Alerts (`#table-mgr-urgent-schedules`).
- **Navigation Restrictions**: System & Security navigation group (DSA Algorithms, Audit Trail, Disaster Recovery) is completely hidden.
- **Action Restrictions**: Red "Delete Asset" button in vehicles register is completely omitted from DOM. Driver deletion is prohibited. Backup triggering is forbidden.

### C. Maintenance Staff Dashboard ("Maintenance Workspace")
- **Header Banner**: "Maintenance Workspace" with Workshop Service Bay pill.
- **Widgets**:
  - Workshop Service Workload Hero Card with 4 real-time counters:
    - *Vehicles In Bay* (`#kpi-staff-bay`)
    - *Overdue Services* (`#kpi-staff-overdue`)
    - *Due Soon (7 Days)* (`#kpi-staff-upcoming`)
    - *Completed Records* (`#kpi-staff-completed`)
  - Pending & Scheduled Work Orders table (`#table-staff-schedules`): itemizes scheduled services with direct "+ Record Service" prefilling modal and "Complete" one-click schedule workflow.
  - Active Workshop Bays widget (`#staff-bay-queue`): real-time cards of vehicles currently with status 'Maintenance'.
  - Recent Service History Log table (`#table-staff-recent-maint`).
- **Navigation Restrictions**: Commercial Drivers, Operating Expenses, Reports & Analytics, DSA, Audit Trail, and Disaster Recovery are hidden. Navigation is streamlined to Workshop Service Bay, Preventive Schedules, and Vehicle Profiles.
- **Action Restrictions**: Vehicle editing, vehicle deletion, driver management, financial expense ledger modification, and system backups are strictly prohibited.

---

## 6. Authentication & Session Security Architecture

1. **PBKDF2-HMAC-SHA256 Password Verification**:
   - 100,000 iterations using unique 16-byte cryptographically random salt per user.
   - Constant-time comparison (`CRYPTO_memcmp`) prevents timing attack vulnerabilities.
2. **Cryptographic Token Generation**:
   - 256 bits of entropy generated using OpenSSL `RAND_bytes`.
   - Formatted as a 64-character lowercase hexadecimal string.
3. **Session Management (`src/auth.c`)**:
   - Thread-safe in-memory cache of 64 active session slots.
   - Sliding 24-hour expiration window (`AUTH_SESSION_TTL_SEC = 86400`).
   - Every request validates that the user account still exists and remains `active = 1` in SQLite.
4. **Cookie Security**:
   - Cookies are set with `HttpOnly; SameSite=Strict; Path=/; Max-Age=86400`.
   - Tokens can also be supplied via standard `Authorization: Bearer <token>` header or query param `?token=<token>`.
5. **Session Destruction**:
   - `POST /api/auth/logout` explicitly zeroizes the session cache slot on the server and expires the cookie (`Max-Age=0`).
   - Subsequent calls with the invalidated token immediately return `401 Unauthorized`.

---

## 7. Remaining Limitations

1. **In-Memory Session Cache**: Sessions are maintained in process memory (up to 64 concurrent active sessions). Restarting the HTTP server terminates active sessions, requiring re-authentication.
2. **Localhost Binding**: The native Winsock2 HTTP server binds to `127.0.0.1` by default for local security. Production internet deployments require a TLS reverse proxy (e.g., Nginx or Caddy) to terminate HTTPS.
3. **Multi-Factor Authentication (MFA)**: Hardware tokens / TOTP are not currently supported; password authentication relies on PBKDF2-HMAC-SHA256.
