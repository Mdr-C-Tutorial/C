<script setup>
import { computed, nextTick, onBeforeUnmount, ref, watch } from "vue";

const props = defineProps({
  title: {
    type: String,
    default: "main.c",
  },
  language: {
    type: String,
    default: "c",
  },
  outputTitle: {
    type: String,
    default: "zsh - output",
  },
  code: {
    type: String,
    default: "",
  },
  steps: {
    type: Array,
    default: () => [],
  },
  autoInterval: {
    type: Number,
    default: 1000,
  },
  startStep: {
    type: Number,
    default: 0,
  },
  editorHeight: {
    type: Number,
    default: 260,
  },
});

const codeText = ref("");
const lineList = computed(() => codeText.value.split("\n"));
const stepList = computed(() => (Array.isArray(props.steps) ? props.steps : []));
const maxStepIndex = computed(() => Math.max(0, stepList.value.length - 1));
const editorHeightPx = computed(() => `${Math.max(160, props.editorHeight)}px`);

const stepIndex = ref(0);
const running = ref(false);
let timer = null;

const inputRef = ref(null);
const overlayRef = ref(null);
const gutterRef = ref(null);

const clampStep = (value) => {
  if (Number.isNaN(value)) {
    return 0;
  }
  if (value < 0) {
    return 0;
  }
  if (value > maxStepIndex.value) {
    return maxStepIndex.value;
  }
  return value;
};

const stopAuto = () => {
  running.value = false;
  if (timer !== null) {
    clearInterval(timer);
    timer = null;
  }
};

const syncScroll = () => {
  const input = inputRef.value;
  if (input == null) {
    return;
  }
  if (overlayRef.value != null) {
    overlayRef.value.scrollTop = input.scrollTop;
    overlayRef.value.scrollLeft = input.scrollLeft;
  }
  if (gutterRef.value != null) {
    gutterRef.value.scrollTop = input.scrollTop;
  }
};

const insertTab = (event) => {
  const el = event.target;
  if (el == null) {
    return;
  }
  const start = el.selectionStart;
  const end = el.selectionEnd;
  const current = codeText.value;
  codeText.value = `${current.slice(0, start)}    ${current.slice(end)}`;
  nextTick(() => {
    if (inputRef.value == null) {
      return;
    }
    inputRef.value.selectionStart = start + 4;
    inputRef.value.selectionEnd = start + 4;
  });
};

const moveTo = (next) => {
  stepIndex.value = clampStep(next);
};

const nextStep = () => {
  if (stepIndex.value >= maxStepIndex.value) {
    stopAuto();
    return;
  }
  stepIndex.value += 1;
};

const prevStep = () => {
  moveTo(stepIndex.value - 1);
};

const resetStep = () => {
  stopAuto();
  moveTo(props.startStep);
};

const toggleAuto = () => {
  if (running.value) {
    stopAuto();
    return;
  }
  if (stepIndex.value >= maxStepIndex.value) {
    moveTo(0);
  }
  running.value = true;
  timer = setInterval(() => {
    nextStep();
  }, Math.max(120, props.autoInterval));
};

watch(
  () => [props.startStep, stepList.value.length],
  () => {
    moveTo(props.startStep);
    stopAuto();
  },
  { immediate: true }
);

watch(
  () => props.code,
  (value) => {
    codeText.value = typeof value === "string" ? value : "";
    nextTick(() => {
      syncScroll();
    });
  },
  { immediate: true }
);

watch(
  () => props.autoInterval,
  () => {
    if (!running.value) {
      return;
    }
    stopAuto();
    toggleAuto();
  }
);

onBeforeUnmount(() => {
  stopAuto();
});

const currentStep = computed(() => stepList.value[stepIndex.value] ?? {});

const normalizeLineList = (value) => {
  if (typeof value === "number") {
    return [value];
  }
  if (Array.isArray(value)) {
    return value.filter((item) => Number.isInteger(item) && item >= 1);
  }
  return [];
};

const activeLineSet = computed(() => {
  const current = currentStep.value;
  const raw = current.lines ?? current.line ?? [];
  return new Set(normalizeLineList(raw));
});

const normalizeRanges = (value) => {
  if (!Array.isArray(value)) {
    return [];
  }
  return value
    .map((item) => {
      if (!item || typeof item !== "object") {
        return null;
      }
      const line = Number(item.line);
      const start = Number(item.start);
      const end = Number(item.end);
      if (!Number.isInteger(line) || line < 1) {
        return null;
      }
      if (!Number.isInteger(start) || start < 1) {
        return null;
      }
      return {
        line,
        start,
        end: Number.isInteger(end) && end >= start ? end : start,
      };
    })
    .filter((item) => item !== null);
};

