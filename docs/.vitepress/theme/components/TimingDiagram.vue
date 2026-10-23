<script setup lang="ts">
import { computed } from "vue";

type Kind =
  "low" | "high" | "pu" | "pd" | "clkP" | "clkN" | "x" | "z" | "data" | "none";

interface Segment {
  key: string;
  kind: Kind;
  start: number;
  len: number;
  label: string;
  tone: string;
  edge: boolean;
}

interface SignalRow {
  key: string;
  type: "signal" | "spacer" | "group";
  row: number;
  name: string;
  segments: Segment[];
  gaps: number[];
}

interface Mark {
  key: string;
  at: number;
  label: string;
}

const props = defineProps({
  // 信号数组：{ name, wave, data }；也接受 WaveDrom 的 { signal: [...] } 对象或 JSON 字符串
  signals: { type: [Array, Object, String], required: true },
  title: { type: String, default: "" },
  // 每个格子的最大宽度倍数（默认 1，即 36px；格子多时自动收窄，最窄 12px）
  hscale: { type: [Number, String], default: 1 },
  // 在格子边界上标周期编号；true 从 0 开始，数字表示起始编号
  ticks: { type: [Boolean, Number, String], default: false },
  // 竖直虚线标记：数字或 { at, label }，at 以格子为单位（2.5 表示第 3 格中间）
  marks: { type: Array, default: () => [] },
});

const TONES: Record<string, string> = {
  "3": "yellow",
  "4": "red",
  "5": "brand",
  "6": "green",
  "7": "purple",
  "8": "gray",
  "9": "yellow",
};

function kindOf(ch: string): Kind | null {
  if (ch === "0" || ch === "l" || ch === "L") return "low";
  if (ch === "1" || ch === "h" || ch === "H") return "high";
  if (ch === "u") return "pu";
  if (ch === "d") return "pd";
  if (ch === "p" || ch === "P") return "clkP";
  if (ch === "n" || ch === "N") return "clkN";
  if (ch === "x" || ch === "X") return "x";
  if (ch === "z" || ch === "Z") return "z";
  if (ch === "=" || /^[2-9]$/.test(ch)) return "data";
  return null;
}

function endLevel(kind: Kind): "hi" | "lo" | null {
  if (kind === "high" || kind === "pu" || kind === "clkN") return "hi";
  if (kind === "low" || kind === "pd" || kind === "clkP") return "lo";
  return null;
}

function startLevel(kind: Kind): "hi" | "lo" | null {
  if (kind === "high" || kind === "pu") return "hi";
  if (kind === "low" || kind === "pd") return "lo";
  return null;
}

function toList(value: unknown): string[] {
  if (Array.isArray(value))
    return value.map((item) => (item === null ? "" : String(item)));
  if (typeof value === "string") return value.split(/\s+/).filter(Boolean);
  if (typeof value === "number") return [String(value)];
  return [];
}

function parseWave(
  wave: string,
  data: string[],
  length: number,
  rowKey: string,
  problems: string[],
  name: string,
) {
  const segments: Segment[] = [];
  const gaps: number[] = [];
  let dataIndex = 0;
  const bad = new Set<string>();
  for (let index = 0; index < length; index += 1) {
    let ch = wave[index] ?? ".";
    if (ch === "|") {
      gaps.push(index);
      ch = ".";
    }
    const last = segments[segments.length - 1];
    if (ch === ".") {
      if (last) last.len += 1;
      else segments.push(newSegment("none", index));
      continue;
    }
    let kind = kindOf(ch);
    if (kind === null) {
      bad.add(ch);
      kind = "x";
    }
    if (last && kind !== "data" && last.kind === kind) {
      last.len += 1;
      continue;
    }
    const segment = newSegment(kind, index);
    if (kind === "data") {
      segment.label = data[dataIndex] ?? "";
      segment.tone = TONES[ch] ?? "";
      dataIndex += 1;
    }
    const before = last ? endLevel(last.kind) : null;
    const after = startLevel(kind);
    segment.edge = before !== null && after !== null && before !== after;
    segments.push(segment);
  }
  if (bad.size > 0) {
    problems.push(`信号「${name}」含不支持的字符 ${[...bad].join("")}，按 x 绘制。`);
  }
  return { segments, gaps };

  function newSegment(kind: Kind, start: number): Segment {
    return {
      key: `${rowKey}-${start}`,
      kind,
      start,
      len: 1,
      label: "",
      tone: "",
      edge: false,
    };
  }
}

