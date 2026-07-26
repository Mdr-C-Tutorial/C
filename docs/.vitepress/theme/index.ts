import Theme from "vitepress/theme";
import "./style/var.css";
import { defineComponent, h } from "vue";
import { createBrowserByokGenerator, type BrowserByokProvider } from "@gentorial/ai";
import { createGentorialRuntime } from "@gentorial/runtime-vue";
import { createGentorialTheme } from "@gentorial/theme-default";
import "@gentorial/theme-default/style.css";
import course from "../../../course.config.js";
import RegisterSW from "./components/RegisterSW.vue";
import CodeCompare from "./components/CodeCompare.vue";
import Quiz from "./components/Quiz.vue";
import TerminalWindow from "./components/TerminalWindow.vue";
import Term from "./components/Term.vue";
import Exercise from "./components/Exercise.vue";
import CodeRunner from "./components/CodeRunner.vue";
import TermIndex from "./components/TermIndex.vue";
import SolutionBackLink from "./components/SolutionBackLink.vue";
import SolutionPageHeader from "./components/SolutionPageHeader.vue";

const runtime = createGentorialRuntime({
  allowUnsafeHtml: course.rendering?.allowUnsafeHtml,
  learnerProfile: {
    detail: "balanced",
    tone: "conversational",
    narrative: "direct",
  },
  generate(request, context) {
    const input = {
      course,
      generate: request.generate,
      concepts: request.concepts,
      ...(request.learner ? { learner: request.learner } : {}),
      ...(request.conversation ? { conversation: request.conversation } : {}),
    };
    if (!context.byok) {
      throw new Error("尚未配置生成服务。请先在 Preferences 中配置 BYOK。");
    }
    const generator = createBrowserByokGenerator({
      ...context.byok,
      provider: context.byok.provider as BrowserByokProvider,
    });

    return (
      generator.stream?.(input, { signal: context.signal }) ??
      generator.generate(input, { signal: context.signal })
    );
  },
});

const MdrCTutorialLayout = defineComponent({
  name: "MdrCTutorialLayout",
  setup(_, { slots }) {
    return () =>
      h(Theme.Layout!, null, {
        ...slots,
        "doc-before": () => [slots["doc-before"]?.(), h(SolutionPageHeader)],
        "layout-bottom": () => [slots["layout-bottom"]?.(), h(RegisterSW)],
      });
  },
});

const cTheme = {
  ...Theme,
  enhanceApp({ app }) {
    app.component("RegisterSW", RegisterSW);
    app.component("CodeCompare", CodeCompare);
    app.component("Quiz", Quiz);
    app.component("TerminalWindow", TerminalWindow);
    app.component("Term", Term);
    app.component("TermIndex", TermIndex);
    app.component("Exercise", Exercise);
    app.component("CodeRunner", CodeRunner);
    app.component("SolutionBackLink", SolutionBackLink);
  },
  Layout: MdrCTutorialLayout,
};

export default createGentorialTheme({
  extends: cTheme,
  codeBlock: {
    lineNumbers: true,
  },
  enhanceApp({ app }) {
    app.use(runtime);
  },
});