const rangesByLine = computed(() => {
  const map = new Map();
  const list = normalizeRanges(currentStep.value.ranges);
  for (const range of list) {
    if (!map.has(range.line)) {
      map.set(range.line, []);
    }
    map.get(range.line).push(range);
  }
  for (const [line, items] of map.entries()) {
    const sorted = items.sort((a, b) => a.start - b.start);
    map.set(line, sorted);
  }
  return map;
});

const splitLine = (content, lineNo) => {
  const ranges = rangesByLine.value.get(lineNo) ?? [];
  if (ranges.length === 0) {
    return [{ text: content, active: false }];
  }
  const parts = [];
  let cursor = 0;
  for (const range of ranges) {
    const start = Math.max(0, Math.min(content.length, range.start - 1));
    const end = Math.max(start, Math.min(content.length, range.end));
    if (start > cursor) {
      parts.push({ text: content.slice(cursor, start), active: false });
    }
    if (end > start) {
      parts.push({ text: content.slice(start, end), active: true });
    }
    cursor = Math.max(cursor, end);
  }
  if (cursor < content.length) {
    parts.push({ text: content.slice(cursor), active: false });
  }
  if (parts.length === 0) {
    parts.push({ text: content, active: false });
  }
  return parts;
};

const segmentedLines = computed(() =>
  lineList.value.map((line, idx) => splitLine(line, idx + 1))
);

const normalizeOutput = (value) => {
  if (value === null || value === undefined) {
    return [];
  }
  if (Array.isArray(value)) {
    return value.flatMap((item) => normalizeOutput(item));
  }
  return String(value).split("\n");
};

const outputText = computed(() => {
  let outputLines = [];
  for (let i = 0; i <= stepIndex.value; ++i) {
    const step = stepList.value[i];
    if (!step) {
      continue;
    }
    if (Object.prototype.hasOwnProperty.call(step, "output")) {
      outputLines = normalizeOutput(step.output);
    }
    if (Object.prototype.hasOwnProperty.call(step, "appendOutput")) {
      outputLines.push(...normalizeOutput(step.appendOutput));
    }
  }
  return outputLines.join("\n");
});

const stepNote = computed(() => {
  const note = currentStep.value.note;
  if (typeof note === "string") {
    return note;
  }
  return "";
});
</script>

<template>
  <div class="code-runner">
    <div class="editor-shell">
      <div class="editor-header">
        <div class="header-buttons">
          <span class="btn red"></span>
          <span class="btn yellow"></span>
          <span class="btn green"></span>
        </div>
        <div class="header-title">{{ title }}</div>
        <div class="header-lang">{{ language }}</div>
      </div>

      <div class="editor-body" :style="{ height: editorHeightPx }">
        <div ref="gutterRef" class="editor-gutter" aria-hidden="true">
          <div
            v-for="(_, idx) in lineList"
            :key="`line-no-${idx}`"
            class="line-no"
            :class="{ 'is-active-line-no': activeLineSet.has(idx + 1) }"
          >
            {{ idx + 1 }}
          </div>
        </div>

        <div class="editor-main">
          <div ref="overlayRef" class="highlight-layer" aria-hidden="true">
            <div
              v-for="(line, idx) in lineList"
              :key="`line-highlight-${idx}`"
              class="editor-line"
              :class="{ 'is-active-line': activeLineSet.has(idx + 1) }"
            >
              <span class="line-code">
                <template
                  v-for="(part, partIdx) in segmentedLines[idx]"
                  :key="`part-${idx}-${partIdx}`"
                >
                  <span
                    :class="{ 'is-active-range': part.active }"
                    class="code-part"
                  >{{ part.text || " " }}</span>
                </template>
              </span>
            </div>
          </div>

          <textarea
            ref="inputRef"
            v-model="codeText"
            class="editor-input"
            spellcheck="false"
            wrap="off"
            @scroll="syncScroll"
            @keydown.tab.prevent="insertTab"
          />
        </div>
      </div>
    </div>

    <div class="runner-toolbar">
      <button type="button" class="runner-btn" @click="prevStep">Prev</button>
      <button type="button" class="runner-btn" @click="nextStep">Next</button>
      <button type="button" class="runner-btn" @click="toggleAuto">
        {{ running ? "Pause" : "Auto Run" }}
      </button>
      <button type="button" class="runner-btn" @click="resetStep">Reset</button>
      <span class="runner-progress">Step {{ stepIndex + 1 }} / {{ stepList.length || 1 }}</span>
    </div>

    <p v-if="stepNote" class="step-note">{{ stepNote }}</p>

    <div class="output-shell">
      <div class="output-header">
        <div class="header-buttons">
          <span class="btn red"></span>
          <span class="btn yellow"></span>
          <span class="btn green"></span>
        </div>
        <div class="header-title">{{ outputTitle }}</div>
      </div>
      <pre class="output-body">{{ outputText }}</pre>
    </div>
  </div>
