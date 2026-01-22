<!-- .vitepress/theme/components/Quiz.vue -->
<script setup>
import { ref, computed } from 'vue'

const props = defineProps({
  // 问题类型：'single' | 'multi'
  type: {
    type: String,
    default: 'single'
  },
  // 选项列表，格式可以是简单的字符串数组，或者对象数组
  // 推荐对象格式: [{ label: '描述', value: 'A' }]
  options: {
    type: Array,
    required: true
  },
  // 正确答案
  // 单选传入值 (如 'A')
  // 多选传入数组 (如 ['A', 'C'])
  answer: {
    type: [String, Array],
    required: true
  }
})

// 用户选中的值
// 单选是 null 或 value
// 多选是 []
const selected = ref(props.type === 'multi' ? [] : null)
const isSubmitted = ref(false)

const isCorrect = computed(() => {
  if (!isSubmitted.value) return false

  if (props.type === 'single') {
    return selected.value === props.answer
  } else {
    // 多选比较：先排序再转字符串比较，确保顺序不影响结果
    const userAns = JSON.stringify([...selected.value].sort())
    const correctAns = JSON.stringify([...props.answer].sort())
    return userAns === correctAns
  }
})

const handleOptionClick = (optionValue) => {
  if (isSubmitted.value && props.type === 'single') return // 单选提交后不能改

  if (props.type === 'single') {
    selected.value = optionValue
    // 单选模式：选中即提交
    isSubmitted.value = true
  } else {
    // 多选模式：如果不包含则添加，包含则移除（提交后依然允许查看，但通常提交后会禁用修改，这里为了体验设为提交后不可改）
    if (isSubmitted.value) return

    const index = selected.value.indexOf(optionValue)
    if (index === -1) {
      selected.value.push(optionValue)
    } else {
      selected.value.splice(index, 1)
    }
  }
}

const submitMulti = () => {
  if (selected.value.length === 0) return // 没选不能交
  isSubmitted.value = true
}

const getOptionClass = (optionValue) => {
  const isSelected = props.type === 'single'
    ? selected.value === optionValue
    : selected.value.includes(optionValue)

  if (!isSubmitted.value) {
    return isSelected ? 'selected' : ''
  }

  // 提交后的状态判定
  // 1. 如果是单选
  if (props.type === 'single') {
    if (optionValue === props.answer) return 'correct' // 正确答案总是绿色
    if (isSelected && optionValue !== props.answer) return 'wrong' // 选错的项目变红
  }

  // 2. 如果是多选
  if (props.type === 'multi') {
    const isAns = props.answer.includes(optionValue)
    if (isAns) return 'correct' // 是答案的一部分
    if (isSelected && !isAns) return 'wrong' // 选了不该选的
  }

  return ''
}
</script>

<template>
  <div class="quiz-container"
    :class="{ 'quiz-passed': isSubmitted && isCorrect, 'quiz-failed': isSubmitted && !isCorrect }">
    <div class="quiz-header">
      <span class="quiz-badge">{{ type === 'single' ? '单选' : '多选' }}</span>
      <span class="quiz-question">
        <slot name="question">题目内容</slot>
      </span>
    </div>

    <div class="quiz-options">
      <div v-for="(opt, idx) in options" :key="idx" class="quiz-option" :class="getOptionClass(opt.value)"
        @click="handleOptionClick(opt.value)">
        <div class="radio-box">
          <span v-if="getOptionClass(opt.value) === 'correct'">✔</span>
          <span v-else-if="getOptionClass(opt.value) === 'wrong'">✖</span>
          <span v-else class="circle"></span>
        </div>
        <div class="option-label">{{ opt.label }}</div>
      </div>
    </div>

    <div v-if="type === 'multi' && !isSubmitted" class="quiz-footer">
      <button class="submit-btn" @click="submitMulti" :disabled="selected.length === 0">
        确认答案
      </button>
    </div>

    <div v-if="isSubmitted" class="quiz-result">
      <div class="result-title" :class="isCorrect ? 'text-green' : 'text-red'">
        {{ isCorrect ? '回答正确！🎉' : '回答错误 😅' }}
      </div>
      <div class="result-explanation">
        <p class="explanation-title">解析：</p>
        <slot>暂时没有解析...</slot>
      </div>
    </div>
  </div>
</template>

<style scoped>
.quiz-container {
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background-color: var(--vp-c-bg-soft);
  padding: .5rem 1.5rem 1.5rem 1.5rem;
  margin: 1.5rem 0;
  transition: border-color 0.3s;
}

.quiz-header {
  display: flex;
  align-items: center;
  gap: 0.8rem;
  margin-bottom: 1rem;
}

.quiz-badge {
  background-color: var(--vp-c-brand);
  color: white;
  padding: 2px 8px;
  border-radius: 4px;
  font-size: 0.75rem;
  flex-shrink: 0;
}

.quiz-options {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.quiz-option {
  display: flex;
  align-items: center;
  gap: 0.8rem;
  padding: 0.8rem;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  cursor: pointer;
  background-color: var(--vp-c-bg);
  transition: all 0.2s;
}

.quiz-option:hover:not(.correct):not(.wrong) {
  border-color: var(--vp-c-brand);
}

.quiz-option.selected {
  border-color: var(--vp-c-brand);
  background-color: var(--vp-c-brand-dimm);
}

.quiz-option.correct {
  border-color: var(--vp-c-green);
  background-color: rgba(16, 185, 129, 0.1);
  color: var(--vp-c-green);
}

.quiz-option.wrong {
  border-color: var(--vp-c-red);
  background-color: rgba(244, 63, 94, 0.1);
  color: var(--vp-c-red);
}

.radio-box {
  width: 20px;
  height: 20px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: bold;
}

.circle {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  border: 2px solid var(--vp-c-text-3);
}

.selected .circle {
  background-color: var(--vp-c-brand);
  border-color: var(--vp-c-brand);
}

.option-label {
  font-family: var(--vp-font-family-mono);
}

.quiz-footer {
  margin-top: 1rem;
  text-align: right;
}

.submit-btn {
  background-color: var(--vp-c-brand);
  color: white;
  padding: 0.5rem 1.5rem;
  border-radius: 4px;
  font-weight: 600;
  transition: opacity 0.2s;
}

.submit-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.submit-btn:hover:not(:disabled) {
  opacity: 0.9;
}

.quiz-result {
  margin-top: 1.5rem;
  padding-top: 1rem;
  border-top: 1px dashed var(--vp-c-divider);
  animation: fadeIn 0.3s ease;
}

.result-title {
  font-weight: bold;
  font-size: 1.1rem;
  margin-bottom: 0.5rem;
}

.text-green {
  color: var(--vp-c-green);
}

.text-red {
  color: var(--vp-c-red);
}

.explanation-title {
  font-size: 0.9rem;
  color: var(--vp-c-text-2);
  margin-bottom: 0.2rem;
  font-weight: bold;
}

@keyframes fadeIn {
  from {
    opacity: 0;
    transform: translateY(-5px);
  }

  to {
    opacity: 1;
    transform: translateY(0);
  }
}
</style>