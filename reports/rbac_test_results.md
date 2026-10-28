# Fleet Vehicle Maintenance Record System — Automated RBAC Test Results

## 1. Test Execution Summary

| Test Suite | Target Component | Assertions Run | Assertions Passed | Assertions Failed | Status |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **C11 Native Core & Security Suite** | `tests/test_main.c` / C11 Backend | 177 | 177 | 0 | **100% PASS** |
| **Live C11 HTTP RBAC Suite** | `tests/test_rbac_live.py` / Winsock2 Server | 68 | 68 | 0 | **100% PASS** |
| **Total Automated Assertions** | **Full System Stack** | **245** | **245** | **0** | **100% PASS** |

---

## 2. Test Execution Details

### A. C11 Native Unit Test Suite (`tests/test_main.c`)
- **Execution Command**: `mingw32-make test`
- **Compiler**: GCC 14.2.0 (UCRT64 MSYS2 Toolchain)
- **Database Used**: `data/test_fleet.db` (Isolated, zero production pollution)
- **Key Modules Verified**:
  - `auth_has_permission` bitmask validation across all 4 roles (`ROLE_ADMIN`, `ROLE_MANAGER`, `ROLE_STAFF`, `ROLE_VIEWER`).
  - Cryptographic token generation (`RAND_bytes`, 256 bits, 64-hex chars).
  - Sliding TTL validation (24 hours).
  - Malformed and NULL token rejection.
  - Invalidation on session destruction (`auth_session_destroy`).
  - Live account status checks (rejects deactivated user accounts even if session token exists).
  - CRUD operations for vehicles, commercial drivers, maintenance work orders, preventive schedules, expenses, and PDF/CSV reporting.
  - DSA algorithms (Hash Table, BST, Min-Heap, FIFO Queue, LIFO Stack, Merge Sort, Dijkstra, DP Knapsack, Branch & Bound TSP).

```text
=================================================================
 TEST RESULTS SUMMARY: 177 / 177 Tests Passed (100%)
=================================================================
```

---

### B. Live C11 HTTP Winsock2 Server RBAC Suite (`tests/test_rbac_live.py`)
- **Execution Command**: `python tests/test_rbac_live.py`
- **Runtime Tested**: Native C11 Winsock2 HTTP Server running live on port `8089` against a sandbox copy (`data/test_rbac.db`).
- **Results**: **68 Passed, 0 Failed**.

#### 1. Anonymous / Unauthenticated Request Rejection (401 Unauthorized)
- Verified that all protected endpoints deny unauthenticated requests with `HTTP 401 Unauthorized` and `{"success": false, "code": 401}`:
  - `GET /api/dashboard` -> `401 Unauthorized` [PASS]
  - `GET /api/vehicles` -> `401 Unauthorized` [PASS]
  - `GET /api/drivers` -> `401 Unauthorized` [PASS]
  - `GET /api/maintenance` -> `401 Unauthorized` [PASS]
  - `GET /api/schedules` -> `401 Unauthorized` [PASS]
  - `GET /api/expenses` -> `401 Unauthorized` [PASS]
  - `GET /api/audit` -> `401 Unauthorized` [PASS]
  - `POST /api/backup` -> `401 Unauthorized` [PASS]
  - `GET /api/reports/csv?type=vehicles` -> `401 Unauthorized` [PASS]
  - `GET /api/reports/pdf?type=fleet` -> `401 Unauthorized` [PASS]
  - `GET /api/auth/session` -> `401 Unauthorized` [PASS]
  - `GET /api/auth/users` -> `401 Unauthorized` [PASS]
  - Forged / invalid 64-hex token -> `401 Unauthorized` [PASS]

#### 2. Maintenance Staff Role Permissions (`staff` / `Staff@123`)
- **Login Verification**: Returns signed 64-hex session token, identifies role as `staff`.
- **Permitted Operations (200 / 201)**:
  - `GET /api/dashboard` -> `200 OK` [PASS]
  - `GET /api/vehicles` -> `200 OK` [PASS]
  - `GET /api/maintenance` -> `200 OK` [PASS]
  - `GET /api/schedules` -> `200 OK` [PASS]
  - `POST /api/maintenance` (log service) -> `201 Created` [PASS]
