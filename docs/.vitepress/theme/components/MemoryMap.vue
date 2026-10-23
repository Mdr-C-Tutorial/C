<script setup lang="ts">
import { computed } from "vue";

type Part = { code: boolean; text: string };

interface Region {
  key: string;
  name: string;
  start: bigint | null;
  end: bigint | null;
  sizeText: string;
  desc: string;
  tone: string;
  reserved: boolean;
  gap: boolean;
  grow: "" | "up" | "down";
  span: number;
  startLabel: string;
  endLabel: string;
}

interface Edge {
  addr: string;
  marks: string[];
}

interface Row {
  key: string;
  name: string;
  size: string;
  desc: Part[];
  arrow: string;
  arrowTitle: string;
  title: string;
  classes: string[];
  span: number;
  top: Edge;
  bottom: Edge | null;
}

const props = defineProps({
  // 区域数组（或同样结构的 JSON 字符串），按地址从低到高列出
  regions: { type: [Array, String], required: true },
  title: { type: String, default: "" },
  // desc：高地址在上（默认）；asc：低地址在上
  order: { type: String, default: "desc" },
  // 地址显示的十六进制位数；默认按最大地址取 4、8、12 或 16 位
  digits: { type: [Number, String], default: undefined },
});

const TONES = ["brand", "green", "yellow", "red", "purple", "gray"];
const ZERO = BigInt(0);
const ONE = BigInt(1);
const KIB = BigInt(1024);
const UNITS: [string, bigint][] = [
  ["TiB", KIB * KIB * KIB * KIB],
  ["GiB", KIB * KIB * KIB],
  ["MiB", KIB * KIB],
  ["KiB", KIB],
];

function toText(value: unknown): string {
  return value === undefined || value === null ? "" : String(value);
}

