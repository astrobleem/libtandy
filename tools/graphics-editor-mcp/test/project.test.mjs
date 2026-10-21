import assert from "node:assert/strict";
import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import test from "node:test";
import { createFileSandbox } from "../lib/project.mjs";

test("file sandbox bounds reads and refuses paths outside or existing outputs", async (t) => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), "libtandy-mcp-boundary-"));
  t.after(async () => fs.rm(root, { recursive: true, force: true }));
  const sandbox = await createFileSandbox(root);
  await fs.writeFile(path.join(root, "small.bin"), Buffer.from([1, 2, 3, 4, 5]));
  await fs.mkdir(path.join(root, "out"));

  await assert.rejects(sandbox.read("small.bin", 4), /exceeds the 4-byte limit/);
  await assert.rejects(sandbox.read("../small.bin"), /cannot contain/);
  await assert.rejects(sandbox.read("C:\\outside.bin"), /relative to the configured/);

  const written = await sandbox.writeNew("out/asset.bin", Buffer.from([9, 8, 7]));
  assert.deepEqual(await fs.readFile(written), Buffer.from([9, 8, 7]));
  await assert.rejects(sandbox.writeNew("out/asset.bin", Buffer.from([0])), /already exists/);
  assert.deepEqual(await fs.readFile(written), Buffer.from([9, 8, 7]));
});

test("file sandbox refuses directory junctions that escape the configured folder", async (t) => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), "libtandy-mcp-junction-root-"));
  const outside = await fs.mkdtemp(path.join(os.tmpdir(), "libtandy-mcp-junction-outside-"));
  t.after(async () => Promise.all([
    fs.rm(root, { recursive: true, force: true }),
    fs.rm(outside, { recursive: true, force: true })
  ]));
  const junction = path.join(root, "outside-link");
  try { await fs.symlink(outside, junction, "junction"); }
  catch (error) {
    if (["EPERM", "EACCES", "ERROR_PRIVILEGE_NOT_HELD"].includes(error.code)) {
      t.skip("This Windows account cannot create a directory junction.");
      return;
    }
    throw error;
  }
  await fs.writeFile(path.join(outside, "secret.txt"), "outside");
  const sandbox = await createFileSandbox(root);
  await assert.rejects(sandbox.read("outside-link/secret.txt"), /resolves outside/);
  await assert.rejects(sandbox.writeNew("outside-link/new.txt", "blocked"), /resolves outside/);
});
