import assert from "node:assert/strict";
import fs from "node:fs/promises";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { compileSource } from "../src/compiler.mjs";

const thisFilePath = fileURLToPath(import.meta.url);
const testsDir = path.dirname(thisFilePath);
const repoRoot = path.resolve(testsDir, "../../../../..");
const examplesDir = path.join(repoRoot, "docs", "教程", "正文", "项目", "Mdr_Lang", "example");

test("all mdr examples can be parsed", async () => {
  const entries = await fs.readdir(examplesDir, { withFileTypes: true });
  const files = entries
    .filter((entry) => entry.isFile() && entry.name.endsWith(".mdr"))
    .map((entry) => path.join(examplesDir, entry.name))
    .sort((a, b) => a.localeCompare(b));

  assert.ok(files.length > 0, "no .mdr files found");

  for (const file of files) {
    const source = await fs.readFile(file, "utf8");
    const { ast } = compileSource(source, file);
    assert.equal(ast.kind, "program");
    assert.ok(Array.isArray(ast.body));
  }
});