const model = computed(() => {
  const problems: string[] = [];
  let input: unknown = props.signals;
  if (typeof input === "string") {
    try {
      input = JSON.parse(input);
    } catch {
      problems.push("signals 不是合法的 JSON。");
      input = [];
    }
  }
  if (input && !Array.isArray(input) && typeof input === "object") {
    input = (input as { signal?: unknown }).signal;
  }
  if (!Array.isArray(input)) {
    problems.push("signals 必须是数组。");
    input = [];
  }

  // 展开 WaveDrom 的分组 ["组名", {...}, {...}]
  type Entry = {
    type: "signal" | "spacer" | "group";
    name: string;
    wave: string;
    data: string[];
  };
  const entries: Entry[] = [];
  const visit = (list: unknown[]) => {
    for (const item of list) {
      if (Array.isArray(item)) {
        const [head, ...rest] = item;
        if (typeof head === "string") {
          entries.push({ type: "group", name: head, wave: "", data: [] });
          visit(rest);
        } else {
          visit(item);
        }
      } else if (item && typeof item === "object") {
        const record = item as Record<string, unknown>;
        if (typeof record.wave === "string") {
          entries.push({
            type: "signal",
            name: record.name === undefined ? "" : String(record.name),
            wave: record.wave,
            data: toList(record.data),
          });
        } else {
          entries.push({ type: "spacer", name: "", wave: "", data: [] });
        }
      }
    }
  };
  visit(input as unknown[]);
  if (!entries.some((entry) => entry.type === "signal"))
    problems.push("没有可绘制的信号。");

  const length = Math.max(1, ...entries.map((entry) => entry.wave.length));
  const firstRow = props.ticks === false ? 1 : 2;
  const rows: SignalRow[] = entries.map((entry, index) => {
    const key = `s${index}`;
    const parsed =
      entry.type === "signal"
        ? parseWave(
            entry.wave,
            entry.data,
            length,
            key,
            problems,
            entry.name || String(index + 1),
          )
        : { segments: [], gaps: [] };
    return {
      key,
      type: entry.type,
      row: firstRow + index,
      name: entry.name,
      ...parsed,
    };
  });

  const tickStart =
    props.ticks === true || props.ticks === ""
      ? 0
      : props.ticks === false
        ? null
        : Number.isFinite(Number(props.ticks))
          ? Number(props.ticks)
          : 0;
  const ticks =
    tickStart === null
      ? []
      : Array.from({ length: length + 1 }, (_, index) => ({
          key: `t${index}`,
          column: Math.min(index, length - 1) + 2,
          end: index === length,
          text: String(tickStart + index),
        }));

  const marks: Mark[] = [];
  (props.marks as unknown[]).forEach((item, index) => {
    const record =
      item && typeof item === "object" ? (item as Record<string, unknown>) : null;
    const at = Number(record ? record.at : item);
    if (!Number.isFinite(at) || at < 0 || at > length) {
      problems.push(`第 ${index + 1} 个 mark 的位置无效。`);
      return;
    }
    marks.push({
      key: `m${index}`,
      at,
      label: record?.label === undefined ? "" : String(record.label),
    });
  });

  const hscale = Number(props.hscale);
  const scale = Number.isFinite(hscale) && hscale > 0 ? Math.min(hscale, 4) : 1;
  const lastRow = firstRow + rows.length;
  return {
    rows,
    ticks,
    marks: marks.map((mark) => {
      const cell = Math.min(Math.floor(mark.at), length - 1);
      return {
        ...mark,
        column: cell + 2,
        offset: `${(mark.at - cell) * 100}%`,
      };
    }),
    markLabels: marks.some((mark) => mark.label),
    length,
    firstRow,
    lastRow,
    style: { "--td-n": length, "--td-scale": scale },
    problems,
  };
});
</script>

