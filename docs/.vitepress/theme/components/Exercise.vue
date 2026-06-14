<script setup lang="ts">
import { computed, nextTick, onMounted, onUpdated, ref } from "vue";
import { withBase } from "vitepress";
import { getExerciseSolutionHref } from "../exerciseLinks";

const props = defineProps({
  id: { type: String, required: true },
  d: { type: Number, required: true }, // Difficulty (0-9)
  w: { type: Number, required: true }, // Workload (0-9)
  math: { type: String, default: "" }, // '', 'M', 'HM'
  scope: { type: String, default: "" }, // '', '*', '**'
  multi: { type: Boolean, default: false },
});

const cardRef = ref<HTMLElement | null>(null);
const questionSummary = ref<{
  minD: number;
  maxD: number;
  minW: number;
  maxW: number;
} | null>(null);

function difficultyColorFor(d: number) {
  if (d <= 2) return "green";
  if (d <= 5) return "blue";
  if (d <= 7) return "orange";
  return "red";
}

const difficultyColor = computed(() => {
  const d =
    props.multi && questionSummary.value !== null
      ? questionSummary.value.maxD
      : props.d;
  return difficultyColorFor(d);
});

function estimatedTimeText(w: number) {
  const mins = Math.pow(3, w);
  if (mins < 60) return `${mins} min`;
  return `${(mins / 60).toFixed(1)} h`;
}

const estimatedTime = computed(() => estimatedTimeText(props.w));
const headerDifficultyText = computed(() => {
  if (!props.multi || questionSummary.value === null) return String(props.d);
  const { minD, maxD } = questionSummary.value;
  return minD === maxD ? String(maxD) : `${minD}-${maxD}`;
});
const headerWorkloadText = computed(() => {
  if (!props.multi || questionSummary.value === null) return String(props.w);
  const { minW, maxW } = questionSummary.value;
  return minW === maxW ? String(maxW) : `${minW}-${maxW}`;
});
const headerDifficultyTitle = computed(() =>
  props.multi && questionSummary.value !== null
    ? `难度等级范围: ${headerDifficultyText.value}`
    : `难度等级: ${props.d}`,
);
const headerWorkloadTitle = computed(() =>
  props.multi && questionSummary.value !== null
    ? `工作量范围: ${headerWorkloadText.value}`
    : `预估耗时: ${estimatedTime.value}`,
);

const isOptional = computed(() => props.scope === "*");
const isExtension = computed(() => props.scope === "**");
const solutionHref = computed(() =>
  getExerciseSolutionHref(props.id)
    ? withBase(getExerciseSolutionHref(props.id))
    : "",
);

type MarkerMeta = {
  d: number;
  w: number;
  math: string;
  scope: string;
};

function parseMarker(text: string): MarkerMeta | null {
  const match = /^\[(HM|M)?(\d+)\.(\d+)(\*{0,2})\]$/.exec(text.trim());
  if (!match) return null;

  return {
    math: match[1] ?? "",
    d: Number(match[2]),
    w: Number(match[3]),
    scope: match[4] ?? "",
  };
}

function findMarkerElement(root: Element): HTMLElement | null {
  return (
    Array.from(root.querySelectorAll("em")).find((item) =>
      Boolean(parseMarker(item.textContent ?? "")),
    ) as HTMLElement | undefined
  ) ?? null;
}

function createBadge(text: string, className: string, title: string) {
  const badge = document.createElement("span");
  badge.className = `badge ${className}`;
  badge.textContent = text;
  badge.title = title;
  return badge;
}

function createQuestionBadges(meta: MarkerMeta) {
  const badges = document.createElement("span");
  badges.className = "question-badges";
  badges.append(
    createBadge(`⚡${meta.d}`, "diff", `难度等级: ${meta.d}`),
    createBadge(`⏳${meta.w}`, "work", `预估耗时: ${estimatedTimeText(meta.w)}`),
  );

  if (meta.math === "M") {
    badges.append(createBadge("∑", "math", "需要数学知识"));
  }
  if (meta.math === "HM") {
    badges.append(createBadge("∫", "math-hard", "需要高深数学知识"));
  }
  if (meta.scope === "*") {
    badges.append(createBadge("选做", "opt", "跳过不影响后续学习"));
  }
  if (meta.scope === "**") {
    badges.append(createBadge("拓展", "ext", "超纲/拓展内容"));
  }

  return badges;
}

function extractQuestionLabel(candidate: HTMLElement) {
  for (const child of Array.from(candidate.childNodes)) {
    if (child.nodeType !== Node.TEXT_NODE) continue;

    const text = child.textContent ?? "";
    const match = /^(\s*)((?:\(\d+\)|\d+\.))\s*/.exec(text);
    if (!match) continue;

    child.textContent = `${match[1]}${text.slice(match[0].length)}`;
    return {
      label: match[2],
      prefix: match[0].slice(match[1].length),
    };
  }

  return null;
}

