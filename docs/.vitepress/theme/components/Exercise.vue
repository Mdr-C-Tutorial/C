<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps({
    id: { type: String, required: true },
    d: { type: Number, required: true }, // Difficulty (0-9)
    w: { type: Number, required: true }, // Workload (0-9)
    math: { type: String, default: '' }, // '', 'M', 'HM'
    scope: { type: String, default: '' } // '', '*', '**'
})

const difficultyColor = computed(() => {
    if (props.d <= 2) return 'green'
    if (props.d <= 5) return 'blue'
    if (props.d <= 7) return 'orange'
    return 'red'
})

const estimatedTime = computed(() => {
    const mins = Math.pow(3, props.w)
    if (mins < 60) return `${mins} min`
    return `${(mins / 60).toFixed(1)} h`
})

const isOptional = computed(() => props.scope === '*')
const isExtension = computed(() => props.scope === '**')
</script>

<template>
    <div class="exercise-card" :class="[difficultyColor, { 'is-optional': isOptional }]">
        <div class="ex-header">
            <span class="ex-id">#{{ id }}</span>

            <div class="ex-badges">
                <span class="badge diff" :title="`难度等级: ${d}`">
                    <span class="icon">⚡</span>{{ d }}
                </span>

                <span class="badge work" :title="`预估耗时: ${estimatedTime}`">
                    <span class="icon">⏳</span>{{ w }}
                </span>

                <span v-if="math === 'M'" class="badge math" title="需要数学知识">∑</span>
                <span v-if="math === 'HM'" class="badge math-hard" title="需要高深数学知识">∫</span>

                <span v-if="isOptional" class="badge opt" title="跳过不影响后续学习">选做</span>
                <span v-if="isExtension" class="badge ext" title="超纲/拓展内容">拓展</span>
            </div>
        </div>

        <div class="ex-content">
            <slot></slot>
        </div>

        <div v-if="$slots.links" class="ex-footer">
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
    transition: transform 0.2s ease, box-shadow 0.2s ease;
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
    opacity: 0.85;
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