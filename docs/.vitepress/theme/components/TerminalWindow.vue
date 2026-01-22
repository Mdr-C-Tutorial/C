<!-- .vitepress/theme/components/TerminalWindow.vue -->
<script setup>
defineProps({
  title: {
    type: String,
    default: 'bash'
  }
})
</script>

<template>
  <div class="terminal-window">
    <div class="terminal-header">
      <div class="buttons">
        <span class="btn red"></span>
        <span class="btn yellow"></span>
        <span class="btn green"></span>
      </div>
      <div class="title">{{ title }}</div>
    </div>
    <div class="terminal-body">
      <slot></slot>
    </div>
  </div>
</template>

<style scoped>
.terminal-window {
  border-radius: 8px;
  background: #1e1e1e;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
  margin: 1rem 0;
  overflow: hidden;
  font-family: 'Menlo', 'Monaco', 'Courier New', monospace;
}

.terminal-header {
  background: #2d2d2d;
  padding: 8px 12px;
  display: flex;
  align-items: center;
  position: relative;
}

.buttons {
  display: flex;
  gap: 6px;
}

.btn {
  width: 12px;
  height: 12px;
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

.title {
  position: absolute;
  left: 0;
  right: 0;
  text-align: center;
  color: #999;
  font-size: 12px;
  pointer-events: none;
}

.terminal-body {
  padding: 1rem;
  color: #e0e0e0;
  font-size: 0.9rem;
  line-height: 1.5;
  white-space: pre-wrap;
}

.terminal-body :deep(strong) {
  color: #fff;
  font-weight: bold;
}

.terminal-body :deep(.prompt) {
  color: #42b883;
  margin-right: 8px;
  user-select: none;
}
</style>