# Codebase Cleanup & Architecture Refactoring Report

**Date:** 2026-10-29  
**Repository:** Fleet Vehicle Maintenance Record System  
**Stack:** C11 | Native Windows Winsock2 | SQLite3 | Vanilla HTML5/CSS3/ES6  

---

## 1. Executive Summary

This report documents the architectural inspection, cleanup, and refactoring executed across the codebase. All existing technology constraints, working features, and production databases (`data/fleet.db`) were preserved without destruction.

A total of 366 automated verification checks across 4 separate test suites confirmed 100% functionality and zero regressions after all refactoring and cleanup actions.

---

## 2. Inventory of Changes & Refactoring Actions

### 2.1 Backend Architecture & Source Code (`src/`, `include/`)

1. **Resolution of Duplicate Operational Handlers in `src/http_server.c`:**
   - *Issue*: An early draft block of operational handlers was duplicated at line 331 and line 2223, causing compiler symbol redefinition errors (`redefinition of 'handle_api_work_order_queue'`, `resolve_hub_id_dijkstra`, etc.).
   - *Action*: Removed the obsolete early draft block and retained the comprehensive operational implementation with strict JSON schema compliance and telemetry metadata.

2. **Portable C11 Case-Insensitive Substring Matcher:**
   - *Issue*: Non-standard GNU extension `strcasestr` was referenced in C code, leading to compilation failures on standard Windows UCRT64 toolchains.
   - *Action*: Implemented a clean, portable C11 helper `str_contains_ignore_case(const char *haystack, const char *needle)` using `tolower((unsigned char)...)` directly in `src/http_server.c`.

3. **Reversible Status Undo (LIFO Stack) Lifecycle Validation:**
   - *Issue*: The status validator in `src/work_order.c` strictly disallowed backward transitions, preventing the LIFO stack from rolling back `Assigned -> Open` and `In Progress -> Assigned`.
   - *Action*: Refactored `is_valid_status_transition()` to explicitly permit safe non-terminal rollbacks while strictly prohibiting rollbacks on `Completed` work orders to safeguard financial records.

4. **HTTP Request Buffer & Content-Length DoS Hardening:**
   - *Issue*: Excessively large or negative `Content-Length` headers could cause buffer overrun risks or unexpected loops during `recv`.
   - *Action*: Added explicit bounds checking (`content_len < 0 || content_len > sizeof(req_buf) - 512`), immediately returning HTTP `413 Payload Too Large` for oversized requests.

5. **Demo Database Reset Authorization Hardening:**
   - *Issue*: `handle_api_demo_reset` previously required only `PERM_READ`.
   - *Action*: Elevated required permission to `PERM_BACKUP_RESTORE` (Admin-only privilege) and enforced strict validation that the active database path contains `demo_fleet.db`.

6. **Deduplication of Database Bootstrapping in `src/main.c`:**
   - *Issue*: A duplicate file-open check (`FILE *f_check`) existed in the `--demo` argument parser.
   - *Action*: Cleaned duplicate variable declarations and streamlined the bootstrapping routine to safely copy `data/fleet.db` into `data/demo_fleet.db` only when `demo_fleet.db` is missing or below 4 KB.

---

### 2.2 Build System & Toolchain Improvements (`Makefile`, `.clang-format`)

1. **Enhanced Makefile Targets:**
   - Added `check`: Runs compiler syntax check and diagnostic warning validation (`-fsyntax-only`) across all C sources, main, and tests.
   - Added `format`: Triggers `clang-format -i` across all `src/*.c`, `include/*.h`, and `tests/*.c` files.
   - Added `format-check`: Runs non-destructive format validation (`clang-format --dry-run --Werror`).
   - Added `help`: Displays a formatted, shell-safe summary of all makefile targets.
   - Refactored `clean`: Uses safe Python glob removals for `*.o`, `fleet.exe`, `test_runner.exe`, and temporary `data/test_*.db` files. **Guaranteed never to delete `data/fleet.db`, migrations, reports, or backups.**

2. **Established `.clang-format`:**
   - Created `.clang-format` based on LLVM/C11 standards (4-space indent, attach braces, right-aligned pointers, 100-character column limit).
   - Designed Makefile targets to produce clear, actionable error messages if `clang-format` is not installed on PATH.

---

### 2.3 Safe Removal of Intermediate Scrap Files (`scratch/`)

The `scratch/` directory was audited. Intermediate build scrap, temporary text dumps, and one-off replays were safely removed, while persistent end-to-end integration scripts were preserved:

| File Removed | Reason for Removal | Status |
| :--- | :--- | :--- |
| `scratch/step2180_rc.txt` | Temporary tool replacement chunk | Removed |
| `scratch/step2186_rc.txt` | Temporary tool replacement chunk | Removed |
| `scratch/step2186_tc.txt` | Temporary tool replacement chunk | Removed |
| `scratch/clean_operational_handlers.c` | Intermediate file assembly snippet | Removed |
| `scratch/http_server_from_transcript.c`| Intermediate dump from log recovery | Removed |
| `scratch/http_server_replayed.c` | Intermediate reconstructed source | Removed |
| `scratch/build_operational_handlers.py`| Intermediate generator script | Removed |
| `scratch/assemble_http_server.py` | Intermediate assembly script | Removed |
| `scratch/replay_edits.py` | One-off replay utility | Removed |

**Preserved in `scratch/`**: Developer diagnostic tools (`e2e_api_workflow_test.py`, `check_endpoints.py`, `inspect_ui.py`, etc.).

---

### 2.4 Documentation Modernization (`docs/`, `README.md`)

1. **Created `docs/architecture.md`**:
   - Comprehensive technical design document covering network architecture, security & RBAC design, operational DSA engine, SQLite persistence layer, and database isolation.
2. **Created `docs/development.md`**:
   - Step-by-step development manual detailing MinGW/MSYS2 toolchain setup, build workflows, testing procedures, database backup/recovery operations, and troubleshooting.
3. **Updated `README.md`**:
   - Synchronized directory tree with all newly created headers (`work_order.h`, `inspection.h`), migrations (`002_*.sql`, `003_*.sql`), and test suites.
   - Added references to architecture and development documentation.

---

## 3. Verification & Regression Analysis

All changes were validated by compiling with `-Wall -Wextra -Wpedantic` and executing the full 4-tier test battery:

- **Compiler Diagnostics (`mingw32-make check`)**: Clean (Exit code 0).
- **C Unit Tests (`mingw32-make test`)**: 199 / 199 Passed (100%).
- **Live RBAC Tests (`test_rbac_live.py`)**: 81 / 81 Passed (100%).
- **Smart Fleet E2E Tests (`test_smart_fleet_e2e.py`)**: 33 / 33 Passed (100%).
- **Live Monitoring & DSA (`test_live_monitoring_dsa.py`)**: 53 / 53 Passed (100%).

**Total Passing Assertions:** 366 / 366 (100% Pass Rate, 0 Failures, 0 Regressions).
