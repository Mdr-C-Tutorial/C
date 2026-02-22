export class MdrCompileError extends Error {
  /**
   * @param {string} message
   * @param {{
   *   filePath: string;
   *   line: number;
   *   column: number;
   * }} options
   */
  constructor(message, options) {
    super(message);
    this.name = "MdrCompileError";
    this.filePath = options.filePath;
    this.line = options.line;
    this.column = options.column;
  }
}
