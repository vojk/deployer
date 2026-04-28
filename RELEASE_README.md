# Deploy Runner Release Quickstart

This release package contains everything needed to run the deploy runner stack:
- `deployer` (prebuilt C++ worker binary)
- `deployer@.service` (systemd unit template)
- `docker-compose.yml`, `.dockerignore`
- `web-server/` (gateway source)
- `example_workflow/` (example GitHub Actions workflow)
- `server-flow.yml` (example executed steps)

## 1) Download and extract

From GitHub Release assets, download either:
- `deploy-release-<tag>.tar.gz`
- `deploy-release-<tag>.zip`

Extract:

```bash
tar -xzf deploy-release-<tag>.tar.gz
cd deploy-release-<tag>
```

Or:

```bash
unzip deploy-release-<tag>.zip
cd deploy-release-<tag>
```

## 2) Install worker service

```bash
sudo mkdir -p /opt/deployer
sudo cp deployer /opt/deployer/deployer
sudo chmod +x /opt/deployer/deployer
sudo cp deployer@.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now deployer@MYUSER
```

Replace `MYUSER` with the Linux user that should run the worker.

## 3) Start gateway

From the extracted release directory:

```bash
docker compose up -d --build
```

## 4) Add deploy API key

```bash
docker compose exec gateway npm run add-key -- "YOUR_DEPLOY_TOKEN"
```

## 5) Trigger deploy

Use your workflow (or `example_workflow/deploy.yml`) to POST `server-flow.yml` to `/deploy` with bearer token auth.

For full details and troubleshooting, see `README.md`.
