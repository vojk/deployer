# Self-hosted deploy runner

This project is a **self-hosted CI/CD runner** for a single machine: your GitHub Actions workflow sends a YAML file that describes shell steps; a small **gateway** validates the request and forwards the payload to a **C++ worker** over a Unix socket; the worker runs each step and **streams logs back** over HTTP so you see output in the Actions UI as it runs.

Use it when you want deployments or maintenance tasks to execute on **your** server with **your** credentials and paths, without exposing arbitrary shell access—only clients that present a configured API key can trigger a run.

---

## What runs where

```
GitHub Actions  ──POST YAML (chunked response)──▶  Gateway (Express, Docker)
                                                      │
                                                      │ Unix socket
                                                      ▼
                                               Worker (C++, systemd on host)
```

| Piece | Role |
| ----- | ---- |
| **Gateway** (`web-server/`) | HTTP API: checks the API key against a SQLite database, then sends the raw YAML body to the worker socket and streams the worker output back to the client. |
| **Worker** (`includes/`) | Daemon: parses YAML, runs each step with `popen`, streams stdout/stderr to the gateway. Runs as the systemd instance user you choose. |
| **Repository workflow** | On each push (or trigger you define), POSTs `server-flow.yml` to your gateway using a secret token. |

---

## Quick usage (after setup)

Health check (no auth):

```bash
curl -sS "https://your-host.example/"
```

Expect JSON: `{"message":"Healthy"}`.

Deploy or run your defined steps (requires a valid token and `server-flow.yml` in the current directory):

```bash
curl -N -f \
  -X POST \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @server-flow.yml \
  "https://your-host.example/deploy"
```

`-N` disables buffering so streamed log lines appear as they arrive. Replace the URL with your gateway’s public base URL (no trailing slash before `/deploy` in the example above—use your real path).

---

## Requirements

- Linux host with **systemd** for the worker
- **Docker** and **Docker Compose** for the gateway container
- **CMake** ≥ 3.14, **C++17** compiler, and **git** (CMake fetches dependencies for the worker build)
- **Node.js** (matching the gateway toolchain, e.g. 22) if you run gateway tests or Biome locally

---

## Setup

### 1. Build the C++ worker

```bash
cd includes
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

Binary: `includes/build/deployer`. Tests: `includes/build/deployer_tests` or `ctest --test-dir build --output-on-failure`.

### 2. Install the worker on the server

```bash
sudo mkdir -p /opt/deployer
sudo cp includes/build/deployer /opt/deployer/deployer
sudo cp deployer@.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now deployer@MYUSER
```

Replace `MYUSER` with the account the worker should use. Confirm in logs that the listener path matches what the gateway will use (see `WORKER_SOCKET` below).

### 3. Configure the gateway environment

Create a `.env` in the **repository root** (used by Docker Compose):

| Variable | Purpose |
| -------- | ------- |
| `DEPLOY_TOKEN` | Shared secret. On gateway **startup**, this value is stored in the SQLite `keys` table so `POST /deploy` can authorize `Authorization: Bearer …` requests. |
| `PORT` | Host port published to the container (default `3000`). |

Optional overrides inside the gateway container:

| Variable | Purpose |
| -------- | ------- |
| `WORKER_SOCKET` | Path to the worker’s Unix socket. Default in Compose is `/run/deployer/deployer.sock`; align with your systemd unit and socket directory mount. |
| `DEPLOY_DB_PATH` | Path to the SQLite file inside the container (default `deploy.db` in the working directory). Change if you mount a persistent volume for keys. |

### 4. Start the gateway

```bash
docker compose up -d --build
```

The gateway listens on `PORT` and connects to the worker via the configured socket.

### 5. Wire GitHub Actions

In the repo that **deploys this project or your app**, add secrets:

| Secret | Value |
| ------ | ----- |
| `DEPLOY_TOKEN` | Same string as `DEPLOY_TOKEN` in `.env` |
| `DEPLOY_URL` | Base URL of the gateway, e.g. `https://deploy.example.com` |

Copy the example workflow:

```bash
mkdir -p .github/workflows
cp example_workflow/deploy.yml .github/workflows/deploy.yml
```

Adjust triggers and branches as needed.

### 6. Define steps on the server repo

Edit `server-flow.yml` at the root of the repository that Actions checks out—this file is what gets POSTed.

```yaml
steps:
  - name: "Pull latest code"
    run: "cd /home/myuser/app && git pull"
  - name: "Install dependencies"
    run: "cd /home/myuser/app && npm install"
  - name: "Restart service"
    run: "sudo systemctl restart myapp"
```

Optional `vars` for substitution:

```yaml
vars:
  - name: "APP_DIR"
    value: "/home/myuser/app"

steps:
  - name: "Pull latest code"
    run: "cd {{!APP_DIR!}} && git pull"
```

---

## Development and tests

### C++ (Google Test)

```bash
cd includes
cmake -S . -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

### Gateway (Jest)

```bash
cd web-server
npm ci
npm test
```

Tests use a mock Unix socket; no live worker required.

### Gateway lint and format (Biome)

```bash
cd web-server
npm ci
npm run check          # lint + format check (non-destructive)
npm run lint           # lint only
npm run format         # write formatted files
npm run ci             # CI-friendly check (used in GitHub Actions)
```

---

## Project layout

```
.
├── server-flow.yml           # Steps executed when you POST from Actions
├── docker-compose.yml        # Gateway service
├── deployer@.service         # systemd template for the worker
├── example_workflow/
│   └── deploy.yml            # Example GitHub Actions workflow to copy
├── web-server/               # Express gateway (TypeScript)
│   ├── src/
│   │   ├── app.ts
│   │   ├── server.ts
│   │   └── deployDb.ts       # SQLite bootstrap and DEPLOY_TOKEN seeding
│   └── tests/
│       └── app.test.ts
└── includes/                 # C++ worker + parser tests
    ├── CMakeLists.txt
    ├── src/
    └── tests/
```

---

## Operating the worker

```bash
sudo systemctl status deployer@MYUSER
sudo journalctl -u deployer@MYUSER -f
sudo cp includes/build/deployer /opt/deployer/deployer
sudo systemctl restart deployer@MYUSER
```

---

## Continuous integration

This repository includes a GitHub Actions workflow that builds and tests the C++ project and runs gateway tests plus Biome on the `web-server` package. See `.github/workflows/ci.yml`.
