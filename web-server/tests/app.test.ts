import request from "supertest";
import app from "../src/app";
import { createServer, Server, Socket } from "net";
import { unlinkSync, existsSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";
import { mkdtempSync } from "fs";

const MOCK_DIR = mkdtempSync(join(tmpdir(), "deploy-test-sock-"));
const MOCK_SOCKET = join(MOCK_DIR, "test.sock");

let mockServer: Server;

function startMockWorker(
  handler: (data: Buffer, client: Socket) => void
): Promise<void> {
  return new Promise((resolve) => {
    if (existsSync(MOCK_SOCKET)) unlinkSync(MOCK_SOCKET);
    mockServer = createServer((client) => {
      const chunks: Buffer[] = [];
      client.on("data", (chunk: Buffer) => chunks.push(chunk));
      client.on("end", () => handler(Buffer.concat(chunks), client));
    });
    mockServer.listen(MOCK_SOCKET, resolve);
  });
}

function stopMockWorker(): Promise<void> {
  return new Promise((resolve) => {
    if (mockServer) mockServer.close(() => resolve());
    else resolve();
  });
}

beforeAll(() => {
  process.env.DEPLOY_TOKEN = "test-secret-token";
  process.env.WORKER_SOCKET = MOCK_SOCKET;
});

afterEach(async () => {
  await stopMockWorker();
});

describe("POST /deploy", () => {
  describe("authentication", () => {
    it("returns 401 when Authorization header is missing", async () => {
      const res = await request(app)
        .post("/deploy")
        .send("steps:\n  - name: test\n    run: echo hi");

      expect(res.status).toBe(401);
      expect(res.body.error).toBe("Unauthorized");
    });

    it("returns 401 when token is wrong", async () => {
      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer wrong-token")
        .send("steps:\n  - name: test\n    run: echo hi");

      expect(res.status).toBe(401);
      expect(res.body.error).toBe("Unauthorized");
    });

    it("returns 401 when Authorization scheme is not Bearer", async () => {
      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Basic test-secret-token")
        .send("steps:\n  - name: test\n    run: echo hi");

      expect(res.status).toBe(401);
    });
  });

  describe("request validation", () => {
    it("returns 400 when body is empty", async () => {
      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream");

      expect(res.status).toBe(400);
      expect(res.body.error).toBe("Empty body");
    });
  });

  describe("with mock worker socket", () => {
    it("streams output from the worker", async () => {
      await startMockWorker((_data, client) => {
        client.write("=== Step: Greet ===\n");
        client.write("hello from mock\n");
        client.write("step done\n");
        client.end();
      });

      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream")
        .send(Buffer.from("steps:\n  - name: test\n    run: echo hi\n"));

      expect(res.status).toBe(200);
      expect(res.text).toContain("hello from mock");
      expect(res.text).toContain("step done");
    });

    it("forwards the YAML body to the worker", async () => {
      const yamlContent = "steps:\n  - name: check\n    run: echo ok\n";
      let received = "";

      await startMockWorker((data, client) => {
        received = data.toString();
        client.write("ok\n");
        client.end();
      });

      await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream")
        .send(Buffer.from(yamlContent));

      expect(received).toBe(yamlContent);
    });

    it("includes exit code marker when worker reports failure", async () => {
      await startMockWorker((_data, client) => {
        client.write("starting\n");
        client.write("\n[EXIT CODE: 42]\n");
        client.end();
      });

      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream")
        .send(Buffer.from("steps:\n  - name: test\n    run: echo hi\n"));

      expect(res.status).toBe(200);
      expect(res.text).toContain("starting");
      expect(res.text).toContain("[EXIT CODE: 42]");
    });

    it("returns 502 when worker socket is unavailable", async () => {
      process.env.WORKER_SOCKET = "/tmp/nonexistent-deployer-test.sock";

      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream")
        .send(Buffer.from("steps:\n  - name: test\n    run: echo hi\n"));

      expect(res.status).toBe(502);
      expect(res.text).toContain("WORKER ERROR");

      process.env.WORKER_SOCKET = MOCK_SOCKET;
    });
  });
});
