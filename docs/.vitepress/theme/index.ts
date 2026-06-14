import Theme from "vitepress/theme";
import "./style/var.css";
import { h } from "vue";
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

export default {
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
  Layout() {
    return h(Theme.Layout, null, {
      "doc-before": () => h(SolutionPageHeader),
      "layout-bottom": () => h(RegisterSW),
    });
  },
};
