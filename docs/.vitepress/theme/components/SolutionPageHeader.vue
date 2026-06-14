<script setup lang="ts">
import { computed } from "vue";
import { useData } from "vitepress";
import SolutionBackLink from "./SolutionBackLink.vue";

const { page } = useData();

const exerciseId = computed(() => {
  if (!page.value.relativePath.startsWith("教程/题解/")) return "";
  const match = /\/(\d+)\.md$/.exec(page.value.relativePath);
  return match?.[1] ?? "";
});
</script>

<template>
  <SolutionBackLink
    v-if="exerciseId"
    :id="exerciseId"
  />
</template>
