# CREW / DECK

> **"Mission control for people."**  
> A high-performance, distinctive Employee Management System engineered in C11.  
> Ships as a single self-contained binary serving both the REST API and the vanilla frontend.

---

```
                       +-----------------------------------+
                       |         BROWSER CLIENT            |
                       |  (Vanilla HTML5 / CSS3 / ES6 JS)  |
                       +-----------------+-----------------+
                                         |
                                HTTP / WebSocket
                                         |
                                         v
+-------------------------------------------------------------------------+
|                    CREW/DECK EMBEDDED C11 BINARY                        |
|                                                                         |
|  +--------------------+   +-------------------+   +------------------+  |
|  |      ROUTER        |   |   ARGON2ID AUTH   |   |   RATE LIMITER   |  |
|  |  (Mongoose v7.15)  |   |    (libsodium)    |   |  (Token Bucket)  |  |
|  +---------+----------+   +---------+---------+   +--------+---------+  |
|            |                        |                      |            |
|            +-------------------+----+----------------------+            |
|                                |                                        |
|                                v                                        |
|  +-------------------------------------------------------------------+  |
|  |                    CONTROLLER DOMAIN SERVICES                     |  |
|  |  [Employees]  [Departments]  [Attendance]  [Leave]  [Payroll]     |  |
|  |  [Constellation Graph]  [Dashboard Metrics]  [Audit Trail]        |  |
|  +-----------------------------+-------------------------------------+  |
|                                |                                        |
|                                v                                        |
|  +-------------------------------------------------------------------+  |
|  |                   PREPARED SQLITE3 ENGINE                         |  |
|  |       (Amalgamation v3.45 with Write-Ahead Logging WAL)           |  |
|  +-----------------------------+-------------------------------------+  |
+--------------------------------|----------------------------------------+
                                 |
                                 v
                     +-----------------------+
                     |  crewdeck.db (SQLite) |
                     +-----------------------+
```

---

## Key Features

1. **Mission Deck Dashboard (Bento Grid)**: Live headcount metrics, new joiners, active time-off tracker, 14-day attendance stability sparkline, department distribution bars, and recent immutable audit events.
2. **Command Palette (`Ctrl/Cmd + K`)**: Instant keyboard-driven navigation across all modules, quick punch in/out, theme toggling, and live search jumping directly into employee profiles.
3. **Org Constellation**: An interactive HTML5 Canvas organizational network graph. Employees render as orbital nodes clustered around department halos, connected by manager-to-report lines with pan/zoom and click-to-drawer interactions.
4. **Slide-Over Profile Drawer**: Seamless slide-in profile view with optimistic inline updates and 4-second undo toasts.
5. **365-Day Attendance Heatmap**: GitHub-style calendar heatmap displaying daily check-in intensity across 52 weeks with detailed hover tooltips.
6. **Time-Off & Leave Operations**: Dual-track interface for employee leave requests and manager approval workflows with live balance deductions (Annual, Sick, Casual).
7. **Department Payroll Summaries & CSV Export**: Automated CTC aggregation, monthly disbursement tracking, and single-click CSV export.
8. **Security & Cryptography**: Password storage powered by **Argon2id** (`libsodium`), sliding window rate limiting on login attempts, and `HttpOnly; SameSite=Strict` session tokens.
9. **Pre-Seeded Roster**: Boots with **40 realistic Indian-context employees** across 5 departments with full past attendance, leave records, and audit logs.

---

## Tech Stack

