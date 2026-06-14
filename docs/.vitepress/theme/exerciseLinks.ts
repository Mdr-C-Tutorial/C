const solutionPages = import.meta.glob("../../教程/题解/**/*.md");

const solutionById = new Map<string, string>();

function docHrefFromGlobPath(path: string) {
  return `/${path.replace(/^\.\.\/\.\.\//, "").replace(/\.md$/, "")}`;
}

for (const path of Object.keys(solutionPages)) {
  const id = /\/(\d+)\.md$/.exec(path)?.[1];
  if (!id) continue;

  solutionById.set(id, docHrefFromGlobPath(path));
}

export function getExerciseSolutionHref(id: string) {
  return solutionById.get(id) ?? "";
}