function parseAddress(value: unknown): bigint | null {
  if (typeof value === "bigint") return value >= ZERO ? value : null;
  if (typeof value === "number") {
    return Number.isSafeInteger(value) && value >= 0 ? BigInt(value) : null;
  }
  if (typeof value === "string") {
    const text = value.trim().replace(/[_\s']/g, "");
    if (/^0x[0-9a-f]+$/i.test(text) || /^\d+$/.test(text)) return BigInt(text);
  }
  return null;
}

function parseSize(value: unknown): {
  bytes: bigint | null;
  label: string;
  bad: boolean;
} {
  if (value === undefined || value === null || value === "") {
    return { bytes: null, label: "", bad: false };
  }
  if (typeof value === "number") {
    return Number.isSafeInteger(value) && value > 0
      ? { bytes: BigInt(value), label: "", bad: false }
      : { bytes: null, label: "", bad: true };
  }
  const text = String(value).trim();
  if (text.startsWith("-")) return { bytes: null, label: "", bad: true };
  const match =
    /^(0x[0-9a-f]+|\d+(?:\.\d+)?)\s*(k|kb|kib|m|mb|mib|g|gb|gib|b|字节)?$/i.exec(
      text.replace(/_/g, ""),
    );
  if (!match) return { bytes: null, label: text, bad: false };
  const unit = (match[2] ?? "").toLowerCase();
  const factor = unit.startsWith("k")
    ? KIB
    : unit.startsWith("m")
      ? KIB * KIB
      : unit.startsWith("g")
        ? KIB * KIB * KIB
        : ONE;
  if (match[1].includes(".")) {
    const bytes = Number(match[1]) * Number(factor);
    return Number.isSafeInteger(bytes) && bytes > 0
      ? { bytes: BigInt(bytes), label: "", bad: false }
      : { bytes: null, label: text, bad: false };
  }
  const bytes = BigInt(match[1]) * factor;
  return bytes > ZERO
    ? { bytes, label: "", bad: false }
    : { bytes: null, label: "", bad: true };
}

function formatSize(bytes: bigint): string {
  for (const [unit, factor] of UNITS) {
    if (bytes >= factor && bytes % factor === ZERO) return `${bytes / factor} ${unit}`;
  }
  const ten = BigInt(10);
  for (const [unit, factor] of UNITS) {
    if (bytes >= factor && (bytes * ten) % factor === ZERO) {
      return `${Number((bytes * ten) / factor) / 10} ${unit}`;
    }
  }
  return `${bytes} 字节`;
}

function inlineParts(text: string): Part[] {
  if (!text) return [];
  const pieces = text.split("`");
  if (pieces.length < 3 || pieces.length % 2 === 0) return [{ code: false, text }];
  return pieces
    .map((piece, index) => ({ code: index % 2 === 1, text: piece }))
    .filter((part) => part.text !== "");
}

function normalizeRegions(input: unknown, problems: string[]): Region[] {
  let list = input;
  if (typeof list === "string") {
    try {
      list = JSON.parse(list);
    } catch {
      problems.push("regions 不是合法的 JSON 数组。");
      return [];
    }
  }
  if (!Array.isArray(list)) {
    problems.push("regions 必须是数组。");
    return [];
  }

  return list.map((raw, index) => {
    const item: Record<string, unknown> =
      raw !== null && typeof raw === "object"
        ? (raw as Record<string, unknown>)
        : { name: raw };
    const name = toText(item.name);
    const label = name ? `区域「${name}」` : `第 ${index + 1} 个区域`;
    const gap = item.gap === true;

    let start = parseAddress(item.start);
    if (item.start !== undefined && start === null)
      problems.push(`${label}的 start 无效。`);
    let end = parseAddress(item.end);
    if (item.end !== undefined && end === null) problems.push(`${label}的 end 无效。`);
    const size = parseSize(item.size);
    if (size.bad) problems.push(`${label}的 size 无效。`);

    if (start !== null && end === null && size.bytes !== null) end = start + size.bytes;
    if (start === null && end !== null && size.bytes !== null && end >= size.bytes) {
      start = end - size.bytes;
    }
    let bytes = size.bytes;
    if (start !== null && end !== null) {
      if (end <= start) {
        problems.push(`${label}的结束地址不大于起始地址。`);
        end = null;
      } else {
        const span = end - start;
        if (bytes !== null && bytes !== span) {
          problems.push(`${label}的 size 与 start、end 不一致，按 start、end 绘制。`);
        }
        if (
          item.end !== undefined &&
          span % BigInt(2) === ONE &&
          (span + ONE) % KIB === ZERO
        ) {
          problems.push(
            `${label}的 end 看起来是最后一个字节的地址；end 应写结束地址的下一个字节（不含），或改用 size。`,
          );
        }
        bytes = span;
      }
    }

    let tone = toText(item.tone);
    if (tone && !TONES.includes(tone)) {
      problems.push(`${label}的 tone「${tone}」无效。`);
      tone = "";
    }
    const growText = toText(item.grow);
    let grow: Region["grow"] = "";
    if (growText === "up" || growText === "down") grow = growText;
    else if (growText) problems.push(`${label}的 grow 只能是 up 或 down。`);
    const spanValue = Number(item.span ?? 1);
    const span = Number.isFinite(spanValue) ? Math.min(Math.max(spanValue, 0.5), 8) : 1;

    return {
      key: `m${index}`,
      name,
      start,
      end,
      sizeText: size.label || (bytes !== null ? formatSize(bytes) : ""),
      desc: toText(item.desc),
      tone,
      reserved: item.reserved === true,
      gap,
      grow,
      span,
      startLabel: toText(item.startLabel),
      endLabel: toText(item.endLabel),
    };
  });
}

function autoGap(key: string): Region {
  return {
    key,
    name: "",
    start: null,
    end: null,
    sizeText: "",
    desc: "",
    tone: "",
    reserved: false,
    gap: true,
    grow: "",
    span: 1,
    startLabel: "",
    endLabel: "",
  };
}

const model = computed(() => {
  const problems: string[] = [];
  const order = props.order === "asc" ? "asc" : "desc";
  if (props.order !== "asc" && props.order !== "desc") {
    problems.push(`order「${props.order}」无效，按 desc 绘制。`);
  }
  let regions = normalizeRegions(props.regions, problems);
  if (regions.length === 0 && problems.length === 0) problems.push("regions 为空。");

  // 非间隙区域都给了起始地址时按地址排序，否则保持书写顺序（视为从低到高）。
  // 没写地址的间隙跟随它前面的区域。
  const real = regions.filter((region) => !region.gap);
  if (real.length > 0 && real.every((region) => region.start !== null)) {
    let lastStart = ZERO;
    regions = regions
      .map((region, index) => {
        const after = region.start === null;
        if (!after) lastStart = region.start as bigint;
        return {
          region,
          index,
          key: after ? lastStart : (region.start as bigint),
          after,
        };
      })
      .sort((a, b) => {
        if (a.key !== b.key) return a.key < b.key ? -1 : 1;
        if (a.after !== b.after) return a.after ? 1 : -1;
        return a.index - b.index;
      })
      .map((item) => item.region);
  }

  // 地址不连续处补一个间隙；重叠时给出提示
  const lowToHigh: Region[] = [];
  regions.forEach((region, index) => {
    const prev = lowToHigh[lowToHigh.length - 1];
    if (
      prev &&
      prev.end !== null &&
      region.start !== null &&
      !(prev.gap && region.gap)
    ) {
      if (region.start > prev.end && !prev.gap && !region.gap) {
        lowToHigh.push(autoGap(`gap${index}`));
      } else if (region.start < prev.end) {
        problems.push(
          `区域「${region.name || index + 1}」与下方区域「${prev.name}」重叠。`,
        );
      }
    }
    lowToHigh.push(region);
  });

  // 地址位数
  let digits = Number(props.digits);
  if (!Number.isInteger(digits) || digits <= 0) {
    let max = ZERO;
    for (const region of lowToHigh) {
      if (region.start !== null && region.start > max) max = region.start;
      if (region.end !== null && region.end - ONE > max) max = region.end - ONE;
    }
    const limits: [number, bigint][] = [
      [4, BigInt("0xFFFF")],
      [8, BigInt("0xFFFFFFFF")],
      [12, BigInt("0xFFFFFFFFFFFF")],
    ];
    digits = limits.find(([, limit]) => max <= limit)?.[0] ?? 16;
  }
  const overflow = ONE << BigInt(digits * 4);
  const format = (value: bigint | null): string => {
    if (value === null) return "";
    // 地址空间顶端（如 32 位的 0x100000000）显示为最后一个字节 0xFFFFFFFF
    const shown = value === overflow ? value - ONE : value;
    return `0x${shown.toString(16).toUpperCase().padStart(digits, "0")}`;
  };

  const display = order === "desc" ? [...lowToHigh].reverse() : lowToHigh;
  const topAddr = (region: Region) => (order === "desc" ? region.end : region.start);
  const bottomAddr = (region: Region) => (order === "desc" ? region.start : region.end);
  const topMark = (region: Region) =>
    order === "desc" ? region.endLabel : region.startLabel;
  const bottomMark = (region: Region) =>
    order === "desc" ? region.startLabel : region.endLabel;

  const edgeBetween = (above: Region | undefined, below: Region | undefined): Edge => {
    const a = above ? bottomAddr(above) : null;
    const b = below ? topAddr(below) : null;
    const addr = a !== null ? a : b;
    const marks = [above ? bottomMark(above) : "", below ? topMark(below) : ""].filter(
      (mark, index, list) => mark !== "" && list.indexOf(mark) === index,
    );
    return { addr: format(addr), marks };
  };

  const rows: Row[] = display.map((region, index) => {
    const range =
      region.start !== null && region.end !== null
        ? `${format(region.start)}–${format(region.end)}`
        : region.start !== null
          ? `${format(region.start)} 起`
          : "";
    let arrow = "";
    let arrowTitle = "";
    if (region.grow) {
      const towardTop = (region.grow === "up") === (order === "desc");
      arrow = towardTop ? "↑" : "↓";
      arrowTitle = region.grow === "up" ? "向高地址增长" : "向低地址增长";
    }
    const classes: string[] = [];
    if (index === 0) classes.push("is-first");
    if (region.gap) classes.push("is-gap");
    if (region.reserved) classes.push("is-reserved");
    if (region.tone) classes.push(`tone-${region.tone}`);
    const titleParts = [region.name, range, region.sizeText, arrowTitle].filter(
      Boolean,
    );
    return {
      key: region.key,
      name: region.gap && !region.name ? "⋯" : region.name,
      size: region.sizeText,
      desc: inlineParts(region.desc),
      arrow,
      arrowTitle,
      title:
        titleParts.join("，") +
        (region.desc ? `。${region.desc.replace(/`/g, "")}` : ""),
      classes,
      span: region.gap ? 0 : region.span,
      top: edgeBetween(display[index - 1], region),
      bottom: index === display.length - 1 ? edgeBetween(region, undefined) : null,
    };
  });

  const allMarks = rows.flatMap((row) => [
    ...row.top.marks,
    ...(row.bottom?.marks ?? []),
  ]);
  const sizerAddr = rows.some((row) => row.top.addr || row.bottom?.addr)
    ? `0x${"0".repeat(digits)}`
    : "";
  const sizerMark = allMarks.reduce(
    (longest, mark) => (mark.length > longest.length ? mark : longest),
    "",
  );

  return { rows, problems, sizerAddr, sizerMark, hasMarks: allMarks.length > 0 };
});
</script>

<template>
  <figure
    class="memory-map"
    :class="{ 'has-marks': model.hasMarks, 'has-addr': model.sizerAddr }"
  >
    <figcaption
      v-if="title"
      class="mm-title"
    >
      {{ title }}
    </figcaption>

    <div class="mm-scroll">
      <div class="mm-grid">
        <template
          v-for="(row, index) in model.rows"
          :key="row.key"
        >
          <div class="mm-addr">
            <span
              v-if="index === 0 && model.sizerAddr"
              class="mm-sizer"
              aria-hidden="true"
              >{{ model.sizerAddr }}</span
            >
            <span
              v-if="row.top.addr"
              class="mm-edge is-top"
              >{{ row.top.addr }}</span
            >
            <span
              v-if="row.bottom && row.bottom.addr"
              class="mm-edge is-bottom"
              >{{ row.bottom.addr }}</span
            >
          </div>
          <div
            class="mm-box"
            :class="row.classes"
            :style="row.span ? { '--mm-span': row.span } : undefined"
            :title="row.title"
          >
            <span class="mm-name"
              >{{ row.name
              }}<span
                v-if="row.arrow"
                class="mm-arrow"
                :title="row.arrowTitle"
                >{{ row.arrow }}</span
              ></span
            >
            <span
              v-if="row.size"
              class="mm-size"
              >{{ row.size }}</span
            >
            <span
              v-if="row.desc.length"
              class="mm-desc"
              ><template
                v-for="(part, partIndex) in row.desc"
                :key="partIndex"
                ><code v-if="part.code">{{ part.text }}</code
                ><template v-else>{{ part.text }}</template></template
              ></span
            >
          </div>
          <div
            v-if="model.hasMarks"
            class="mm-marks"
          >
            <span
              v-if="index === 0"
              class="mm-sizer"
              aria-hidden="true"
              >{{ model.sizerMark }}</span
            >
            <span
              v-if="row.top.marks.length"
              class="mm-edge is-top"
            >
              <span
                v-for="mark in row.top.marks"
                :key="mark"
                >{{ mark }}</span
              >
            </span>
            <span
              v-if="row.bottom && row.bottom.marks.length"
              class="mm-edge is-bottom"
            >
              <span
                v-for="mark in row.bottom.marks"
                :key="mark"
                >{{ mark }}</span
              >
            </span>
          </div>
        </template>
      </div>
    </div>

    <ul
      v-if="model.problems.length"
      class="mm-problems"
      role="note"
    >
      <li
        v-for="(problem, index) in model.problems"
        :key="index"
      >
        内存布局图：{{ problem }}
      </li>
    </ul>
  </figure>
</template>

<style scoped>
.memory-map {
  --mm-line: color-mix(in srgb, var(--vp-c-text-2) 55%, transparent);
  --mm-hatch: color-mix(in srgb, var(--vp-c-text-3) 45%, transparent);
  container-type: inline-size;
  margin: 1.25rem 0;
  color: var(--vp-c-text-1);
  font-size: 0.8125rem;
  line-height: 1.35;
}

.mm-title {
  margin-bottom: 0.5rem;
  color: var(--vp-c-text-1);
  font-size: 0.875rem;
  font-weight: 600;
  text-align: center;
}

.mm-scroll {
  overflow-x: auto;
}

.mm-grid {
  display: grid;
  grid-template-columns: max-content minmax(7rem, 1fr);
  padding: 0.625rem 0;
}

.has-marks .mm-grid {
  grid-template-columns: max-content minmax(7rem, 1fr) max-content;
}

.mm-addr,
.mm-marks {
  position: relative;
  min-width: 0.75rem;
}

.mm-addr {
  padding-right: 0.875rem;
}

.memory-map:not(.has-addr) .mm-addr {
  min-width: 0;
  padding-right: 0;
}

.mm-marks {
  padding-left: 0.875rem;
}

.mm-sizer {
  display: block;
  height: 0;
  overflow: hidden;
  font-family: var(--vp-font-family-mono);
  font-size: 0.75rem;
  visibility: hidden;
  white-space: nowrap;
}

.mm-edge {
  position: absolute;
  display: flex;
  flex-direction: column;
  font-family: var(--vp-font-family-mono);
  font-size: 0.75rem;
  line-height: 1.2;
  white-space: nowrap;
}

.mm-edge.is-top {
  top: 0;
  transform: translateY(-50%);
}

.mm-edge.is-bottom {
  bottom: 0;
  transform: translateY(50%);
}

.mm-addr .mm-edge {
  right: 0.875rem;
  color: var(--vp-c-text-2);
}

.mm-addr .mm-edge::after {
  position: absolute;
  top: 50%;
  left: calc(100% + 0.25rem);
  width: 0.625rem;
  border-top: 1px solid var(--mm-line);
  content: "";
}

.mm-marks .mm-edge {
  left: 0.875rem;
  color: var(--vp-c-brand-1);
}

.mm-marks .mm-edge::before {
  position: absolute;
  top: 50%;
  right: calc(100% + 0.25rem);
  width: 0.625rem;
  border-top: 1px solid var(--mm-line);
  content: "";
}

.mm-box {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 0.125rem;
  min-width: 0;
  min-height: calc(2.75rem * var(--mm-span, 1));
  margin-top: -1px;
  padding: 0.375rem 0.625rem;
  border: 1px solid var(--mm-line);
  background-color: var(--mm-fill, transparent);
  text-align: center;
}

.mm-box.is-first {
  margin-top: 0;
}

.mm-box.tone-brand {
  --mm-fill: var(--vp-c-brand-soft);
}

.mm-box.tone-green {
  --mm-fill: var(--vp-c-green-soft);
}

.mm-box.tone-yellow {
  --mm-fill: var(--vp-c-yellow-soft);
}

.mm-box.tone-red {
  --mm-fill: var(--vp-c-red-soft);
}

.mm-box.tone-purple {
  --mm-fill: var(--vp-c-purple-soft);
}

.mm-box.tone-gray {
  --mm-fill: var(--vp-c-gray-soft);
}

.mm-box.is-reserved {
  color: var(--vp-c-text-3);
  background-image: repeating-linear-gradient(
    -45deg,
    var(--mm-hatch) 0 1px,
    transparent 1px 6px
  );
}

.mm-box.is-gap {
  min-height: 1.75rem;
  margin-top: 0;
  padding-top: 0.125rem;
  padding-bottom: 0.125rem;
  border-top-width: 0;
  border-bottom-width: 0;
  border-left-style: dashed;
  border-right-style: dashed;
  color: var(--vp-c-text-3);
}

.mm-box.is-gap + .mm-marks + .mm-addr + .mm-box,
.mm-box.is-gap + .mm-addr + .mm-box {
  margin-top: 0;
}

.mm-name {
  font-weight: 600;
  overflow-wrap: anywhere;
}

.mm-arrow {
  margin-left: 0.375rem;
  color: var(--vp-c-brand-1);
  font-weight: 700;
}

.mm-size {
  color: var(--vp-c-text-2);
  font-family: var(--vp-font-family-mono);
  font-size: 0.75rem;
}

.mm-desc {
  color: var(--vp-c-text-2);
  font-size: 0.75rem;
  overflow-wrap: anywhere;
}

.mm-problems {
  margin: 0.5rem 0 0;
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  background-color: var(--vp-c-danger-soft);
  color: var(--vp-c-danger-1);
  font-size: 0.8125rem;
  list-style: none;
}

.mm-problems li + li {
  margin-top: 0.25rem;
}

@container (max-width: 419.98px) {
  .mm-edge,
  .mm-sizer {
    font-size: 0.6875rem;
  }

  .mm-box {
    padding-right: 0.375rem;
    padding-left: 0.375rem;
  }
}
</style>
