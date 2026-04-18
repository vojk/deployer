import { open } from "sqlite";
import sqlite3 from "sqlite3";

export function deployDbPath(): string {
	return process.env.DEPLOY_DB_PATH ?? "deploy.db";
}

export async function bootstrapDeployDatabase(): Promise<void> {
	const db = await open({
		filename: deployDbPath(),
		driver: sqlite3.Database,
	});

	await db.run(
		"CREATE TABLE IF NOT EXISTS keys (id INTEGER PRIMARY KEY AUTOINCREMENT, key TEXT, created_at TEXT)",
	);

	const token = process.env.DEPLOY_TOKEN;
	if (token) {
		await db.run("DELETE FROM keys WHERE key = ?", [token]);
		await db.run(
			"INSERT INTO keys (key, created_at) VALUES (?, datetime('now'))",
			[token],
		);
	}

	await db.close();
}
