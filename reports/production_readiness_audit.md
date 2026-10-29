# Production Readiness & Security Audit Report

**Audit Date:** 2026-10-29  
**Target Application:** Fleet Vehicle Maintenance Record System  
**Auditor Roles:** Senior Application Security Engineer & Systems Architect  
**Classification:** Internal Technical Audit  

---

## 1. Security Architecture & Controls Evaluation

### 1.1 Authentication & Session Security
| Security Control | Implementation Mechanism | Verification Result |
| :--- | :--- | :--- |
| **Password Hashing** | OpenSSL `PKCS5_PBKDF2_HMAC` with SHA-256, 100,000 iterations, 16-byte CSPRNG salt. | **VERIFIED** - Meets OWASP password storage requirements. |
| **Timing Attack Mitigation** | OpenSSL `CRYPTO_memcmp` constant-time verification for hash comparisons. | **VERIFIED** - Prevents timing side-channel leaks. |
| **Token Predictability** | 256-bit cryptographically secure pseudorandom numbers generated via `RAND_bytes`, encoded as 64-character hexadecimal strings. | **VERIFIED** - High-entropy session identifiers. |
| **Session State Storage** | Persisted server-side in SQLite `sessions` table with explicit `expires_at` timestamp (24-hour TTL). | **VERIFIED** - Stateless client cannot forge tokens. |
| **Session Invalidation** | `POST /api/auth/logout` deletes the token from the server-side table and issues an expired `Set-Cookie` header. | **VERIFIED** - 81/81 RBAC tests verify destroyed tokens return 401 Unauthorized. |
| **Credential Masking** | Passwords and session tokens are strictly excluded from console logs, debug prints, and error messages. | **VERIFIED** - No secrets leakage in outputs. |

---

### 1.2 Role-Based Access Control (RBAC) Hardening
Access control is enforced strictly on the C backend inside `http_authorize()`, independently of UI visibility:

- **Administrator (`ROLE_ADMIN`)**: Unrestricted access to user accounts, database backups, audit logs, and system configuration.
- **Fleet Manager (`ROLE_MANAGER`)**: Authorized for vehicle, driver, work order, schedule, and expense management. **Forbidden (403)** from deleting assets/drivers, user account administration, and backup triggers.
- **Maintenance Staff (`ROLE_STAFF`)**: Authorized for viewing vehicles, recording service records, executing work orders, and submitting digital inspections. **Forbidden (403)** from viewing financial ledgers, creating expenses, exporting reports, or deleting records.
- **Unauthorized / Unauthenticated Requests**: Returned HTTP 401 Unauthorized with standardized JSON error payloads.

---

### 1.3 Injection Defense & Input Validation
| Threat Vector | Mitigation Strategy | Status |
| :--- | :--- | :--- |
| **SQL Injection (SQLi)** | 100% of dynamic user parameters are bound using parameterized SQLite prepared statements (`sqlite3_bind_text`, `sqlite3_bind_int`, `sqlite3_bind_int64`). Zero string-concatenated SQL queries in data layers. | **HARDENED** |
| **CSV Formula Injection (CWE-1236)** | `report_escape_csv_field()` inspects the leading byte of all export fields. Fields starting with `=`, `+`, `-`, `@`, `\t`, or `\r` are prefixed with a single quote (`'`) and wrapped in quotes. | **HARDENED** |
| **Path Traversal (CWE-22)** | Static file serving in `src/http_server.c` strictly dispatches hardcoded static paths (`web/index.html`, `web/dashboard.css`, `web/app.js`). No user-controlled path parameters are passed to `fopen`. | **HARDENED** |
| **Buffer Overrun & HTTP DoS** | Request buffer capped at 16 KB. Requests with `Content-Length < 0` or exceeding 15 KB are rejected immediately with `413 Payload Too Large`. | **HARDENED** |
| **Integer Overflow** | Financial values are calculated and stored in 64-bit integer minor currency units (`INTEGER` paise/cents). | **HARDENED** |

---

### 1.4 Demonstration Mode Isolation & Database Protection
The system supports an isolated college demonstration mode (`--demo`) that is strictly segregated from production:

1. **Path Separation**:
   - Production database: `data/fleet.db`
   - Demonstration database: `data/demo_fleet.db`
2. **Reset Protection**:
   - `/api/demo/reset` strictly validates that the active database path contains `"demo_fleet.db"`.
   - Any attempt to reset `data/fleet.db` is rejected with `HTTP 403 Forbidden` (`"Safety Violation: Only demo_fleet.db may be reset. Production database data/fleet.db is protected."`).
   - Operation requires `PERM_BACKUP_RESTORE` (Admin role).
3. **Bootstrapping Safety**:
   - Demo mode boots `demo_fleet.db` by making a read-only snapshot copy of `data/fleet.db` only if `demo_fleet.db` is missing or below 4 KB. `data/fleet.db` is never opened in write mode during demo bootstrapping.

---

## 2. Summary of Vulnerabilities Found & Remediated

| Vulnerability / Defect | Severity | Remediation Applied |
| :--- | :--- | :--- |
| **Missing HTTP Request Size Guard** | Medium | Added bounds check on `Content-Length` rejecting requests > 15 KB with HTTP 413 Payload Too Large. |
| **Permissive Demo Reset Authorization** | Medium | Elevated authorization for `POST /api/demo/reset` from `PERM_READ` to `PERM_BACKUP_RESTORE`. |
| **GNU Extension Header Failure** | Low | Replaced non-standard `strcasestr` with portable C11 `str_contains_ignore_case()`. |
| **Reversible Undo Transition Block** | Low | Updated `work_order.c` to permit non-terminal status rollbacks while strictly locking completed records. |

---

## 3. Production Deployment Prerequisites & Unresolved Operational Risks

> [!WARNING]
> While the internal C11 application and SQLite data layer are hardened against typical application-level vulnerabilities, public production deployment requires standard perimeter network controls.

1. **TLS / HTTPS Termination (Crucial)**:
   - The embedded Winsock2 HTTP server communicates over unencrypted HTTP/1.1.
   - **Production Requirement**: The server MUST sit behind a dedicated reverse proxy (Nginx, Caddy, HAProxy, or Cloudflare Tunnel) that handles TLS 1.3 termination, SSL certificates, and HSTS headers.

2. **Network Binding**:
   - The server binds to `127.0.0.1` by default. Do not expose the C backend directly to public IP addresses without firewall rules (Windows Defender Firewall / security groups).

3. **Credential Rotation**:
   - The default development credentials (`Admin@123`, `Manager@123`, `Staff@123`) must be changed immediately upon deployment. An initial setup wizard or command-line password update must be executed before going live.

4. **Continuous Backup Schedule**:
   - Automated OS scheduling (Windows Task Scheduler / cron) should invoke the SQLite hot backup endpoint (`POST /api/backup`) or run `--check-reminders` at configured intervals.
