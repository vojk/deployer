import { createConnection } from "node:net";
import express, { type Request, type Response } from "express";
import { open } from "sqlite";
import sqlite3 from "sqlite3";
import { deployDbPath, hashDeployToken } from "./deployDb";

const app = express();

app.use(express.raw({ type: "*/*", limit: "1mb" }));

app.get("/*", async (_req: Request, res: Response) => {
	res.status(200).json({ message: "Healthy" });
});

app.post("/deploy", async (req: Request, res: Response) => {
	const socketPath = process.env.WORKER_SOCKET ?? "/var/run/deployer.sock";

	const authHeader = req.headers.authorization;
	if (!authHeader) {
		res.status(401).json({ error: "Unauthorized" });
		return;
	}

	const parsedAuth = authHeader.match(/^Bearer\s+(\S+)$/);
	if (!parsedAuth) {
		res.status(401).json({ error: "Unauthorized" });
		return;
	}
	const token = parsedAuth[1];

	const db = await open({
		filename: deployDbPath(),
		driver: sqlite3.Database,
	});

	try {
		const row = await db.get("SELECT id FROM keys WHERE token_hash = ?", [
			hashDeployToken(token),
		]);
		if (!row) {
			res.status(401).json({ error: "Unauthorized" });
			return;
		}
	} finally {
		await db.close();
	}

	const body = req.body as Buffer;
	if (!body || body.length === 0) {
		res.status(400).json({ error: "Empty body" });
		return;
	}

	const sock = createConnection({ path: socketPath, allowHalfOpen: true });

	sock.on("connect", () => {
		res.setHeader("Content-Type", "text/plain; charset=utf-8");
		res.setHeader("Transfer-Encoding", "chunked");
		res.setHeader("X-Content-Type-Options", "nosniff");
		res.flushHeaders();

		const frame = Buffer.alloc(4 + body.length);
		frame.writeUInt32BE(body.length, 0);
		body.copy(frame, 4);
		sock.end(frame);
	});

	sock.on("data", (chunk: Buffer) => {
		res.write(chunk);
	});

	sock.on("end", () => {
		sock.end();
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
