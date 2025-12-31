import Theme from "vitepress/theme";
import "./style/var.css";
import { h } from "vue";
import RegisterSW from "./components/RegisterSW.vue";
import CodeCompare from "./components/CodeCompare.vue";
import Quiz from "./components/Quiz.vue";
import TerminalWindow from "./components/TerminalWindow.vue";

export default {
  ...Theme,
  enhanceApp({ app }) {
    app.component("RegisterSW", RegisterSW);
    app.component("CodeCompare", CodeCompare);
    app.component("Quiz", Quiz);
    app.component("TerminalWindow", TerminalWindow);
  },
  Layout() {
    return h(Theme.Layout, null, {
      "layout-bottom": () => h(RegisterSW),
    });
  },
};