</template>

<style scoped>
.code-runner {
  margin: 1rem 0;
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  overflow: hidden;
  background: var(--vp-c-bg);
}

.editor-shell,
.output-shell {
  border-bottom: 1px solid var(--vp-c-divider);
}

.editor-header,
.output-header {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  min-height: 40px;
  padding: 0.5rem 0.75rem;
  background: var(--vp-c-bg-soft);
  border-bottom: 1px solid var(--vp-c-divider);
}

.header-buttons {
  display: inline-flex;
  gap: 6px;
}

.btn {
  width: 10px;
  height: 10px;
  border-radius: 50%;
}

.red {
  background: #ff5f56;
}

.yellow {
  background: #ffbd2e;
}

.green {
  background: #27c93f;
}

.header-title {
  flex: 1;
  font-size: 0.84rem;
  color: var(--vp-c-text-2);
  text-align: center;
}

.header-lang {
  font-size: 0.74rem;
  color: var(--vp-c-text-3);
  text-transform: uppercase;
}

.editor-body {
  display: flex;
  position: relative;
  font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
  background: var(--vp-code-block-bg);
  line-height: 1.75rem;
  tab-size: 4;
}

.editor-gutter {
  flex: 0 0 3.2rem;
  border-right: 1px solid var(--vp-c-divider);
  background: color-mix(in srgb, var(--vp-c-default-soft) 60%, transparent);
  overflow: hidden;
}

.editor-main {
  position: relative;
  flex: 1;
  min-width: 0;
}

.highlight-layer {
  position: absolute;
  inset: 0;
  overflow: auto;
  pointer-events: none;
  z-index: 1;
}

.editor-line.is-active-line {
  background: color-mix(in srgb, var(--vp-c-brand-1) 12%, transparent);
}

.line-no {
  height: 1.75rem;
  line-height: 1.75rem;
  display: block;
  text-align: right;
  padding-right: 0.75rem;
  color: var(--vp-c-text-3);
  user-select: none;
  font-size: 0.84rem;
}

.line-no.is-active-line-no {
  color: var(--vp-c-brand-1);
  background: color-mix(in srgb, var(--vp-c-brand-1) 10%, transparent);
}

.editor-line {
  min-height: 1.75rem;
  line-height: 1.75rem;
}

.line-code {
  display: block;
  white-space: pre;
  padding: 0 0.75rem;
  color: transparent;
}

.code-part {
  white-space: pre;
}

.is-active-range {
  background: color-mix(in srgb, var(--vp-c-warning-1) 25%, transparent);
  border-radius: 4px;
}

.editor-input {
  position: absolute;
  inset: 0;
  z-index: 2;
  width: 100%;
  height: 100%;
  border: none;
  outline: none;
  resize: none;
  overflow: auto;
  background: transparent;
  color: var(--vp-c-text-1);
  font: inherit;
  line-height: 1.75rem;
  padding: 0 0.75rem;
  white-space: pre;
  caret-color: var(--vp-c-brand-1);
  tab-size: 4;
}

.runner-toolbar {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  flex-wrap: wrap;
  padding: 0.75rem;
  background: var(--vp-c-bg-soft);
  border-bottom: 1px solid var(--vp-c-divider);
}

.runner-btn {
  border: 1px solid var(--vp-c-border);
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  border-radius: 8px;
  font-size: 0.82rem;
  padding: 0.35rem 0.7rem;
  cursor: pointer;
}

.runner-btn:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}

.runner-progress {
  margin-left: auto;
  font-size: 0.82rem;
  color: var(--vp-c-text-2);
}

.step-note {
  margin: 0;
  padding: 0.7rem 0.9rem;
  color: var(--vp-c-text-2);
  border-bottom: 1px solid var(--vp-c-divider);
  background: var(--vp-c-default-soft);
}

.output-body {
  margin: 0;
  padding: 0.9rem;
  min-height: 92px;
  line-height: 1.5;
  font-size: 0.86rem;
  color: #e5e7eb;
  background: #111827;
  white-space: pre-wrap;
  font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
}

@media (max-width: 768px) {
  .runner-progress {
    width: 100%;
    margin-left: 0;
  }
}
</style>
