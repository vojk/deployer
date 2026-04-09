import request from "supertest";
import app from "../src/app";
import { existsSync, mkdirSync, writeFileSync, chmodSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";
import { mkdtempSync } from "fs";

const MOCK_DIR = mkdtempSync(join(tmpdir(), "deploy-test-mock-"));
const MOCK_WORKER = join(MOCK_DIR, "mock-worker.sh");
const MOCK_WORKER_FAIL = join(MOCK_DIR, "mock-worker-fail.sh");

beforeAll(() => {
  writeFileSync(
    MOCK_WORKER,
    '#!/bin/bash\necho "hello from mock"\necho "step done"\n'
  );
  chmodSync(MOCK_WORKER, "755");

  writeFileSync(
    MOCK_WORKER_FAIL,
    '#!/bin/bash\necho "starting"\nexit 42\n'
  );
  chmodSync(MOCK_WORKER_FAIL, "755");

  process.env.DEPLOY_TOKEN = "test-secret-token";
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

  describe("with valid auth and mock worker", () => {
    beforeAll(() => {
      process.env.WORKER_BIN = MOCK_WORKER;
    });

    it("returns 400 when body is empty", async () => {
      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream");

      expect(res.status).toBe(400);
      expect(res.body.error).toBe("Empty body");
    });

    it("streams output from the worker", async () => {
      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream")
        .send(Buffer.from("steps:\n  - name: test\n    run: echo hi\n"));

      expect(res.status).toBe(200);
      expect(res.text).toContain("hello from mock");
      expect(res.text).toContain("step done");
    });

    it("cleans up the temp directory after completion", async () => {
      // We need to intercept the tmpDir path. We'll verify by checking
      // that no deploy-* dirs remain beyond what existed before.
      const osTmpDir = tmpdir();
      const beforeDirs = new Set(
        require("fs")
          .readdirSync(osTmpDir)
          .filter((d: string) => d.startsWith("deploy-"))
      );

      await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream")
        .send(Buffer.from("some yaml content\n"));

      // Small delay to let async cleanup finish
      await new Promise((r) => setTimeout(r, 200));

      const afterDirs = require("fs")
        .readdirSync(osTmpDir)
        .filter(
          (d: string) => d.startsWith("deploy-") && !beforeDirs.has(d)
        );

      expect(afterDirs.length).toBe(0);
    });
  });

  describe("worker failure", () => {
    beforeAll(() => {
      process.env.WORKER_BIN = MOCK_WORKER_FAIL;
    });

    it("includes exit code marker when worker exits non-zero", async () => {
      const res = await request(app)
        .post("/deploy")
        .set("Authorization", "Bearer test-secret-token")
        .set("Content-Type", "application/octet-stream")
        .send(Buffer.from("steps:\n  - name: test\n    run: echo hi\n"));

      expect(res.status).toBe(200);
      expect(res.text).toContain("starting");
      expect(res.text).toContain("[EXIT CODE: 42]");
    });
  });
});
