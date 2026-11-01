import { inBrowser } from "vitepress";

const MAINLY_C_BASE = "https://minsecrus.github.io/Mainly.C/";
const ignoredNodes = [".vp-copy-ignore", ".diff.remove"].join(", ");

function toBase64Url(str: string): string {
  const bytes = new TextEncoder().encode(str);
  let binary = "";
  for (let i = 0; i < bytes.byteLength; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

export function useRunCode(): void {
  if (!inBrowser) return;

  const runCodeSvg =
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="16" height="16" fill="currentColor" style="margin-left: 2px;"><polygon points="6 3 20 12 6 21 6 3"></polygon></svg>';

  const injectButtons = () => {
    document
      .querySelectorAll<HTMLElement>(
        'div[class*="language-c"]:not([class*="language-cmake"]):not([class*="language-css"]), div[class*="language-cpp"]',
      )
      .forEach((el) => {
        if (el.querySelector("button.run")) return;
        const copyBtn = el.querySelector("button.copy");
        if (!copyBtn) return;
        const btn = document.createElement("button");
        btn.className = "run";
        btn.title = "在 Mainly.C 中运行";
        btn.setAttribute("aria-label", "在 Mainly.C 中运行");
        btn.innerHTML = runCodeSvg;
        copyBtn.insertAdjacentElement("afterend", btn);
      });
  };

  if (document.readyState === "loading") {
    window.addEventListener("DOMContentLoaded", injectButtons);
  } else {
    injectButtons();
  }

  const observer = new MutationObserver(() => {
    injectButtons();
  });
  if (document.body) {
    observer.observe(document.body, { childList: true, subtree: true });
  }

  window.addEventListener("click", (e) => {
    const target = e.target as HTMLElement | null;
    if (!target) return;

    const button = target.closest<HTMLButtonElement>("button.run");
    if (!button) return;

    const parent = button.parentElement;
    if (!parent) return;

    const pre = parent.querySelector("pre");
    if (!pre) return;

    const clone = pre.cloneNode(true) as HTMLElement;
    clone.querySelectorAll(ignoredNodes).forEach((node) => node.remove());
    clone.innerHTML = clone.innerHTML.replace(/\n+/g, "\n");
    const text = (clone.textContent || "").trim();
    if (!text) return;

    const langMatch = /language-(\w+)/.exec(parent.className);
    const lang = langMatch?.[1]?.toLowerCase() || "c";
    const std = lang === "cpp" ? "c++23" : "c23";

    const b64 = toBase64Url(text);
    const targetUrl = `${MAINLY_C_BASE}#b64=${b64}&run=1&std=${std}`;

    window.open(targetUrl, "_blank", "noopener,noreferrer");
  });
}
