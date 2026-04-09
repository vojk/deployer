import express, { Request, Response } from "express";
import { spawn } from "child_process";
import { mkdtemp, writeFile, rm } from "fs/promises";
import { join } from "path";
import { tmpdir } from "os";

const app = express();

app.use(express.raw({ type: "*/*", limit: "1mb" }));

app.post("/deploy", async (req: Request, res: Response) => {
  const token = process.env.DEPLOY_TOKEN ?? "changeme";
  const workerBin =
    process.env.WORKER_BIN ?? join(__dirname, "../../includes/build/deployer");

  const authHeader = req.headers.authorization;
  if (!authHeader || authHeader !== `Bearer ${token}`) {
    res.status(401).json({ error: "Unauthorized" });
    return;
  }

  const body = req.body as Buffer;
  if (!body || body.length === 0) {
    res.status(400).json({ error: "Empty body" });
    return;
  }

  let tmpDir: string | undefined;

  try {
    tmpDir = await mkdtemp(join(tmpdir(), "deploy-"));
    const yamlPath = join(tmpDir, "flow.yml");
    await writeFile(yamlPath, body);

    res.setHeader("Content-Type", "text/plain; charset=utf-8");
    res.setHeader("Transfer-Encoding", "chunked");
    res.setHeader("X-Content-Type-Options", "nosniff");
    res.flushHeaders();

    const worker = spawn(workerBin, [yamlPath], {
      stdio: ["ignore", "pipe", "pipe"],
    });

    worker.stdout.on("data", (chunk: Buffer) => {
      res.write(chunk);
    });

    worker.stderr.on("data", (chunk: Buffer) => {
      res.write(chunk);
    });

    const cleanupDir = tmpDir;
    worker.on("close", async (code) => {
      if (code !== 0) {
        res.write(`\n[EXIT CODE: ${code}]\n`);
      }
      res.end();

      try {
        await rm(cleanupDir, { recursive: true, force: true });
      } catch {
        // best-effort cleanup
      }
    });

    worker.on("error", async (err) => {
      res.write(`\n[WORKER ERROR: ${err.message}]\n`);
      res.end();

      try {
        await rm(cleanupDir, { recursive: true, force: true });
      } catch {
        // best-effort cleanup
      }
    });
  } catch (err) {
    if (tmpDir) {
      try {
        await rm(tmpDir, { recursive: true, force: true });
      } catch {
        // best-effort cleanup
      }
    }

    if (!res.headersSent) {
      res.status(500).json({ error: "Internal server error" });
    } else {
      res.end();
    }
  }
});

export default app;
