<script setup lang="ts">
import { computed } from "vue";

type Mode = "register" | "packet" | "frame";

type Part = { code: boolean; text: string };

interface Field {
  key: string;
  name: string;
  bits: number;
  range: [number, number] | null;
  hi: number;
  lo: number;
  start: number;
  end: number;
  desc: string;
  value: string;
  access: string;
  reserved: boolean;
  tone: string;
  variable: boolean;
  size: string;
  group: string;
  byteUnit: boolean;
  auto: boolean;
}

interface BitNumber {
  key: string;
  column: number;
  text: string;
}

interface Cell {
  key: string;
  column: string;
  name: string;
  nameParts: string[];
  value: string;
  title: string;
  classes: string[];
  vertical: boolean;
}

interface Row {
  key: string;
  offset: string;
  numbers: BitNumber[];
  cells: Cell[];
  access: { key: string; column: string; text: string }[];
}

interface Layout {
  key: "wide" | "narrow";
  lane: number;
  gutter: boolean;
  head: BitNumber[];
  rows: Row[];
}

interface FrameItem {
  key: string;
  name: string;
  nameParts: string[];
  value: string;
  size: string;
  title: string;
  grow: number;
  minWidth: string;
  classes: string[];
}

interface FrameGroup {
  key: string;
  label: string;
  grow: number;
  items: FrameItem[];
}

interface TableCell {
  key: string;
  mono: boolean;
  parts: Part[];
}

interface Model {
  mode: Mode;
  switchClass: string;
  layouts: Layout[];
  groups: FrameGroup[];
  hasGroups: boolean;
  table: { head: string[]; rows: { key: string; cells: TableCell[] }[] } | null;
  problems: string[];
}

const props = defineProps({
  // 字段数组（或同样结构的 JSON 字符串）
  fields: { type: [Array, String], required: true },
  // register：寄存器；packet：协议首部；frame：串行帧/报文
  mode: { type: String, default: "register" },
  // register 模式的寄存器总位数
  width: { type: [Number, String], default: undefined },
  // 每行位数（register 默认 16，packet 默认 32）
  lane: { type: [Number, String], default: undefined },
  // 窄屏时每行位数（默认为 lane 的一半）
  narrowLane: { type: [Number, String], default: undefined },
  // register 模式图中最低位的位号，只画寄存器的一段时用（range 仍写真实位号）
  lsb: { type: [Number, String], default: 0 },
  title: { type: String, default: "" },
  // frame 模式长度标注单位：bit 或 byte
  unit: { type: String, default: "" },
  // 是否在图下列出字段表；默认在有字段写了 desc 时显示
  table: { type: Boolean, default: undefined },
  // packet 模式是否在左侧显示每行的字节偏移
  offsets: { type: Boolean, default: true },
});

const MODES: Mode[] = ["register", "packet", "frame"];
const TONES = ["brand", "green", "yellow", "red", "purple", "gray"];
const SWITCH_STEPS = [320, 360, 400, 440, 480, 520, 560, 600, 640, 720, 800, 960];
// 估算单元格宽度用的参考版心宽度（桌面正文栏与手机正文栏）
const WIDE_PX = 688;
const NARROW_PX = 343;
const GUTTER_PX = 36;
const NAME_PX = 13;

function toNumber(value: unknown): number {
  if (typeof value === "number") return value;
  if (typeof value === "string" && value.trim() !== "") return Number(value.trim());
  return Number.NaN;
}

function positiveInt(value: unknown): number | null {
  const n = toNumber(value);
  return Number.isInteger(n) && n > 0 ? n : null;
}

function toText(value: unknown): string {
  return value === undefined || value === null ? "" : String(value);
}

function parseRange(value: unknown): [number, number] | null {
  let first: unknown = value;
  let second: unknown = value;
  if (Array.isArray(value)) {
    first = value[0];
    second = value.length > 1 ? value[1] : value[0];
  } else if (typeof value === "string") {
    const parts = value.split(/\s*[:～~–-]\s*/);
    first = parts[0];
    second = parts.length > 1 ? parts[1] : parts[0];
  }
  const a = toNumber(first);
  const b = toNumber(second);
  if (!Number.isInteger(a) || !Number.isInteger(b) || a < 0 || b < 0) return null;
  return a >= b ? [a, b] : [b, a];
}

