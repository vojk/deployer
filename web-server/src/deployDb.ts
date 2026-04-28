import { createHash } from "node:crypto";
import { open } from "sqlite";
import sqlite3 from "sqlite3";

export function deployDbPath(): string {
	return process.env.DEPLOY_DB_PATH ?? "deploy.db";
}

export function hashDeployToken(token: string): string {
	return createHash("sha256").update(token, "utf8").digest("hex");
}

async function ensureKeysTableSchema(): Promise<void> {
	const db = await open({
		filename: deployDbPath(),
		driver: sqlite3.Database,
	});

	await db.run(
		"CREATE TABLE IF NOT EXISTS keys (id INTEGER PRIMARY KEY AUTOINCREMENT, token_hash TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL)",
	);

	const tableInfo = (await db.all("PRAGMA table_info(keys)")) as Array<{
		name: string;
	}>;

	const hasLegacyKeyColumn = tableInfo.some((column) => column.name === "key");
	const hasTokenHashColumn = tableInfo.some(
		(column) => column.name === "token_hash",
	);

	if (hasLegacyKeyColumn && !hasTokenHashColumn) {
		const legacyRows = (await db.all(
			"SELECT key, COALESCE(created_at, datetime('now')) AS created_at FROM keys WHERE key IS NOT NULL",
		)) as Array<{ key: string; created_at: string }>;

		await db.exec("BEGIN TRANSACTION");
		try {
			await db.run("ALTER TABLE keys RENAME TO keys_legacy");
			await db.run(
				"CREATE TABLE keys (id INTEGER PRIMARY KEY AUTOINCREMENT, token_hash TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL)",
			);

			for (const row of legacyRows) {
				await db.run(
					"INSERT OR IGNORE INTO keys (token_hash, created_at) VALUES (?, ?)",
					[hashDeployToken(row.key), row.created_at],
				);
			}

			await db.run("DROP TABLE keys_legacy");
			await db.exec("COMMIT");
		} catch (error) {
			await db.exec("ROLLBACK");
			throw error;
		}
	}

	await db.close();
}

export async function bootstrapDeployDatabase(): Promise<void> {
	await ensureKeysTableSchema();
}

export async function insertDeployKey(token: string): Promise<void> {
	const normalizedToken = token.trim();
	if (!normalizedToken) {
		throw new Error("Token must be non-empty");
	}

	const db = await open({
		filename: deployDbPath(),
		driver: sqlite3.Database,
	});

	try {
		await db.run(
			"INSERT OR IGNORE INTO keys (token_hash, created_at) VALUES (?, datetime('now'))",
			[hashDeployToken(normalizedToken)],
		);
	} finally {
		await db.close();
	}
}