<template>
  <figure class="timing-diagram">
    <figcaption
      v-if="title"
      class="td-title"
    >
      {{ title }}
    </figcaption>

    <div class="td-scroll">
      <div
        class="td-grid"
        :style="model.style"
      >
        <template v-if="model.ticks.length">
          <span
            v-for="tick in model.ticks"
            :key="tick.key"
            class="td-tick"
            :class="{ 'is-end': tick.end }"
            :style="{ gridRow: 1, gridColumn: tick.column }"
            aria-hidden="true"
            >{{ tick.text }}</span
          >
        </template>

        <template
          v-for="row in model.rows"
          :key="row.key"
        >
          <span
            class="td-name"
            :class="`is-${row.type}`"
            :style="{ gridRow: row.row }"
            >{{ row.name }}</span
          >
          <span
            v-for="segment in row.segments"
            :key="segment.key"
            class="td-seg"
            :class="[
              `kind-${segment.kind}`,
              { 'has-edge': segment.edge },
              segment.tone ? `tone-${segment.tone}` : '',
            ]"
            :style="{
              gridRow: row.row,
              gridColumn: `${segment.start + 2} / span ${segment.len}`,
              '--td-len': segment.len,
            }"
            :title="segment.label || undefined"
            ><span
              v-if="segment.label"
              class="td-label"
              >{{ segment.label }}</span
            ></span
          >
          <span
            v-for="gap in row.gaps"
            :key="`${row.key}-gap${gap}`"
            class="td-gap"
            :style="{ gridRow: row.row, gridColumn: gap + 2 }"
            aria-hidden="true"
          ></span>
        </template>

        <span
          v-for="mark in model.marks"
          :key="mark.key"
          class="td-mark"
          :style="{
            gridRow: `${model.firstRow} / ${model.lastRow}`,
            gridColumn: mark.column,
            marginLeft: mark.offset,
          }"
          aria-hidden="true"
        ></span>
        <template v-if="model.markLabels">
          <span
            v-for="mark in model.marks"
            :key="`${mark.key}-label`"
            class="td-mark-label"
            :style="{
              gridRow: model.lastRow,
              gridColumn: mark.column,
              marginLeft: mark.offset,
            }"
            >{{ mark.label }}</span
          >
        </template>
      </div>
    </div>

    <ul
      v-if="model.problems.length"
      class="td-problems"
      role="note"
    >
      <li
        v-for="(problem, index) in model.problems"
        :key="index"
      >
        时序图：{{ problem }}
      </li>
    </ul>
  </figure>
</template>

<style scoped>
.timing-diagram {
  --td-line: var(--vp-c-text-1);
  --td-hatch: color-mix(in srgb, var(--vp-c-text-3) 70%, transparent);
  --td-h: 28px;
  --td-s: 1.5px;
  --td-hi: 5px;
  --td-lo: 21.5px;
  --td-mid: 13.25px;
  --td-swing: 18px;
  --td-slant: 4px;
  margin: 1.25rem 0;
  color: var(--vp-c-text-1);
  font-size: 0.8125rem;
}

.td-title {
  margin-bottom: 0.5rem;
  color: var(--vp-c-text-1);
  font-size: 0.875rem;
  font-weight: 600;
  text-align: center;
}

.td-scroll {
  overflow-x: auto;
  padding-bottom: 2px;
}

.td-grid {
  display: grid;
  grid-template-columns:
    max-content
    repeat(
      var(--td-n),
      minmax(calc(0.75rem * var(--td-scale)), calc(2.25rem * var(--td-scale)))
    );
  row-gap: 0.375rem;
  width: fit-content;
  margin: 0 auto;
  padding: 0 0.75rem 0 0;
}

.td-name {
  position: sticky;
  left: 0;
  z-index: 3;
  grid-column: 1;
  min-height: var(--td-h);
  padding-right: 0.75rem;
  background-color: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  font-family: var(--vp-font-family-mono);
  font-size: 0.75rem;
  line-height: var(--td-h);
  text-align: right;
  white-space: nowrap;
}

.td-name.is-spacer {
  min-height: 0.5rem;
  line-height: 0.5rem;
}

.td-name.is-group {
  min-height: 1.25rem;
  color: var(--vp-c-text-2);
  font-family: var(--vp-font-family-base);
  font-weight: 600;
  line-height: 1.25rem;
  text-align: left;
}

.td-tick {
  justify-self: start;
  color: var(--vp-c-text-3);
  font-family: var(--vp-font-family-mono);
  font-size: 0.6875rem;
  line-height: 1.25rem;
  transform: translateX(-50%);
}

.td-tick.is-end {
  justify-self: end;
  transform: translateX(50%);
}

.td-seg {
  position: relative;
  display: block;
  height: var(--td-h);
  min-width: 0;
  background-repeat: no-repeat;
}

.kind-high {
  background-image: linear-gradient(var(--td-line), var(--td-line));
  background-position: 0 var(--td-hi);
  background-size: 100% var(--td-s);
}

.kind-low {
  background-image: linear-gradient(var(--td-line), var(--td-line));
  background-position: 0 var(--td-lo);
  background-size: 100% var(--td-s);
}

.kind-pu,
.kind-pd {
  background-image: repeating-linear-gradient(
    to right,
    var(--td-line) 0 3px,
    transparent 3px 6px
  );
  background-size: 100% var(--td-s);
}

.kind-pu {
  background-position: 0 var(--td-hi);
}

.kind-pd {
  background-position: 0 var(--td-lo);
}

