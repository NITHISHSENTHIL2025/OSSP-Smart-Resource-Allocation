# BANKER OS
### Smart Resource Allocation Using Banker's Algorithm

**OSSP Hackathon 2026 | Problem 13 | Group G-13 | Section S7 | KLH Hyderabad**

**Team:** Nithish S (2520030346), Sai Sathwik (2520030328), Ch. Vivek (2520030532).

## Why this version is different

This is a **user-input-driven resource manager**. The application does **not** load fixed allocation matrices or automatically run sample values. When you open it for the first time you see blank input fields. Enter the process count, resource count, Available, Allocation, and Maximum. You can then repeatedly request/release resources and inspect the resulting state. A sample JSON file is supplied **only** for optional tests or faculty demonstration and is never loaded automatically.

A modern browser dashboard calls a **compiled C Banker engine**, which executes the safety algorithm, grants safe allocations and rolls back unsafe requests. It is a **resource-allocation simulator**, not an actual Linux kernel modification.

## Fast start: Ubuntu or Ubuntu on WSL

### 1. Extract and open a terminal in this project directory

```bash
cd ~/OSSP_BANKER_FINAL_MEGA
```

If you unzipped to Downloads, instead use `cd ~/Downloads/OSSP_BANKER_FINAL_MEGA`.

### 2. Run

```bash
make
make web
```

Open **http://localhost:8765** in your Windows or Ubuntu browser. Keep the Ubuntu Terminal open while you use the dashboard. To stop the local server, press `Ctrl+C`.

**Alternative one-command launcher:**

```bash
bash START_WEB.sh
```

### 3. What to enter

- Workspace name: any name you choose
- Processes: between 1 and 20
- Resource types: between 1 and 10
- Available vector: free resources (non-negative integers)
- Allocation matrix: resources each process holds
- Maximum matrix: largest number each process may ever need

**Required:** `Allocation[i][j] <= Maximum[i][j]`, and `Maximum[i][j] <= Total resources[j]`.

Click **Initialize workspace**. The dashboard calculates **Need = Maximum - Allocation** and displays a safe sequence, if any.

### 4. Keep working in the same session

- Request resources: select process, enter vector, click **Check & request**. The C engine approves or denies and preserves the old state on unsafe requests.
- Release resources: select process, enter vector, click **Release resources**.
- Inspect the timeline to see the decisions.
- **Export workspace** saves your current resource matrices as JSON; **Import workspace** loads your saved JSON.
- User-created state is also saved locally as `runtime/session.json` and is restored when you restart `make web`.
- **New workspace** clears current state after confirmation.

## Terminal fallback

```bash
make
./banker
```

Choose **1 - Enter MY OWN inputs**, not the optional example. The terminal menu also supports safe checks, requests, releases, save/load and history. It is a separate interactive console state from the browser session.

## Automated verification

```bash
make test
```

Expected: **Ran 21 tests ... OK** (core regression suite). A human can also use the optional faculty verification dataset, `examples/OPTIONAL_FACULTY_TEST_INPUT.json`, which does not load unless explicitly imported.

## Two faculty demonstration scenarios

After entering the 5-process test dataset or importing the optional test JSON:

**Safe request:** P1 `[1, 0, 2]` -> **GRANTED**, resulting Available `[2, 3, 0]`.

**Unsafe request:** From the **original** state, P4 `[3, 3, 0]` -> **DENIED**, Available stays `[3, 3, 2]`. Restore the original dataset between independent tests by exporting before your first request and importing it again later.

The initial safe sequence is **P1 -> P3 -> P4 -> P0 -> P2**.

## Technology stack

- `src/bankers_core.c` + `src/bankers_core.h`: C11 resource validation, safety checker, requests and rollback
- `web/server.py`: localhost-only Python standard-library HTTP API; uses C shared library via `ctypes`
- `web/index.html`, `style.css`, `app.js`: responsive offline browser UI
- `src/bankers_cli.c`: Linux terminal mode
- `tests/test_core.py`: 21 unittest methods (including 50 randomized test states)
- `deliverables/`: project report PDF and DOCX, 16-slide PPT and slide PDF, live-demo guide
- `examples/`: optional reference-only data for validation (not automatically used)

The browser app itself does not need Flask, Node.js, npm, or an internet connection. Only **GCC, make and Python 3** are required on the demonstration machine.

## Setup troubleshooting

| Problem | Fix |
|---|---|
| `make: command not found` / `gcc: command not found` | `sudo apt update && sudo apt install build-essential python3` |
| `Address already in use` | Another copy of Banker OS may already be running. Stop it with Ctrl+C. |
| Browser cannot reach localhost | Keep `make web` running, open `http://localhost:8765` (not HTTPS), try the Ubuntu browser if WSL forwarding is disabled. |
| Invalid dataset | Check every cell is a non-negative integer, Allocation <= Maximum, Maximum <= Total. |
| Request denied | Check the explanation: exceeds Need, exceeds Available or unsafe after safety test. |
| `./banker --demo` loads examples | Use `./banker` for custom terminal input; use `make web` for the polished browser dashboard. |

## Academic note

The application models an operating system resource allocation problem, not operating-system kernel internals. The example dataset illustrates one safe sequence only; alternative safe sequences may exist. The source code is intended for understanding, live demonstration, and original team presentation.
