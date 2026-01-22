<script setup>
defineProps({
  leftTitle: {
    type: String,
    default: '错误写法 (Bad)'
  },
  rightTitle: {
    type: String,
    default: '正确写法 (Good)'
  },
  vertical: {
    type: Boolean,
    default: false
  }
})
</script>

<template>
  <div class="code-compare" :class="{ 'is-vertical': vertical }">
    <div class="compare-pane left-pane">
      <div class="pane-header bad-header">
        <span class="icon">❌</span> {{ leftTitle }}
      </div>
      <div class="pane-content">
        <slot name="left"></slot>
      </div>
    </div>

    <div class="compare-pane right-pane">
      <div class="pane-header good-header">
        <span class="icon">✅</span> {{ rightTitle }}
      </div>
      <div class="pane-content">
        <slot name="right"></slot>
      </div>
    </div>
  </div>
</template>

<style scoped>
.code-compare {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 1rem;
  margin: 1rem 0;
  align-items: start;
}

@media (max-width: 768px) {
  .code-compare {
    grid-template-columns: 1fr;
  }
}

.code-compare.is-vertical {
  grid-template-columns: 1fr;
}

.compare-pane {
  border-radius: 8px;
  overflow: hidden;
  border: 1px solid var(--vp-c-divider);
  background-color: var(--vp-c-bg-soft);
  height: 100%;
  display: flex;
  flex-direction: column;
}

.pane-header {
  padding: 0.5rem 1rem;
  font-weight: 600;
  font-size: 0.9rem;
  border-bottom: 1px solid var(--vp-c-divider);
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.bad-header {
  background-color: rgba(234, 88, 88, 0.1);
  /* 红色背景淡色 */
  color: #d85656;
}

.good-header {
  background-color: rgba(66, 184, 131, 0.1);
  /* 绿色背景淡色 */
  color: #349d6e;
}

.pane-content {
  flex: 100%;
  display: flex;
}

.pane-content :deep(div[class*='language-']) {
  margin: 0 !important;
  border-radius: 0 !important;
  display: flex;
  flex: 100%;
}
</style>