function textWidth(text: string): number {
  let width = 0;
  for (const ch of text) {
    const code = ch.codePointAt(0) ?? 0;
    if (code >= 0x2e80) {
      width += 1;
    } else if (/[A-Z0-9#%&@_]/.test(ch)) {
      width += 0.72;
    } else if (/[\s.,:;'|!()[\]{}il]/.test(ch)) {
      width += 0.32;
    } else {
      width += 0.58;
    }
  }
  return width * NAME_PX;
}

function inlineParts(text: string): Part[] {
  const pieces = text.split("`");
  if (pieces.length < 3 || pieces.length % 2 === 0) return [{ code: false, text }];
  return pieces
    .map((piece, index) => ({ code: index % 2 === 1, text: piece }))
    .filter((part) => part.text !== "");
}

// 在「[」之前、「_」和「/」之后允许换行，避免 MODE7[1:0] 断成 MODE7[1:0 与 ]
function breakParts(text: string): string[] {
  const parts: string[] = [];
  let current = "";
  for (const ch of text) {
    if (ch === "[" && current) {
      parts.push(current);
      current = "";
    }
    current += ch;
    if (ch === "_" || ch === "/") {
      parts.push(current);
      current = "";
    }
  }
  if (current) parts.push(current);
  return parts;
}

function plain(text: string): string {
  return text.replace(/`/g, "");
}

function bitsText(hi: number, lo: number): string {
  return hi === lo ? String(hi) : `${hi}:${lo}`;
}

function offsetText(start: number, end: number): string {
  return start === end ? String(start) : `${start}–${end}`;
}

function fieldLabel(field: Field, index?: number): string {
  if (field.name) return `字段「${field.name}」`;
  return index === undefined ? "未命名字段" : `第 ${index + 1} 个字段`;
}

function sizeText(field: Field, unit: string): string {
  if (field.size) return field.size;
  if (field.variable) return "可变";
  if ((field.byteUnit || unit === "byte") && field.bits % 8 === 0) {
    return `${field.bits / 8} 字节`;
  }
  return `${field.bits} 位`;
}

// frame 模式字段框的最小宽度：尽量让名称和取值不折行，最多 7rem
function frameMinWidth(field: Field): number {
  const name = textWidth(field.name) + 14;
  const value = field.value.length * 7.4 + 14;
  return Math.round(Math.min(Math.max(56, name, value), 112));
}

function cellClasses(field: Field): string[] {
  const classes: string[] = [];
  if (field.tone) classes.push(`tone-${field.tone}`);
  if (field.reserved) classes.push("is-reserved");
  if (field.variable) classes.push("is-variable");
  return classes;
}

// 容器宽度低于阈值时改用窄屏布局；阈值向下取到预设档位
function switchClass(px: number): string {
  const step =
    [...SWITCH_STEPS].reverse().find((item) => item <= px) ?? SWITCH_STEPS[0];
  return `bf-sw-${step}`;
}

function normalizeMode(value: string, problems: string[]): Mode {
  if ((MODES as string[]).includes(value)) return value as Mode;
  problems.push(`mode「${value}」无效，按 register 绘制。`);
  return "register";
}

function normalizeFields(input: unknown, problems: string[]): Field[] {
  let list = input;
  if (typeof list === "string") {
    try {
      list = JSON.parse(list);
    } catch {
      problems.push("fields 不是合法的 JSON 数组。");
      return [];
    }
  }
  if (!Array.isArray(list)) {
    problems.push("fields 必须是数组。");
    return [];
  }

  return list.map((raw, index) => {
    const item: Record<string, unknown> =
      raw !== null && typeof raw === "object"
        ? (raw as Record<string, unknown>)
        : { name: raw };
    const name = toText(item.name);
    const label = name ? `字段「${name}」` : `第 ${index + 1} 个字段`;

    let range: [number, number] | null = null;
    if (item.range !== undefined) {
      range = parseRange(item.range);
      if (!range) problems.push(`${label}的 range 无效。`);
    }

    let bits = positiveInt(item.bits);
    let byteUnit = false;
    if (bits === null && item.bytes !== undefined) {
      const bytes = positiveInt(item.bytes);
      if (bytes !== null) {
        bits = bytes * 8;
        byteUnit = true;
      }
    }
    if (range) {
      const width = range[0] - range[1] + 1;
      if (bits !== null && bits !== width) {
        problems.push(`${label}的 bits 与 range 不一致，按 range 绘制。`);
      }
      bits = width;
    }
    if (bits === null) {
      problems.push(`${label}缺少有效的 bits、bytes 或 range，按 1 位绘制。`);
      bits = 1;
    }

    let tone = toText(item.tone);
    if (tone && !TONES.includes(tone)) {
      problems.push(`${label}的 tone「${tone}」无效。`);
      tone = "";
    }

    return {
      key: `f${index}`,
      name,
      bits,
      range,
      hi: 0,
      lo: 0,
      start: 0,
      end: 0,
      desc: toText(item.desc),
      value: toText(item.value),
      access: toText(item.access),
      reserved: item.reserved === true,
      tone,
      variable: item.variable === true,
      size: toText(item.size),
      group: toText(item.group),
      byteUnit,
      auto: false,
    };
  });
}

function reservedGap(hi: number, lo: number): Field {
  return {
    key: `gap${hi}`,
    name: "保留",
    bits: hi - lo + 1,
    range: [hi, lo],
    hi,
    lo,
    start: 0,
    end: 0,
    desc: "",
    value: "",
    access: "",
    reserved: true,
    tone: "",
    variable: false,
    size: "",
    group: "",
    byteUnit: false,
    auto: true,
  };
}

function narrowLaneFor(lane: number): number | null {
  const explicit = positiveInt(props.narrowLane);
  if (explicit !== null) return explicit < lane ? explicit : null;
  return lane >= 16 && lane % 2 === 0 ? lane / 2 : null;
}

function makeCell(
  field: Field,
  key: string,
  column: string,
  span: number,
  pxPerBit: number,
  contLeft: boolean,
  contRight: boolean,
  title: string,
): Cell {
  const classes = cellClasses(field);
  if (contLeft) classes.push("cont-left");
  if (contRight) classes.push("cont-right");
  const vertical =
    span === 1 && field.name.length > 1 && textWidth(field.name) > pxPerBit - 10;
  return {
    key,
    column,
    name: field.name,
    nameParts: breakParts(field.name),
    value: field.value,
    title,
    classes,
    vertical,
  };
}

function registerTitle(field: Field, lsb: number): string {
  let text = `${field.name}：位 ${bitsText(field.hi + lsb, field.lo + lsb)}`;
  if (field.access) text += `，${field.access}`;
  if (field.value) text += `，取值 ${field.value}`;
  if (field.desc) text += `。${plain(field.desc)}`;
  return text;
}

function packetTitle(field: Field, unit: string): string {
  let text = `${field.name}：${sizeText(field, unit)}，位偏移 ${offsetText(field.start, field.end)}`;
  if (field.value) text += `，取值 ${field.value}`;
  if (field.desc) text += `。${plain(field.desc)}`;
  return text;
}

function frameTitle(field: Field, unit: string): string {
  let text = `${field.name}：${sizeText(field, unit)}`;
  if (field.value) text += `，取值 ${field.value}`;
  if (field.desc) text += `。${plain(field.desc)}`;
  return text;
}

function registerRows(
  fields: Field[],
  width: number,
  lane: number,
  pxPerBit: number,
  lsb: number,
) {
  const rows: Row[] = [];
  for (let r = Math.ceil(width / lane) - 1; r >= 0; r -= 1) {
    const rowLo = r * lane;
    const rowHi = rowLo + lane - 1;
    const numbers: BitNumber[] = [];
    for (let column = 1; column <= lane; column += 1) {
      const bit = rowHi - (column - 1);
      if (bit < width) {
        numbers.push({ key: `n${bit}`, column, text: String(bit + lsb) });
      }
    }
    const cells: Cell[] = [];
    const access: Row["access"] = [];
    for (const field of fields) {
      const hi = Math.min(field.hi, rowHi);
      const lo = Math.max(field.lo, rowLo);
      if (hi < lo) continue;
      const span = hi - lo + 1;
      const column = `${rowHi - hi + 1} / span ${span}`;
      cells.push(
        makeCell(
          field,
          `${field.key}-${r}`,
          column,
          span,
          pxPerBit,
          field.hi > rowHi,
          field.lo < rowLo,
          registerTitle(field, lsb),
        ),
      );
      if (field.access) {
        access.push({ key: `${field.key}-a${r}`, column, text: field.access });
      }
    }
    rows.push({ key: `r${r}`, offset: "", numbers, cells, access });
  }
  return rows;
}

function packetLayout(
  key: Layout["key"],
  fields: Field[],
  total: number,
  lane: number,
  unit: string,
  pxPerBit: number,
): Layout {
  const gutter = props.offsets && lane % 8 === 0;
  const shift = gutter ? 1 : 0;
  const head: BitNumber[] = [];
  for (let bit = 0; bit < lane; bit += 1) {
    head.push({ key: `h${bit}`, column: bit + 1 + shift, text: String(bit) });
  }
  const rows: Row[] = [];
  const rowCount = Math.max(1, Math.ceil(total / lane));
  for (let r = 0; r < rowCount; r += 1) {
    const rowStart = r * lane;
    const rowEnd = rowStart + lane - 1;
    const cells: Cell[] = [];
    for (const field of fields) {
      const start = Math.max(field.start, rowStart);
      const end = Math.min(field.end, rowEnd);
      if (start > end) continue;
      const span = end - start + 1;
      cells.push(
        makeCell(
          field,
          `${field.key}-${r}`,
          `${start - rowStart + 1 + shift} / span ${span}`,
          span,
          pxPerBit,
          field.start < rowStart,
          field.end > rowEnd,
          packetTitle(field, unit),
        ),
      );
    }
    rows.push({
      key: `r${r}`,
      offset: gutter ? `+${rowStart / 8}` : "",
      numbers: [],
      cells,
      access: [],
    });
  }
  return { key, lane, gutter, head, rows };
}

function buildRegister(input: Field[], problems: string[]): Model {
  const lsbValue = toNumber(props.lsb);
  const lsb = Number.isInteger(lsbValue) && lsbValue >= 0 ? lsbValue : 0;
  if (lsb !== lsbValue) problems.push("lsb 无效，按 0 处理。");
  // range 写的是真实位号，换算成相对于 lsb 的位置
  const fields = input.map((field) =>
    field.range && lsb > 0
      ? {
          ...field,
          range: [field.range[0] - lsb, field.range[1] - lsb] as [number, number],
        }
      : field,
  );
  const ranged = fields.filter((field) => field.range !== null);
  const sum = fields.reduce((total, field) => total + field.bits, 0);
  let width = positiveInt(props.width);
  if (props.width !== undefined && width === null) {
    problems.push("width 无效，已按字段自动推算。");
  }
  if (width === null) {
    if (ranged.length > 0) {
      const need = Math.max(
        ...ranged.map((field) => (field.range as [number, number])[0] + 1),
      );
      width = lsb > 0 ? need : ([8, 16, 32, 64].find((item) => item >= need) ?? need);
    } else {
      width = Math.max(sum, 1);
    }
  }

  let cursor = width - 1;
  const placed: Field[] = [];
  fields.forEach((field, index) => {
    let hi = cursor;
    let lo = cursor - field.bits + 1;
    if (field.range) [hi, lo] = field.range;
    cursor = lo - 1;
    if (hi > width - 1 || lo < 0) {
      problems.push(
        `${fieldLabel(field, index)}（位 ${bitsText(hi + lsb, lo + lsb)}）超出图中的位 ${bitsText(width - 1 + lsb, lsb)}。`,
      );
      hi = Math.min(hi, width - 1);
      lo = Math.max(lo, 0);
      if (hi < lo) return;
    }
    placed.push({ ...field, hi, lo });
  });
  if (ranged.length === 0 && props.width !== undefined && sum < width) {
    problems.push(
      `字段位宽合计 ${sum} 位，少于 width 的 ${width} 位，空缺部分按保留位绘制。`,
    );
  }

  placed.sort((a, b) => b.hi - a.hi);
  const drawn: Field[] = [];
  let expected = width - 1;
  for (const field of placed) {
    if (field.hi > expected) {
      problems.push(
        `${fieldLabel(field)}（位 ${bitsText(field.hi + lsb, field.lo + lsb)}）与其他字段重叠。`,
      );
    } else if (field.hi < expected) {
      drawn.push(reservedGap(expected, field.hi + 1));
    }
    drawn.push(field);
    expected = Math.min(expected, field.lo - 1);
  }
  if (expected >= 0) drawn.push(reservedGap(expected, 0));

  const laneInput = positiveInt(props.lane);
  if (props.lane !== undefined && laneInput === null) problems.push("lane 无效。");
  const lane = Math.min(laneInput ?? 16, width);
  const narrow = narrowLaneFor(lane);

  const layouts: Layout[] = [
    {
      key: "wide",
      lane,
      gutter: false,
      head: [],
      rows: registerRows(drawn, width, lane, WIDE_PX / lane, lsb),
    },
  ];
  if (narrow !== null) {
    layouts.push({
      key: "narrow",
      lane: narrow,
      gutter: false,
      head: [],
      rows: registerRows(drawn, width, narrow, NARROW_PX / narrow, lsb),
    });
  }

  const listed = tableFields(placed.filter((field) => !field.auto));
  const hasAccess = listed.some((field) => field.access);
  const hasValue = listed.some((field) => field.value);
  const head = ["位", "字段"];
  if (hasAccess) head.push("访问");
  if (hasValue) head.push("取值");
  head.push("说明");
  const rows = listed.map((field) => {
    const cells: TableCell[] = [
      {
        key: "bits",
        mono: true,
        parts: [{ code: false, text: bitsText(field.hi + lsb, field.lo + lsb) }],
      },
      { key: "name", mono: true, parts: [{ code: false, text: field.name }] },
    ];
    if (hasAccess) {
      cells.push({
        key: "access",
        mono: true,
        parts: [{ code: false, text: field.access }],
      });
    }
    if (hasValue) {
      cells.push({
        key: "value",
        mono: true,
        parts: [{ code: false, text: field.value }],
      });
    }
    cells.push({ key: "desc", mono: false, parts: inlineParts(field.desc) });
    return { key: field.key, cells };
  });

  return {
    mode: "register",
    switchClass: narrow !== null ? switchClass(lane * 25) : "",
    layouts,
    groups: [],
    hasGroups: false,
    table: wantsTable(listed) && rows.length > 0 ? { head, rows } : null,
    problems,
  };
}

function buildPacket(fields: Field[], problems: string[]): Model {
  let offset = 0;
  const placed = fields.map((field) => {
    const start = offset;
    offset += field.bits;
    return { ...field, start, end: offset - 1 };
  });

  const laneInput = positiveInt(props.lane);
  if (props.lane !== undefined && laneInput === null) problems.push("lane 无效。");
  const lane = laneInput ?? 32;
  const narrow = narrowLaneFor(lane);
  const unit = props.unit;

  const layouts = [
    packetLayout("wide", placed, offset, lane, unit, (WIDE_PX - GUTTER_PX) / lane),
  ];
  if (narrow !== null) {
    layouts.push(
      packetLayout(
        "narrow",
        placed,
        offset,
        narrow,
        unit,
        (NARROW_PX - GUTTER_PX) / narrow,
      ),
    );
  }

  const listed = tableFields(placed);
  const hasValue = listed.some((field) => field.value);
  const head = ["位偏移", "字段", "长度"];
  if (hasValue) head.push("取值");
  head.push("说明");
  const rows = listed.map((field) => {
    const cells: TableCell[] = [
      {
        key: "offset",
        mono: true,
        parts: [
          {
            code: false,
            text: field.variable
              ? `${field.start} 起`
              : offsetText(field.start, field.end),
          },
        ],
      },
      { key: "name", mono: false, parts: [{ code: false, text: field.name }] },
      {
        key: "size",
        mono: false,
        parts: [{ code: false, text: sizeText(field, unit) }],
      },
    ];
    if (hasValue) {
      cells.push({
        key: "value",
        mono: true,
        parts: [{ code: false, text: field.value }],
      });
    }
    cells.push({ key: "desc", mono: false, parts: inlineParts(field.desc) });
    return { key: field.key, cells };
  });

  return {
    mode: "packet",
    switchClass: narrow !== null ? switchClass(lane * 18 + GUTTER_PX) : "",
    layouts,
    groups: [],
    hasGroups: false,
    table: wantsTable(placed) && rows.length > 0 ? { head, rows } : null,
    problems,
  };
}

function buildFrame(fields: Field[], problems: string[]): Model {
  const unit = props.unit;
  const groups: FrameGroup[] = [];
  for (const field of fields) {
    const item: FrameItem = {
      key: field.key,
      name: field.name,
      nameParts: breakParts(field.name),
      value: field.value,
      size: sizeText(field, unit),
      title: frameTitle(field, unit),
      grow: Math.round(Math.sqrt(field.bits) * 100) / 100,
      minWidth: `${frameMinWidth(field)}px`,
      classes: cellClasses(field),
    };
    const last = groups[groups.length - 1];
    if (field.group && last && last.label === field.group) {
      last.items.push(item);
      last.grow = Math.round((last.grow + item.grow) * 100) / 100;
    } else {
      groups.push({
        key: `g${groups.length}`,
        label: field.group,
        grow: item.grow,
        items: [item],
      });
    }
  }

  const listed = tableFields(fields);
  const hasValue = listed.some((field) => field.value);
  const head = ["字段", "长度"];
  if (hasValue) head.push("取值");
  head.push("说明");
  const rows = listed.map((field) => {
    const cells: TableCell[] = [
      { key: "name", mono: false, parts: [{ code: false, text: field.name }] },
      {
        key: "size",
        mono: false,
        parts: [{ code: false, text: sizeText(field, unit) }],
      },
    ];
    if (hasValue) {
      cells.push({
        key: "value",
        mono: true,
        parts: [{ code: false, text: field.value }],
      });
    }
    cells.push({ key: "desc", mono: false, parts: inlineParts(field.desc) });
    return { key: field.key, cells };
  });

  return {
    mode: "frame",
    switchClass: "",
    layouts: [],
    groups,
    hasGroups: groups.some((group) => group.label),
    table: wantsTable(fields) && rows.length > 0 ? { head, rows } : null,
    problems,
  };
}

function wantsTable(fields: Field[]): boolean {
  if (props.table !== undefined) return props.table;
  return fields.some((field) => field.desc);
}

// 字段表默认只列写了 desc 的字段；显式 table 时列出全部字段
function tableFields<T extends Field>(fields: T[]): T[] {
  return props.table === true ? fields : fields.filter((field) => field.desc);
}

const model = computed<Model>(() => {
  const problems: string[] = [];
  const mode = normalizeMode(props.mode, problems);
  const fields = normalizeFields(props.fields, problems);
  if (fields.length === 0 && problems.length === 0) problems.push("fields 为空。");
  if (mode === "packet") return buildPacket(fields, problems);
  if (mode === "frame") return buildFrame(fields, problems);
  return buildRegister(fields, problems);
});
</script>

<template>
  <figure
    class="bit-field"
    :class="[`bf-mode-${model.mode}`, model.switchClass]"
  >
    <figcaption
      v-if="title"
      class="bf-title"
    >
      {{ title }}
    </figcaption>

    <template v-if="model.mode !== 'frame'">
      <div
        v-for="layout in model.layouts"
        :key="layout.key"
        class="bf-scroll"
        :class="`bf-layout-${layout.key}`"
      >
        <div
          class="bf-grid"
          :class="{ 'has-gutter': layout.gutter }"
          :style="{ '--bf-lane': layout.lane }"
        >
          <div
            v-if="layout.head.length"
            class="bf-row bf-head"
            aria-hidden="true"
          >
            <span
              v-for="number in layout.head"
              :key="number.key"
              class="bf-bitno"
              :style="{ gridColumn: number.column }"
              >{{ number.text }}</span
            >
          </div>
          <template
            v-for="(row, rowIndex) in layout.rows"
            :key="row.key"
          >
            <div
              v-if="row.numbers.length"
              class="bf-row bf-head"
              aria-hidden="true"
            >
              <span
                v-for="number in row.numbers"
                :key="number.key"
                class="bf-bitno"
                :style="{ gridColumn: number.column }"
                >{{ number.text }}</span
              >
            </div>
            <div
              class="bf-row bf-cells"
              :class="{ 'is-first': rowIndex === 0 }"
            >
              <span
                v-if="row.offset"
                class="bf-offset"
                aria-hidden="true"
                >{{ row.offset }}</span
              >
              <div
                v-for="cell in row.cells"
                :key="cell.key"
                class="bf-cell"
                :class="cell.classes"
                :style="{ gridColumn: cell.column }"
                :title="cell.title"
              >
                <span
                  class="bf-name"
                  :class="{ 'is-vertical': cell.vertical }"
                  ><template
                    v-for="(part, index) in cell.nameParts"
                    :key="index"
                    ><wbr v-if="index" />{{ part }}</template
                  ></span
                >
                <span
                  v-if="cell.value"
                  class="bf-value"
                  >{{ cell.value }}</span
                >
              </div>
            </div>
            <div
              v-if="row.access.length"
              class="bf-row bf-access"
              aria-hidden="true"
            >
              <span
                v-for="item in row.access"
                :key="item.key"
                :style="{ gridColumn: item.column }"
                >{{ item.text }}</span
              >
            </div>
          </template>
        </div>
      </div>
    </template>

    <div
      v-else
      class="bf-frame"
    >
      <div
        v-for="group in model.groups"
        :key="group.key"
        class="bf-group"
        :style="{ flexGrow: group.grow }"
      >
        <div
          v-if="model.hasGroups"
          class="bf-group-label"
          :class="{ 'is-empty': !group.label }"
        >
          {{ group.label }}
        </div>
        <div class="bf-group-items">
          <div
            v-for="item in group.items"
            :key="item.key"
            class="bf-item"
            :style="{ flexGrow: item.grow, minWidth: item.minWidth }"
          >
            <div
              class="bf-cell"
              :class="item.classes"
              :title="item.title"
            >
              <span class="bf-name"
                ><template
                  v-for="(part, index) in item.nameParts"
                  :key="index"
                  ><wbr v-if="index" />{{ part }}</template
                ></span
              >
              <span
                v-if="item.value"
                class="bf-value"
                >{{ item.value }}</span
              >
            </div>
            <span class="bf-size">{{ item.size }}</span>
          </div>
        </div>
      </div>
    </div>

    <table
      v-if="model.table"
      class="bf-table"
    >
      <thead>
        <tr>
          <th
            v-for="label in model.table.head"
            :key="label"
          >
            {{ label }}
          </th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="row in model.table.rows"
          :key="row.key"
        >
          <td
            v-for="cell in row.cells"
            :key="cell.key"
            :class="{ 'is-mono': cell.mono }"
          >
            <template
              v-for="(part, index) in cell.parts"
              :key="index"
              ><code v-if="part.code">{{ part.text }}</code
              ><template v-else>{{ part.text }}</template></template
            >
          </td>
        </tr>
      </tbody>
    </table>

    <ul
      v-if="model.problems.length"
      class="bf-problems"
      role="note"
    >
      <li
        v-for="(problem, index) in model.problems"
        :key="index"
      >
        位域图：{{ problem }}
      </li>
    </ul>
  </figure>
</template>

<style scoped>
.bit-field {
  --bf-line: color-mix(in srgb, var(--vp-c-text-2) 55%, transparent);
  --bf-hatch: color-mix(in srgb, var(--vp-c-text-3) 45%, transparent);
  --bf-gutter: 0px;
  container-type: inline-size;
  margin: 1.25rem 0;
  color: var(--vp-c-text-1);
  font-size: 0.8125rem;
  line-height: 1.35;
}

.bf-title {
  margin-bottom: 0.5rem;
  color: var(--vp-c-text-1);
  font-size: 0.875rem;
  font-weight: 600;
  text-align: center;
}

.bf-scroll {
  overflow-x: auto;
  padding-bottom: 2px;
}

.bf-layout-narrow {
  display: none;
}

.bf-grid {
  min-width: calc(var(--bf-lane) * 1.0625rem + var(--bf-gutter));
  padding-left: 1px;
}

.bf-grid.has-gutter {
  --bf-gutter: 2.25rem;
}

.bf-row {
  display: grid;
  grid-template-columns: repeat(var(--bf-lane), minmax(0, 1fr));
}

.bf-grid.has-gutter .bf-row {
  grid-template-columns: var(--bf-gutter) repeat(var(--bf-lane), minmax(0, 1fr));
}

.bf-bitno {
  color: var(--vp-c-text-2);
  font-family: var(--vp-font-family-mono);
  font-size: 0.6875rem;
  font-variant-numeric: tabular-nums;
  line-height: 1.5rem;
  text-align: center;
}

.bf-mode-register .bf-head:not(:first-child) {
  margin-top: 0.625rem;
}

.bf-cell {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 0.125rem;
  min-width: 0;
  min-height: 2.75rem;
  margin-left: -1px;
  padding: 0.3rem 0.2rem;
  border: 1px solid var(--bf-line);
  background-color: var(--bf-fill, transparent);
  text-align: center;
}

.bf-mode-packet .bf-cells:not(.is-first) .bf-cell {
  margin-top: -1px;
}

.bf-cell.tone-brand {
  --bf-fill: var(--vp-c-brand-soft);
}

.bf-cell.tone-green {
  --bf-fill: var(--vp-c-green-soft);
}

.bf-cell.tone-yellow {
  --bf-fill: var(--vp-c-yellow-soft);
}

.bf-cell.tone-red {
  --bf-fill: var(--vp-c-red-soft);
}

.bf-cell.tone-purple {
  --bf-fill: var(--vp-c-purple-soft);
}

.bf-cell.tone-gray {
  --bf-fill: var(--vp-c-gray-soft);
}

.bf-cell.is-reserved {
  color: var(--vp-c-text-3);
  background-image: repeating-linear-gradient(
    -45deg,
    var(--bf-hatch) 0 1px,
    transparent 1px 6px
  );
}

.bf-cell.is-variable {
  border-style: dashed;
}

.bf-cell.cont-left {
  border-left-style: dashed;
}

.bf-cell.cont-right {
  border-right-style: dashed;
}

.bf-name {
  max-width: 100%;
  font-weight: 500;
  overflow-wrap: anywhere;
}

.bf-name.is-vertical {
  line-height: 1.1;
  text-orientation: mixed;
  writing-mode: vertical-rl;
}

.bf-value {
  max-width: 100%;
  color: var(--vp-c-brand-1);
  font-family: var(--vp-font-family-mono);
  font-size: 0.75rem;
  overflow-wrap: anywhere;
}

.bf-offset {
  grid-column: 1;
  align-self: center;
  padding-right: 0.5rem;
  color: var(--vp-c-text-2);
  font-family: var(--vp-font-family-mono);
  font-size: 0.6875rem;
  text-align: right;
}

.bf-access span {
  color: var(--vp-c-text-2);
  font-family: var(--vp-font-family-mono);
  font-size: 0.6875rem;
  line-height: 1.25rem;
  text-align: center;
}

.bf-frame {
  display: flex;
  flex-wrap: wrap;
  row-gap: 0.75rem;
  padding-left: 1px;
}

.bf-group {
  display: flex;
  flex: 1 1 0;
  flex-direction: column;
}

.bf-group-label {
  position: relative;
  margin: 0 0.375rem 0.5rem;
  padding: 0 0.25rem 0.25rem;
  border-bottom: 1px solid var(--vp-c-text-3);
  color: var(--vp-c-text-2);
  font-size: 0.75rem;
  line-height: 1.25rem;
  text-align: center;
  white-space: nowrap;
}

.bf-group-label::before,
.bf-group-label::after {
  position: absolute;
  bottom: -0.375rem;
  width: 1px;
  height: 0.375rem;
  background-color: var(--vp-c-text-3);
  content: "";
}

.bf-group-label::before {
  left: 0;
}

.bf-group-label::after {
  right: 0;
}

.bf-group-label.is-empty {
  border-bottom-color: transparent;
}

.bf-group-label.is-empty::before,
.bf-group-label.is-empty::after {
  display: none;
}

.bf-group-items {
  display: flex;
  flex: 1;
}

.bf-item {
  display: flex;
  flex: 1 1 0;
  flex-direction: column;
  min-width: 3.5rem;
  margin-left: -1px;
}

.bf-item .bf-cell {
  flex: 1;
  min-height: 3rem;
  margin-left: 0;
}

.bf-size {
  padding: 0 0.125rem;
  color: var(--vp-c-text-2);
  font-size: 0.6875rem;
  line-height: 1.5rem;
  text-align: center;
}

.bf-table {
  margin: 0.75rem 0 0;
}

.bf-table th,
.bf-table td {
  padding: 0.375rem 0.75rem;
  font-size: 0.8125rem;
}

.bf-table th {
  white-space: nowrap;
}

.bf-table td:not(:last-child) {
  white-space: nowrap;
}

.bf-table td.is-mono {
  font-family: var(--vp-font-family-mono);
}

.bf-problems {
  margin: 0.5rem 0 0;
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  background-color: var(--vp-c-danger-soft);
  color: var(--vp-c-danger-1);
  font-size: 0.8125rem;
  list-style: none;
}

.bf-problems li + li {
  margin-top: 0.25rem;
}

@container (max-width: 319.98px) {
  .bf-sw-320 .bf-layout-wide {
    display: none;
  }

  .bf-sw-320 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 359.98px) {
  .bf-sw-360 .bf-layout-wide {
    display: none;
  }

  .bf-sw-360 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 399.98px) {
  .bf-sw-400 .bf-layout-wide {
    display: none;
  }

  .bf-sw-400 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 439.98px) {
  .bf-sw-440 .bf-layout-wide {
    display: none;
  }

  .bf-sw-440 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 479.98px) {
  .bf-sw-480 .bf-layout-wide {
    display: none;
  }

  .bf-sw-480 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 519.98px) {
  .bf-sw-520 .bf-layout-wide {
    display: none;
  }

  .bf-sw-520 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 559.98px) {
  .bf-sw-560 .bf-layout-wide {
    display: none;
  }

  .bf-sw-560 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 599.98px) {
  .bf-sw-600 .bf-layout-wide {
    display: none;
  }

  .bf-sw-600 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 639.98px) {
  .bf-sw-640 .bf-layout-wide {
    display: none;
  }

  .bf-sw-640 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 719.98px) {
  .bf-sw-720 .bf-layout-wide {
    display: none;
  }

  .bf-sw-720 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 799.98px) {
  .bf-sw-800 .bf-layout-wide {
    display: none;
  }

  .bf-sw-800 .bf-layout-narrow {
    display: block;
  }
}

@container (max-width: 959.98px) {
  .bf-sw-960 .bf-layout-wide {
    display: none;
  }

  .bf-sw-960 .bf-layout-narrow {
    display: block;
  }
}
</style>
