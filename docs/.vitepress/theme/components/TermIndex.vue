<script setup lang="ts">
import { computed, ref } from "vue";

type TermEntry = {
  term: string;
  translation: string;
  note?: string;
  aliases?: string[];
};

const props = defineProps<{
  entries: TermEntry[];
  title?: string;
  description?: string;
}>();

const keyword = ref("");

function scrollToTop() {
  window.scrollTo({ top: 0, behavior: "smooth" });
}

const normalizedEntries = computed(() =>
  [...props.entries]
    .map((entry) => ({
      ...entry,
      aliases: entry.aliases ?? [],
    }))
    .sort((left, right) =>
      left.term.localeCompare(right.term, "en", {
        sensitivity: "base",
        numeric: true,
      }),
    ),
);

const filteredEntries = computed(() => {
  const query = keyword.value.trim().toLocaleLowerCase("en-US");
  if (!query) {
    return normalizedEntries.value;
  }

  return normalizedEntries.value.filter((entry) => {
    const haystack = [entry.term, entry.translation, entry.note ?? "", ...entry.aliases]
      .join("\n")
      .toLocaleLowerCase("en-US");

    return haystack.includes(query);
  });
});

const letterGroups = computed(() => {
  const groups = new Map<string, TermEntry[]>();

  for (const entry of filteredEntries.value) {
    const first = entry.term.trim().charAt(0).toUpperCase();
    const key = /[A-Z]/.test(first) ? first : "#";
    const bucket = groups.get(key);
    if (bucket) {
      bucket.push(entry);
    } else {
      groups.set(key, [entry]);
    }
  }

  return Array.from(groups.entries()).map(([letter, entries]) => ({
    letter,
    entries,
  }));
});

const availableLetters = computed(() =>
  letterGroups.value.map((group) => group.letter),
);
</script>

<template>
  <section class="term-index">
    <button
      class="term-index__backtop"
      type="button"
      aria-label="返回顶部"
      title="返回顶部"
      @click="scrollToTop"
    >
      ↑
    </button>

    <header class="term-index__header">
      <div>
        <p
          v-if="title"
          class="term-index__title"
        >
          {{ title }}
        </p>
        <p
          v-if="description"
          class="term-index__description"
        >
          {{ description }}
        </p>
      </div>
      <label class="term-index__search">
        <span class="term-index__search-label">检索</span>
        <input
          v-model="keyword"
          class="term-index__search-input"
          type="search"
          placeholder="英文术语 / 中文译法 / 备注"
        />
      </label>
    </header>

    <nav
      class="term-index__letters"
      aria-label="术语首字母索引"
    >
      <a
        v-for="letter in availableLetters"
        :key="letter"
        class="term-index__letter"
        :href="`#term-letter-${letter}`"
      >
        {{ letter }}
      </a>
    </nav>

    <p class="term-index__count">共 {{ filteredEntries.length }} 条术语。</p>

    <section
      v-for="group in letterGroups"
      :id="`term-letter-${group.letter}`"
      :key="group.letter"
      class="term-index__group"
    >
      <h2 class="term-index__group-title">{{ group.letter }}</h2>
      <div class="term-index__list">
        <article
          v-for="entry in group.entries"
          :key="entry.term"
          class="term-index__item"
        >
          <div class="term-index__item-main">
            <code class="term-index__term">{{ entry.term }}</code>
            <strong class="term-index__translation">{{ entry.translation }}</strong>
          </div>
          <p
            v-if="entry.aliases.length"
            class="term-index__aliases"
          >
            别名：{{ entry.aliases.join(" / ") }}
          </p>
          <p
            v-if="entry.note"
            class="term-index__note"
          >
            {{ entry.note }}
          </p>
        </article>
      </div>
    </section>

    <p
      v-if="!filteredEntries.length"
      class="term-index__empty"
    >
      没有匹配结果。请尝试英文原词、中文译法或相关备注。
    </p>
  </section>
</template>

<style scoped>
.term-index {
  margin: 1.5rem 0 2rem;
}

.term-index__backtop {
  position: sticky;
  top: 5.5rem;
  z-index: 10;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  margin-left: auto;
  margin-bottom: 0.75rem;
  width: 2.6rem;
  height: 2.6rem;
  padding: 0;
  border: 1px solid var(--vp-c-divider);
  border-radius: 999px;
  background: color-mix(in srgb, var(--vp-c-bg) 92%, transparent);
  color: var(--vp-c-text-2);
  font-size: 1.15rem;
  line-height: 1;
  text-decoration: none;
  backdrop-filter: blur(10px);
  cursor: pointer;
}

.term-index__backtop:hover {
  color: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
}

.term-index__header {
  display: grid;
  gap: 1rem;
  grid-template-columns: minmax(0, 1fr);
  margin-bottom: 1rem;
  padding: 1rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 16px;
  background:
    linear-gradient(
      135deg,
      color-mix(in srgb, var(--vp-c-brand-1) 9%, transparent),
      transparent
    ),
    var(--vp-c-bg-soft);
}

.term-index__title {
  margin: 0;
  font-size: 1.1rem;
  font-weight: 700;
  color: var(--vp-c-text-1);
}

.term-index__description {
  margin: 0.45rem 0 0;
  color: var(--vp-c-text-2);
  line-height: 1.65;
}

.term-index__search {
  display: grid;
  gap: 0.4rem;
}

.term-index__search-label {
  font-size: 0.9rem;
  font-weight: 600;
  color: var(--vp-c-text-2);
}

.term-index__search-input {
  width: 100%;
  min-height: 2.75rem;
  padding: 0.7rem 0.9rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}

.term-index__letters {
  display: flex;
  flex-wrap: wrap;
  gap: 0.45rem;
  margin: 1rem 0 0.75rem;
}

.term-index__letter {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 2.2rem;
  min-height: 2.2rem;
  padding: 0 0.65rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 999px;
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-2);
  font-weight: 700;
  text-decoration: none;
}

.term-index__letter:hover {
  color: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
}

.term-index__count,
.term-index__empty {
  color: var(--vp-c-text-2);
}

.term-index__group {
  margin-top: 1.75rem;
  scroll-margin-top: 5rem;
}

.term-index__group-title {
  margin: 0 0 0.8rem;
  padding-bottom: 0.35rem;
  border-bottom: 1px solid var(--vp-c-divider);
}

.term-index__list {
  display: grid;
  gap: 0.85rem;
}

.term-index__item {
  padding: 1rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 14px;
  background: var(--vp-c-bg-soft);
}

.term-index__item-main {
  display: flex;
  flex-wrap: wrap;
  gap: 0.75rem;
  align-items: baseline;
}

.term-index__term {
  font-size: 0.96rem;
}

.term-index__translation {
  color: var(--vp-c-text-1);
}

.term-index__aliases,
.term-index__note {
  margin: 0.55rem 0 0;
  color: var(--vp-c-text-2);
  line-height: 1.7;
}

@media (min-width: 860px) {
  .term-index__header {
    grid-template-columns: minmax(0, 1fr) minmax(18rem, 22rem);
    align-items: end;
  }
}
</style>