function createQuestionHeader(label: string, badges: HTMLElement, hasDivider: boolean) {
  const header = document.createElement("div");
  header.className = "question-header";
  if (hasDivider) header.classList.add("has-divider");

  if (label) {
    const labelElement = document.createElement("span");
    labelElement.className = "question-label";
    labelElement.textContent = label;
    header.append(labelElement);
  }

  header.append(badges);
  return header;
}

function resetMultiQuestionContent(content: Element) {
  content.querySelectorAll(".question-header").forEach((item) => item.remove());
  content.querySelectorAll(".question-badges").forEach((item) => item.remove());
  content.querySelectorAll(".question-body").forEach((item) => {
    item.classList.remove("question-body");
  });
  content.querySelectorAll("[data-exercise-hidden-marker]").forEach((item) => {
    (item as HTMLElement).style.display = "";
    item.removeAttribute("data-exercise-hidden-marker");
  });
  content.querySelectorAll("[data-exercise-original-prefix]").forEach((item) => {
    const firstText = Array.from(item.childNodes).find(
      (child) => child.nodeType === Node.TEXT_NODE,
    );
    if (firstText) {
      firstText.textContent = `${(item as HTMLElement).dataset.exerciseOriginalPrefix}${firstText.textContent ?? ""}`;
    }
    item.removeAttribute("data-exercise-original-prefix");
  });
}

function directQuestionCandidates(content: Element) {
  const candidates: HTMLElement[] = [];

  Array.from(content.children).forEach((child) => {
    if (child.tagName === "OL" || child.tagName === "UL") {
      Array.from(child.children).forEach((item) => {
        if (item.tagName === "LI" && findMarkerElement(item)) {
          candidates.push(item as HTMLElement);
        }
      });
      return;
    }

    if (findMarkerElement(child)) {
      candidates.push(child as HTMLElement);
    }
  });

  return candidates;
}

async function enhanceMultiQuestions() {
  await nextTick();
  const content = cardRef.value?.querySelector(".ex-content");
  if (!content) return;

  resetMultiQuestionContent(content);
  if (!props.multi) {
    if (questionSummary.value !== null) {
      questionSummary.value = null;
    }
    return;
  }

  const metas: MarkerMeta[] = [];

  directQuestionCandidates(content).forEach((candidate, index) => {
    const marker = findMarkerElement(candidate);
    const meta = parseMarker(marker?.textContent ?? "");
    if (!marker || !meta) return;

    metas.push(meta);
    const labelInfo = extractQuestionLabel(candidate);
    if (labelInfo) {
      candidate.dataset.exerciseOriginalPrefix = labelInfo.prefix;
    }
    marker.style.display = "none";
    marker.setAttribute("data-exercise-hidden-marker", "true");
    candidate.classList.add("question-body");
    candidate.before(
      createQuestionHeader(
        labelInfo?.label ?? "",
        createQuestionBadges(meta),
        index > 0,
      ),
    );
  });

  const nextSummary =
    metas.length === 0
      ? null
      : {
          minD: Math.min(...metas.map((meta) => meta.d)),
          maxD: Math.max(...metas.map((meta) => meta.d)),
          minW: Math.min(...metas.map((meta) => meta.w)),
          maxW: Math.max(...metas.map((meta) => meta.w)),
        };

  if (JSON.stringify(questionSummary.value) !== JSON.stringify(nextSummary)) {
    questionSummary.value = nextSummary;
  }
}

onMounted(enhanceMultiQuestions);
onUpdated(enhanceMultiQuestions);
</script>

<template>
  <div
    ref="cardRef"
    :id="id"
    class="exercise-card"
    :class="[difficultyColor, { 'is-optional': isOptional, 'is-multi': multi }]"
  >
    <div class="ex-header">
      <a
        v-if="solutionHref"
        class="ex-id"
        :href="solutionHref"
        title="查看解析和答案"
      >
        #{{ id }}
      </a>
      <span
        v-else
        class="ex-id"
      >
        #{{ id }}
      </span>

      <div class="ex-badges">
        <span
          class="badge diff"
          :title="headerDifficultyTitle"
        >
          <span class="icon">⚡</span>{{ headerDifficultyText }}
        </span>

        <span
          class="badge work"
          :title="headerWorkloadTitle"
        >
          <span class="icon">⏳</span>{{ headerWorkloadText }}
        </span>

        <span
          v-if="math === 'M'"
          class="badge math"
          title="需要数学知识"
          >∑</span
        >
        <span
          v-if="math === 'HM'"
          class="badge math-hard"
          title="需要高深数学知识"
          >∫</span
        >

        <span
          v-if="isOptional"
          class="badge opt"
          title="跳过不影响后续学习"
          >选做</span
        >
        <span
          v-if="isExtension"
          class="badge ext"
          title="超纲/拓展内容"
          >拓展</span
        >
      </div>
    </div>

    <div class="ex-content">
      <slot></slot>
    </div>

    <div
      v-if="$slots.links"
      class="ex-footer"
    >
      <div class="links-container">
        <slot name="links"></slot>
      </div>
    </div>
  </div>
