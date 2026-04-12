<template>
  <span class="term-with-tooltip">
    <!-- 术语部分 -->
    <span
      class="term-block"
      @mouseenter="isHovered = true"
      @mouseleave="isHovered = false"
    >
      <slot></slot>
    </span>

    <!-- 提示内容部分 -->
    <div
      v-if="isHovered"
      class="content-block"
      @mouseenter="isHovered = true"
      @mouseleave="isHovered = false"
    >
      <slot name="tip"></slot>
    </div>
  </span>
</template>

<script setup lang="ts">
import { ref } from "vue";

const isHovered = ref(false);
</script>

<style scoped>
.term-with-tooltip {
  position: relative;
  display: inline-flex;
  flex-direction: column;
}

.term-block {
  /* 术语样式 */
  color: var(--vp-c-brand);
  cursor: pointer;
  display: inline-block;
}

.content-block {
  position: absolute !important;
  width: max-content !important;

  bottom: 75%;
  border: 1px solid var(--vp-c-divider);
  background-color: var(--vp-c-bg-soft);
  border-radius: 8px;
  padding: 3px 20px 3px 20px;
  color: var(--vp-c-text-2);
  max-width: 90vw;
  font-size: 14px;
  z-index: var(--vp-z-index-layout-top);
  box-shadow: 0 4px 4px rgba(63, 53, 70, 0.32);
  margin-bottom: 8px;
}
</style>
