// 差分测试：同一批输入分别交给 JS 前端与 C 前端，比较两者接受还是拒绝。
//
// 用法：
//   node code/项目/Mdr_Lang/tests/diff_frontends.mjs <mdrcc 路径> [用例目录...]
//
// 目录按名字解释，约定与 cases/ 下的子目录一致：
//   pass、run      两个前端都应接受
//   syntax         两个前端都应拒绝（都只到语法阶段）
//   fail            C 拒绝、JS 接受：JS 不做语义检查，差异是预期的
//   divergence      两个前端的语法规则本身不同，差异同样是预期的
// 其余组合都算意外，脚本以非零状态退出。

import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import process from "node:process";

import { compileSource } from "../compiler/src/compiler.mjs";
import { MdrCompileError } from "../compiler/src/error.mjs";

const args = process.argv.slice(2);
const mdrcc = args[0];
const dirs = args.length > 1 ? args.slice(1) : ["cases"];
if (mdrcc === undefined) {
  console.error("usage: node diff_frontends.mjs <mdrcc> [case-dir...]");
  process.exit(2);
}

function collect(dir) {
  const files = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...collect(full));
    } else if (entry.name.endsWith(".mdr")) {
      files.push(full);
    }
  }
  return files;
}

function cFrontend(file) {
  const result = spawnSync(mdrcc, [file], { encoding: "utf8" });
  const first = (result.stderr || "").trim().split(/\r?\n/u)[0] ?? "";
  return { accepted: result.status === 0, message: first };
}

function jsFrontend(file) {
  const source = fs.readFileSync(file, "utf8");
  try {
    compileSource(source, file);
    return { accepted: true, message: "" };
  } catch (error) {
    if (error instanceof MdrCompileError) {
      return { accepted: false, message: `${error.line}:${error.column}: ${error.message}` };
    }
    throw error;
  }
}

const summary = new Map();
const unexpected = [];

for (const file of dirs.flatMap(collect).sort()) {
  const folder = path.basename(path.dirname(file));
  const c = cFrontend(file);
  const js = jsFrontend(file);
  let verdict;
  if (c.accepted && js.accepted) {
    verdict = "都接受";
  } else if (!c.accepted && !js.accepted) {
    verdict = "都拒绝";
  } else if (folder === "fail" || folder === "divergence") {
    verdict = c.accepted ? "意外：C 接受" : "C 拒绝，JS 接受";
    if (c.accepted) {
      unexpected.push({ file, c, js });
    }
  } else {
    verdict = `意外：C ${c.accepted ? "接受" : "拒绝"}，JS ${js.accepted ? "接受" : "拒绝"}`;
    unexpected.push({ file, c, js });
  }

  if (!summary.has(folder)) {
    summary.set(folder, new Map());
  }
  const bucket = summary.get(folder);
  bucket.set(verdict, (bucket.get(verdict) ?? 0) + 1);
}

for (const folder of [...summary.keys()].sort()) {
  const parts = [...summary.get(folder).entries()].map(([verdict, count]) => `${verdict} ${count}`);
  console.log(`${folder.padEnd(12)} ${parts.join("，")}`);
}

for (const item of unexpected) {
  console.log(`DIFF ${item.file}`);
  console.log(`     C  ${item.c.accepted ? "接受" : "拒绝"}: ${item.c.message}`);
  console.log(`     JS ${item.js.accepted ? "接受" : "拒绝"}: ${item.js.message}`);
}

const total = [...summary.values()].reduce(
  (sum, bucket) => sum + [...bucket.values()].reduce((a, b) => a + b, 0),
  0,
);
console.log(`${total} files, ${unexpected.length} unexpected`);
process.exit(unexpected.length === 0 ? 0 : 1);