import express, { Request, Response } from "express";
import { createConnection } from "net";

const app = express();

app.use(express.raw({ type: "*/*", limit: "1mb" }));

app.get("/*", async (req: Request, res: Response) => {
  res.status(200).json({ message: "Healthy" });
});

app.post("/deploy", async (req: Request, res: Response) => {
  console.log("deploy");
  const token = process.env.DEPLOY_TOKEN ?? "changeme";
  const socketPath = process.env.WORKER_SOCKET ?? "/var/run/deployer.sock";

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

  const sock = createConnection(socketPath);

  sock.on("connect", () => {
    res.setHeader("Content-Type", "text/plain; charset=utf-8");
    res.setHeader("Transfer-Encoding", "chunked");
    res.setHeader("X-Content-Type-Options", "nosniff");
    res.flushHeaders();

    sock.end(body);
  });

  sock.on("data", (chunk: Buffer) => {
    res.write(chunk);
  });

  sock.on("end", () => {
    res.end();
  });

  sock.on("error", (err) => {
    if (!res.headersSent) {
      res.status(502).send(`[WORKER ERROR: ${err.message}]\n`);
    } else {
      res.write(`\n[WORKER ERROR: ${err.message}]\n`);
      res.end();
    }
  });
});

export default app;
