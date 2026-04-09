# Custom CI/CD Runner

Self-hosted CI/CD runner that receives a YAML workflow from GitHub Actions, executes the steps on the host machine, and streams logs back in real time.

## Architecture

```
GitHub Action ──POST YAML──▶ Express gateway (Docker) ──Unix socket──▶ C++ worker (host)
               ◀─chunked HTTP─                         ◀──stream logs──
```

- **Gateway** -- Node.js/Express server running in a Docker container. Authenticates requests via Bearer token and forwards the YAML to the worker over a Unix socket.
- **Worker** -- C++ daemon running natively on the host under a specific user. Parses the YAML, executes each step via `popen`, and streams output back through the socket. Commands run with the permissions of the configured system user.
- **Client** -- A GitHub Actions workflow that sends `server-flow.yml` to the server on every push to `main`.

## Prerequisites

- Linux server with systemd
- Docker and Docker Compose
- CMake >= 3.14, a C++17 compiler, and git (for building the worker)
- Node.js >= 18 (only needed if running tests locally)

## Setup

### 1. Build the C++ worker

```bash
cd includes
cmake -S . -B build
cmake --build build -j$(nproc)
```

This produces `includes/build/deployer`.

### 2. Install the worker on the server

Copy the binary:

```bash
sudo mkdir -p /opt/deployer
sudo cp includes/build/deployer /opt/deployer/deployer
```

Install the systemd service (template unit -- the instance name is the user the worker runs as):

```bash
sudo cp deployer@.service /etc/systemd/system/
sudo systemctl daemon-reload
```

Start the worker as your desired user (replace `myuser` with the actual username):

```bash
sudo systemctl enable --now deployer@myuser
```

Verify it is running:

```bash
sudo systemctl status deployer@myuser
```

You should see `Deployer listening on /run/deployer/deployer.sock` in the log output.

### 3. Configure the gateway

Create a `.env` file in the project root:

```bash
DEPLOY_TOKEN=your-secret-token-here
PORT=3000
```

### 4. Start the gateway

```bash
docker compose up -d --build
```

The Express server will start on port 3000 (or whatever `PORT` is set to) and communicate with the worker via `/run/deployer/deployer.sock`.

### 5. Configure GitHub repository

Add the following secrets in your GitHub repository settings (Settings > Secrets and variables > Actions):


| Secret         | Value                                              |
| -------------- | -------------------------------------------------- |
| `DEPLOY_TOKEN` | Same token as in `.env`                            |
| `DEPLOY_URL`   | Your server URL, e.g. `https://deploy.example.com` |


Copy the example workflow into your repository:

```bash
mkdir -p .github/workflows
cp example/workflows/deploy.yml .github/workflows/deploy.yml
```

### 6. Define your workflow

Edit `server-flow.yml` in the repository root:

```yaml
steps:
  - name: "Pull latest code"
    run: "cd /home/myuser/app && git pull"
  - name: "Install dependencies"
    run: "cd /home/myuser/app && npm install"
  - name: "Restart service"
    run: "sudo systemctl restart myapp"
```

The `vars` section is also supported for reusable values:

```yaml
vars:
  - name: "APP_DIR"
    value: "/home/myuser/app"

steps:
  - name: "Pull latest code"
    run: "cd {{!APP_DIR!}} && git pull"
```

Push to `main` and the workflow will execute on your server with logs streamed back to the GitHub Actions console.

## Running Tests

### C++ parser tests (Google Test)

```bash
cd includes
cmake -S . -B build
cmake --build build -j$(nproc)
./build/deployer_tests
```

### Express gateway tests (Jest + Supertest)

```bash
cd web-server
npm install
npm test
```

The gateway tests use a mock Unix socket server -- no running C++ worker is needed.

## Project Structure

```
.
├── server-flow.yml              # Workflow definition (steps to execute)
├── docker-compose.yml           # Gateway container configuration
├── deployer@.service            # systemd template unit for the worker
├── .env                         # DEPLOY_TOKEN, PORT (not in git)
├── example/
│   └── workflows/
│       └── deploy.yml           # GitHub Actions workflow to copy into your repo
├── web-server/                  # Express gateway
│   ├── Dockerfile
│   ├── package.json
│   ├── tsconfig.json
│   ├── jest.config.ts
│   ├── src/
│   │   ├── app.ts               # Express app (auth, socket forwarding)
│   │   └── server.ts            # Entry point
│   └── tests/
│       └── app.test.ts          # 8 tests (auth, streaming, errors)
└── includes/                    # C++ worker
    ├── CMakeLists.txt
    ├── src/
    │   ├── main.cpp             # Unix socket server daemon
    │   ├── parser.h             # YAML parsing (testable interface)
    │   └── parser.cpp
    └── tests/
        └── parser_test.cpp      # 7 tests (parsing logic)
```

## Managing the Worker

```bash
# Check status
sudo systemctl status deployer@myuser

# View logs
sudo journalctl -u deployer@myuser -f

# Restart after rebuilding
sudo cp includes/build/deployer /opt/deployer/deployer
sudo systemctl restart deployer@myuser
```

