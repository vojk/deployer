import app from "./app";
import { bootstrapDeployDatabase } from "./deployDb";

const PORT = parseInt(process.env.PORT ?? "3000", 10);

(async () => {
	await bootstrapDeployDatabase();

	app.listen(PORT, () => {
		console.log(`Deploy server listening on port ${PORT}`);
	});
})();