.kind-z {
  background-image: linear-gradient(var(--vp-c-text-3), var(--vp-c-text-3));
  background-position: 0 var(--td-mid);
  background-size: 100% var(--td-s);
}

.has-edge::before {
  position: absolute;
  top: var(--td-hi);
  left: 0;
  height: calc(var(--td-swing) - var(--td-s));
  border-left: var(--td-s) solid var(--td-line);
  content: "";
}

.kind-clkP,
.kind-clkN {
  --td-p: calc(100% / var(--td-len));
  background-image:
    linear-gradient(to right, var(--td-line) 50%, transparent 50%),
    linear-gradient(to right, transparent 50%, var(--td-line) 50%),
    linear-gradient(
      to right,
      var(--td-line) 0 var(--td-s),
      transparent var(--td-s) calc(50% - var(--td-s) / 2),
      var(--td-line) calc(50% - var(--td-s) / 2) calc(50% + var(--td-s) / 2),
      transparent calc(50% + var(--td-s) / 2)
    );
  background-repeat: repeat-x;
  background-size:
    var(--td-p) var(--td-s),
    var(--td-p) var(--td-s),
    var(--td-p) var(--td-swing);
}

.kind-clkP {
  background-position:
    0 var(--td-hi),
    0 var(--td-lo),
    0 var(--td-hi);
}

.kind-clkN {
  background-position:
    0 var(--td-lo),
    0 var(--td-hi),
    0 var(--td-hi);
}

.kind-data::before,
.kind-x::before {
  position: absolute;
  top: var(--td-hi);
  right: 0;
  left: 0;
  height: var(--td-swing);
  background-color: var(--td-line);
  clip-path: polygon(
    0 50%,
    var(--td-slant) 0,
    calc(100% - var(--td-slant)) 0,
    100% 50%,
    calc(100% - var(--td-slant)) 100%,
    var(--td-slant) 100%
  );
  content: "";
}

.kind-data::after,
.kind-x::after {
  position: absolute;
  top: calc(var(--td-hi) + var(--td-s));
  right: var(--td-s);
  left: var(--td-s);
  height: calc(var(--td-swing) - 2 * var(--td-s));
  background:
    linear-gradient(var(--td-fill, transparent), var(--td-fill, transparent)),
    var(--vp-c-bg);
  clip-path: polygon(
    0 50%,
    calc(var(--td-slant) - 0.6px) 0,
    calc(100% - var(--td-slant) + 0.6px) 0,
    100% 50%,
    calc(100% - var(--td-slant) + 0.6px) 100%,
    calc(var(--td-slant) - 0.6px) 100%
  );
  content: "";
}

.kind-x::after {
  background:
    repeating-linear-gradient(-45deg, var(--td-hatch) 0 1px, transparent 1px 5px),
    var(--vp-c-bg);
}

.tone-yellow {
  --td-fill: var(--vp-c-yellow-soft);
}

.tone-red {
  --td-fill: var(--vp-c-red-soft);
}

.tone-brand {
  --td-fill: var(--vp-c-brand-soft);
}

.tone-green {
  --td-fill: var(--vp-c-green-soft);
}

.tone-purple {
  --td-fill: var(--vp-c-purple-soft);
}

.tone-gray {
  --td-fill: var(--vp-c-gray-soft);
}

.td-label {
  position: relative;
  z-index: 1;
  display: block;
  overflow: hidden;
  padding: 0 calc(var(--td-slant) + 2px);
  color: var(--vp-c-text-1);
  font-family: var(--vp-font-family-mono);
  font-size: 0.6875rem;
  line-height: var(--td-h);
  text-align: center;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.td-gap {
  z-index: 2;
  justify-self: start;
  width: 7px;
  height: var(--td-h);
  margin-left: -3.5px;
  border-right: var(--td-s) solid var(--td-line);
  border-left: var(--td-s) solid var(--td-line);
  background-color: var(--vp-c-bg);
  transform: skewX(-18deg);
}

.td-mark {
  z-index: 0;
  justify-self: start;
  width: 0;
  border-left: 1px dashed var(--vp-c-brand-1);
  pointer-events: none;
}

.td-mark-label {
  justify-self: start;
  margin-top: 0.25rem;
  color: var(--vp-c-brand-1);
  font-size: 0.6875rem;
  line-height: 1.2;
  white-space: nowrap;
  transform: translateX(-50%);
}

.td-problems {
  margin: 0.5rem 0 0;
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  background-color: var(--vp-c-danger-soft);
  color: var(--vp-c-danger-1);
  font-size: 0.8125rem;
  list-style: none;
}

.td-problems li + li {
  margin-top: 0.25rem;
}
</style>