</template>

<style scoped>
.exercise-card {
  position: relative;
  margin: 1.5rem 0;
  padding: 1rem 1.2rem;
  border-radius: 8px;
  background-color: var(--vp-c-bg);
  border: 1px solid var(--vp-c-divider);
  border-left-width: 4px;
  transition:
    transform 0.2s ease,
    box-shadow 0.2s ease;
}

.exercise-card:hover {
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.05);
  transform: translateY(-1px);
}

.exercise-card.green {
  border-left-color: var(--vp-c-green-1);
}

.exercise-card.blue {
  border-left-color: var(--vp-c-indigo-1);
}

.exercise-card.orange {
  border-left-color: var(--vp-c-yellow-1);
}

.exercise-card.red {
  border-left-color: var(--vp-c-red-1);
}

.exercise-card.is-optional {
  background-color: transparent;
  border-style: dashed;
}

.ex-header {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 0.8rem;
  font-family: var(--vp-font-family-mono);
}

.ex-id {
  font-weight: bold;
  color: var(--vp-c-text-1);
  font-size: 1em;
  padding: 2px 2px;
  font-family: var(--vp-font-family-base);
}

.ex-id[href] {
  text-decoration: none;
}

.ex-id[href]:hover {
  text-decoration: underline;
}

.ex-badges {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
}

.badge {
  display: inline-flex;
  align-items: center;
  font-size: 0.75rem;
  padding: 2px 8px;
  border-radius: 12px;
  font-weight: 600;
  line-height: 1.4;
  cursor: help;
}

.badge .icon {
  margin-right: 4px;
  font-size: 0.8em;
}

.diff {
  background-color: var(--vp-c-bg-alt);
  color: var(--vp-c-text-1);
}

.work {
  background-color: var(--vp-c-bg-alt);
  color: var(--vp-c-text-2);
}

.math {
  background-color: var(--vp-c-purple-soft);
  color: var(--vp-c-purple-1);
}

.math-hard {
  background-color: var(--vp-c-red-soft);
  color: var(--vp-c-red-1);
  border: 1px solid var(--vp-c-red-3);
}

.opt {
  background-color: var(--vp-c-gray-soft);
  color: var(--vp-c-text-3);
}

.ext {
  background-color: var(--vp-c-yellow-soft);
  color: var(--vp-c-yellow-dark);
}

.ex-content {
  font-size: 1rem;
  line-height: 1.6;
  color: var(--vp-c-text-1);
}

.is-multi :deep(.question-header) {
  display: flex;
  align-items: center;
  gap: 10px;
  padding-top: 0.75rem;
  font-family: var(--vp-font-family-mono);
}

.is-multi :deep(.question-header.has-divider) {
  margin-top: 0.75rem;
  border-top: 1px solid var(--vp-c-divider);
}

.is-multi :deep(.question-label) {
  font-weight: 700;
  color: var(--vp-c-text-1);
  font-family: var(--vp-font-family-base);
}

.is-multi :deep(.question-body) {
  margin-top: 0.35rem;
}

.is-multi :deep(.question-badges) {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
}

.is-multi :deep(.question-badges .badge) {
  display: inline-flex;
  align-items: center;
  font-size: 0.75rem;
  padding: 2px 8px;
  border-radius: 12px;
  font-weight: 600;
  line-height: 1.4;
  cursor: help;
  vertical-align: middle;
}

.is-multi :deep(.question-badges .diff) {
  background-color: var(--vp-c-bg-alt);
  color: var(--vp-c-text-1);
}

.is-multi :deep(.question-badges .work) {
  background-color: var(--vp-c-bg-alt);
  color: var(--vp-c-text-2);
}

.is-multi :deep(.question-badges .math) {
  background-color: var(--vp-c-purple-soft);
  color: var(--vp-c-purple-1);
}

.is-multi :deep(.question-badges .math-hard) {
  background-color: var(--vp-c-red-soft);
  color: var(--vp-c-red-1);
  border: 1px solid var(--vp-c-red-3);
}

.is-multi :deep(.question-badges .opt) {
  background-color: var(--vp-c-gray-soft);
  color: var(--vp-c-text-3);
}

.is-multi :deep(.question-badges .ext) {
  background-color: var(--vp-c-yellow-soft);
  color: var(--vp-c-yellow-dark);
}

.ex-footer {
  margin-top: 1rem;
  padding-top: 0.8rem;
  border-top: 1px dashed var(--vp-c-divider);
}

.links-container {
  display: flex;
  gap: 1rem;
  font-size: 0.9rem;
}

:deep(.links-container a) {
  display: inline-flex;
  align-items: center;
  font-weight: 500;
  text-decoration: none;
}

:deep(.links-container a:hover) {
  text-decoration: underline;
}
</style>