- **Forbidden Operations Strictly Denied with 403 Forbidden**:
  - `DELETE /api/vehicles?id=1` -> `403 Forbidden` [PASS]
  - `POST /api/drivers` -> `403 Forbidden` [PASS]
  - `DELETE /api/drivers?id=1` -> `403 Forbidden` [PASS]
  - `GET /api/expenses` -> `403 Forbidden` [PASS]
  - `POST /api/expenses` -> `403 Forbidden` [PASS]
  - `GET /api/audit` -> `403 Forbidden` [PASS]
  - `POST /api/backup` -> `403 Forbidden` [PASS]
  - `GET /api/reports/csv?type=vehicles` -> `403 Forbidden` [PASS]
  - `GET /api/reports/pdf?type=fleet` -> `403 Forbidden` [PASS]
  - `GET /api/auth/users` -> `403 Forbidden` [PASS]

#### 3. Fleet Manager Role Permissions (`manager` / `Manager@123`)
- **Login Verification**: Returns signed 64-hex session token, identifies role as `manager`.
- **Permitted Operational Workflows (200 / 201)**:
  - `GET /api/dashboard` -> `200 OK` [PASS]
  - `GET /api/vehicles` -> `200 OK` [PASS]
  - `POST /api/vehicles` (update specifications, assign driver) -> `200 OK` [PASS]
  - `GET /api/drivers` -> `200 OK` [PASS]
  - `POST /api/drivers` (register driver) -> `201 Created` [PASS]
  - `GET /api/expenses` -> `200 OK` [PASS]
  - `POST /api/expenses` (record operational ledger entry) -> `201 Created` [PASS]
  - `GET /api/reports/csv?type=vehicles` -> `200 OK` [PASS]
  - `GET /api/reports/pdf?type=fleet` -> `200 OK` [PASS]
- **Forbidden Operations Strictly Denied with 403 Forbidden**:
  - `DELETE /api/vehicles?id=1` -> `403 Forbidden` [PASS]
  - `DELETE /api/drivers?id=1` -> `403 Forbidden` [PASS]
  - `GET /api/audit` -> `403 Forbidden` [PASS]
  - `POST /api/backup` -> `403 Forbidden` [PASS]
  - `GET /api/auth/users` -> `403 Forbidden` [PASS]

#### 4. Administrator Role Permissions (`admin` / `Admin@123`)
- **Login Verification**: Returns signed 64-hex session token, identifies role as `admin`.
- **Privileged Administrative Operations (200 / 201)**:
  - `GET /api/audit` -> `200 OK` (Live Security Audit Trail list returned) [PASS]
  - `POST /api/backup` -> `201 Created` (Online hot backup generated via SQLite Backup API) [PASS]
  - `GET /api/auth/users` -> `200 OK` (Full user account security list) [PASS]

#### 5. Logout & Session Invalidation
- `POST /api/auth/logout` -> `200 OK` [PASS]
- `Set-Cookie` header sets `Max-Age=0` to purge client cookie [PASS]
- Subsequent request with destroyed token returns `401 Unauthorized` [PASS]
- `/api/auth/session` returns `401 Unauthorized` [PASS]

---

## 3. Visual & UI Dashboard Verification

- **Administrator**: Verified "System Overview" header banner, 7 KPI cards, SVG charts, live audit log feed, and unrestricted navigation.
- **Fleet Manager**: Verified "Fleet Operations" header banner, 6 operational KPIs, Driver & Vehicle Allocation Roster, Actionable Overdue Alerts, omission of vehicle deletion buttons, and hidden System & Security navigation group.
- **Maintenance Staff**: Verified "Maintenance Workspace" header banner, Workshop Hero Card with 4 counters, Pending & Scheduled Work Orders table with quick "+ Record Service" and "Complete" buttons, Active Workshop Bays widget, and recent service log. Restrictive sidebar hiding drivers, financial ledgers, and administration.

---

## 4. Conclusion

The RBAC implementation is verified at both the backend HTTP protocol layer and the frontend presentation layer. All 245 assertions across the unit and live integration suites pass with 0 regressions.
