import { bootstrapDeployDatabase, insertDeployKey } from "./deployDb";

async function run(): Promise<void> {
	const token = process.argv[2];

	if (!token) {
		console.error("Usage: npm run add-key -- <deploy-token>");
		process.exitCode = 1;
		return;
	}

	await bootstrapDeployDatabase();
	await insertDeployKey(token);

	console.log("Deploy key stored in database.");
}

run().catch((error: Error) => {
	console.error(`Failed to store deploy key: ${error.message}`);
	process.exitCode = 1;
});
