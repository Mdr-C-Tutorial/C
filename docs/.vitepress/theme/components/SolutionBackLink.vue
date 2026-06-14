<script setup lang="ts">
import { computed } from "vue";
import { useData, withBase } from "vitepress";

type ExerciseSourceLink = {
  href: string;
  title: string;
};

const props = defineProps({
  id: { type: String, required: true },
});

const { page } = useData();

const sourceLinks = computed(() =>
  (
    (page.value.frontmatter.exerciseSources as ExerciseSourceLink[] | undefined) ??
    []
  ).map((source) => ({
    ...source,
    href: withBase(`${source.href}#${props.id}`),
  })),
);
</script>

<template>
  <nav
    v-if="sourceLinks.length > 0"
    class="solution-back-links"
    aria-label="题解来源"
  >
    <span class="solution-back-label">返回正文：</span>
    <a
      v-for="source in sourceLinks"
      :key="source.href"
      class="solution-back-link"
      :href="source.href"
    >
      {{ source.title }}
    </a>
  </nav>
</template>

<style scoped>
.solution-back-links {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem;
  margin: 0 0 1rem;
}

.solution-back-label {
  color: var(--vp-c-text-2);
  font-weight: 600;
}

.solution-back-link {
  display: inline-flex;
  align-items: center;
  font-weight: 600;
  text-decoration: none;
}

.solution-back-link:hover {
  text-decoration: underline;
}
</style>
