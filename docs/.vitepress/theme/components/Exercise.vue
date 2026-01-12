<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps({
  id: { type: String, required: true },
  d: { type: Number, required: true }, // Difficulty (0-9)
  w: { type: Number, required: true }, // Workload (0-9)
  math: { type: String, default: '' }, // '', 'M', 'HM'
  scope: { type: String, default: '' } // '', '*', '**'
})

// 难度颜色计算：0-2 绿色，3-5 蓝色，6-7 橙色，8-9 红色
const difficultyColor = computed(() => {
  if (props.d <= 2) return 'green'
  if (props.d <= 5) return 'blue'
  if (props.d <= 7) return 'orange'
  return 'red'
})

// 估算时间（基于 2^w 分钟的假设，可按需修改）
const estimatedTime = computed(() => {
  const mins = Math.pow(2, props.w)
  if (mins < 60) return `${mins} min`
  return `${(mins / 60).toFixed(1)} h`
})

// 是否为选做/拓展
const isOptional = computed(() => props.scope === '*')
const isExtension = computed(() => props.scope === '**')
</script>

<template>
  <div 
    class="exercise-card" 
    :class="[difficultyColor, { 'is-optional': isOptional }]"
  >
    <!-- 头部元数据行 -->
    <div class="ex-header">
      <span class="ex-id">#{{ id }}</span>
      
      <div class="ex-badges">
        <!-- 难度 Badge -->
        <span class="badge diff" :title="`难度等级: ${d}`">
          <span class="icon">⚡</span>{{ d }}
        </span>
        
        <!-- 工作量 Badge -->
        <span class="badge work" :title="`预估耗时: ${estimatedTime}`">
          <span class="icon">⏳</span>{{ w }}
        </span>

        <!-- 数学标记 -->
        <span v-if="math === 'M'" class="badge math" title="需要数学知识">∑ Math</span>
        <span v-if="math === 'HM'" class="badge math-hard" title="需要高深数学知识">∫ Hard Math</span>
        
        <!-- 范围标记 -->
        <span v-if="isOptional" class="badge opt" title="跳过不影响后续学习">选做</span>
        <span v-if="isExtension" class="badge ext" title="超纲/拓展内容">拓展</span>
      </div>
    </div>

    <!-- 题目内容 -->
    <div class="ex-content">
      <slot></slot>
    </div>

    <!-- 底部链接插槽（如果有） -->
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
  background-color: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
  border-left-width: 4px; /* 左侧色条 */
  transition: transform 0.2s ease, box-shadow 0.2s ease;
}

/* 悬停微交互 */
.exercise-card:hover {
  box-shadow: 0 4px 12px rgba(0,0,0,0.05);
  transform: translateY(-1px);
}

/* 难度颜色系统 (利用 VitePress CSS 变量) */
.exercise-card.green { border-left-color: var(--vp-c-green-1); }
.exercise-card.blue { border-left-color: var(--vp-c-indigo-1); }
.exercise-card.orange { border-left-color: var(--vp-c-yellow-1); }
.exercise-card.red { border-left-color: var(--vp-c-red-1); }

/* 选做题样式弱化 */
.exercise-card.is-optional {
  opacity: 0.85;
  background-color: transparent;
  border-style: dashed;
}

/* 头部布局 */
.ex-header {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 0.8rem;
  font-family: var(--vp-font-family-mono);
}

.ex-id {
  font-weight: bold;
  color: var(--vp-c-text-2);
  font-size: 0.9em;
  background: var(--vp-c-bg-alt);
  padding: 2px 6px;
  border-radius: 4px;
}

/* Badges 胶囊样式 */
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

/* Badge 具体配色 */
.diff { background-color: var(--vp-c-bg-alt); color: var(--vp-c-text-1); }
.work { background-color: var(--vp-c-bg-alt); color: var(--vp-c-text-2); }

.math { background-color: var(--vp-c-purple-soft); color: var(--vp-c-purple-1); }
.math-hard { background-color: var(--vp-c-red-soft); color: var(--vp-c-red-1); border: 1px solid var(--vp-c-red-3); }

.opt { background-color: var(--vp-c-gray-soft); color: var(--vp-c-text-3); }
.ext { background-color: var(--vp-c-yellow-soft); color: var(--vp-c-yellow-dark); }

/* 内容区 */
.ex-content {
  font-size: 1rem;
  line-height: 1.6;
  color: var(--vp-c-text-1);
}

/* 底部链接区 */
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

/* 针对插槽内链接的样式修正 */
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