- **Backend**: C11, [Mongoose](https://github.com/cesanta/mongoose) (embedded HTTP/WS server vendored), [SQLite3](https://sqlite.org) (amalgamation vendored), [cJSON](https://github.com/DaveGamble/cJSON) (vendored), [libsodium](https://libsodium.org) (Argon2id password hashing).
- **Frontend**: Vanilla HTML5, CSS3, ES6 JavaScript. No bundlers, no external frameworks. Google Fonts: *Space Grotesk*, *Inter*, *JetBrains Mono*.
- **Containers**: Multi-stage `Dockerfile` (GCC builder + debian-slim runtime running as unprivileged user).

---

## Quickstart

### Option 1: Docker (Recommended)

```bash
docker compose up --build
```
Open **http://localhost:8080** in your browser.

### Option 2: Local Compilation

#### On Linux / macOS:
```bash
# Install libsodium development package
sudo apt-get install -y libsodium-dev   # Debian/Ubuntu
brew install libsodium                  # macOS

# Compile and run
make
./crewdeck
```

#### On Windows (with MinGW-w64 / w64devkit):
```powershell
make
.\crewdeck.exe
```

---

## Default Seed Credentials

| Role | Email | Password | Scope |
|------|-------|----------|-------|
| **Admin** | `admin@crewdeck.io` | `admin123` | Full system control, department CRUD, employee delete |
| **Manager** | `rajesh.verma@crewdeck.io` | `manager123` | VP of Engineering: Leave approvals, employee management |
| **Employee** | `aarav.sharma@crewdeck.io` | `employee123` | Principal Architect: Self check-in, leave requests |

*(Quick-fill buttons are provided on the login page for instant one-click testing)*

---

## REST API Reference

All protected endpoints require an active `crew_session` cookie issued by `POST /api/v1/auth/login`.

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| `GET` | `/healthz` | System uptime & database connection check | Public |
| `POST` | `/api/v1/auth/login` | Authenticate with email and password | Public (Rate Limited) |
| `POST` | `/api/v1/auth/logout` | Invalidate session cookie | Public |
| `GET` | `/api/v1/auth/me` | Retrieve currently logged-in user profile | Session |
| `GET` | `/api/v1/dashboard/metrics` | Headcount, joiners, leave, 14-day trend | Session |
| `GET` | `/api/v1/employees` | Paginated employee list (`?search=&dept=&status=&page=&limit=`) | Session |
| `POST` | `/api/v1/employees` | Onboard new employee + initialize leave | Admin / Manager |
| `GET` | `/api/v1/employees/:id` | Full employee profile with leave & attendance | Session |
| `PUT` | `/api/v1/employees/:id` | Update employee record | Admin / Manager |
| `DELETE` | `/api/v1/employees/:id` | Remove employee record | Admin |
| `GET` | `/api/v1/departments` | Department list with headcount & payroll totals | Session |
| `POST` | `/api/v1/departments` | Create new department | Admin |
| `PUT` | `/api/v1/departments/:id` | Update department details | Admin / Manager |
| `DELETE` | `/api/v1/departments/:id` | Delete department (unassigns members) | Admin |
| `GET` | `/api/v1/org/constellation` | Nodes and links for Org Constellation chart | Session |
| `GET` | `/api/v1/attendance` | Daily attendance roster (`?date=YYYY-MM-DD`) | Session |
| `POST` | `/api/v1/attendance/check-in` | Punch-in for today | Session |
| `POST` | `/api/v1/attendance/check-out` | Punch-out for today | Session |
| `GET` | `/api/v1/attendance/heatmap/:id` | 365-day calendar heatmap data | Session |
| `GET` | `/api/v1/leave/requests` | List leave requests (`?status=&employee_id=`) | Session |
| `POST` | `/api/v1/leave/requests` | Submit new time-off request | Session |
| `PUT` | `/api/v1/leave/requests/:id/review` | Approve or reject leave request | Admin / Manager |
| `GET` | `/api/v1/leave/balances/:id` | Leave balances (Annual, Sick, Casual) | Session |
| `GET` | `/api/v1/payroll/summary` | Department compensation breakdown | Admin / Manager |
| `GET` | `/api/v1/payroll/export` | Download full payroll roster as CSV | Admin / Manager |
| `GET` | `/api/v1/audit` | Paginated immutable audit trail | Admin / Manager |

---

## Testing & Quality Assurance

```bash
# Run unit test suite (util, auth, db layers)
make test

# Build with AddressSanitizer (Memory leak & bounds validation)
make asan
```

---

## Deployment Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `8080` | Port on which the HTTP server listens |
| `DB_PATH` | `./data/crewdeck.db` | Path to persistent SQLite database |
| `PUBLIC_DIR` | `./public` | Static frontend directory |
| `ADMIN_EMAIL` | `admin@crewdeck.io` | Seed admin email address |
| `ADMIN_PASSWORD` | `admin123` | Seed admin password (hashed with Argon2id) |

### Render / Fly.io 1-Click Deployment
- `render.yaml` is pre-configured with a persistent volume mounted at `/app/data`.
- `fly.toml` is pre-configured for Fly Machines with a persistent volume `crewdeck_data`.

---

## License
MIT License. Crafted with precision for mission-critical team operations.
