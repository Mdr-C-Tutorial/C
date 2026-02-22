#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";
import process from "node:process";

import { compileSource } from "./src/compiler.mjs";
import { MdrCompileError } from "./src/error.mjs";

const args = process.argv.slice(2);
const options = {
  inputPath: "",
  astOutPath: "",
  dumpTokens: false,
};

if (args.length === 0 || args.includes("--help") || args.includes("-h")) {
  printUsage();
  process.exit(args.length === 0 ? 1 : 0);
}

for (let i = 0; i < args.length; i += 1) {
  const arg = args[i];
  if (arg === "--tokens") {
    options.dumpTokens = true;
    continue;
  }
  if (arg === "--out") {
    const next = args[i + 1];
    if (next == null) {
      console.error("missing output path after --out");
      process.exit(1);
    }
    options.astOutPath = next;
    i += 1;
    continue;
  }
  if (arg.startsWith("-")) {
    console.error(`unknown option: ${arg}`);
    process.exit(1);
  }
  if (options.inputPath !== "") {
    console.error("only one input file is supported");
    process.exit(1);
  }
  options.inputPath = arg;
}

if (options.inputPath === "") {
  console.error("missing input file");
  printUsage();
  process.exit(1);
}

const absoluteInput = path.resolve(options.inputPath);
const source = fs.readFileSync(absoluteInput, "utf8");

try {
  const { tokens, ast } = compileSource(source, absoluteInput);
  const astJson = JSON.stringify(ast, null, 2);
  if (options.astOutPath !== "") {
    const outPath = path.resolve(options.astOutPath);
    fs.mkdirSync(path.dirname(outPath), { recursive: true });
    fs.writeFileSync(outPath, `${astJson}\n`, "utf8");
  } else {
    process.stdout.write(`${astJson}\n`);
  }

  if (options.dumpTokens) {
    process.stdout.write(`${JSON.stringify(tokens, null, 2)}\n`);
  }
} catch (error) {
  if (error instanceof MdrCompileError) {
    printCompileError(error, source);
    process.exit(1);
  }
  throw error;
}

function printUsage() {
  process.stdout.write("Usage: node cli.mjs <input.mdr> [--out <file>] [--tokens]\n");
}

/**
 * @param {MdrCompileError} error
 * @param {string} source
 */
function printCompileError(error, source) {
  const lines = source.split(/\r?\n/u);
  const lineText = lines[error.line - 1] ?? "";
  const pointer = `${" ".repeat(Math.max(error.column - 1, 0))}^`;

  console.error(`${error.filePath}:${error.line}:${error.column}: ${error.message}`);
  console.error(lineText);
  console.error(pointer);
}
