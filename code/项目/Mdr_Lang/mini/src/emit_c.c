#include "mini_internal.h"

#include <inttypes.h>

typedef enum {
    TY_INT = 0,
    TY_BOOL,
    TY_RANGE
} value_type;

typedef struct {
    const char *name;
    size_t len;
    value_type type;
    bool is_const;
} type_entry;

typedef struct {
    type_entry *items;
    size_t len;
    size_t cap;
    size_t *marks;
    size_t mark_len;
    size_t mark_cap;
    mdr_buffer *out;
    mdr_mini_error *error;
    bool failed;
    size_t indent;
} emitter;

static bool emit_fail(emitter *em, size_t line, size_t column, const char *message) {
    if (!em->failed) {
        mini_set_error(em->error, false, line, column, "%s", message);
        em->failed = true;
    }
    return false;
}

static bool emit_failf(emitter *em, size_t line, size_t column, const char *format, ...) {
    if (!em->failed) {
        char message[256];
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        mini_set_error(em->error, false, line, column, "%s", message);
        em->failed = true;
    }
    return false;
}

static const char *type_name(value_type type) {
    switch (type) {
        case TY_INT:
            return "int64_t";
        case TY_BOOL:
            return "bool";
        case TY_RANGE:
            return "struct mdr_range";
    }
    return "int64_t";
}

static bool push_scope(emitter *em) {
    if (em->mark_len == em->mark_cap) {
        size_t next = em->mark_cap == 0u ? 8u : em->mark_cap * 2u;
        size_t *marks = (size_t *)realloc(em->marks, next * sizeof(*marks));
        if (marks == NULL) {
            return emit_fail(em, 1u, 1u, "out of memory");
        }
        em->marks = marks;
        em->mark_cap = next;
    }
    em->marks[em->mark_len++] = em->len;
    return true;
}

static void pop_scope(emitter *em) {
    if (em->mark_len > 0u) {
        em->len = em->marks[--em->mark_len];
    }
}

static type_entry *lookup_type(emitter *em, const char *name, size_t len) {
    for (size_t i = em->len; i > 0u; --i) {
        type_entry *entry = &em->items[i - 1u];
        if (entry->len == len && strncmp(entry->name, name, len) == 0) {
            return entry;
        }
    }
    return NULL;
}

static type_entry *lookup_type_in_scope(emitter *em, const char *name, size_t len) {
    size_t begin = em->mark_len == 0u ? 0u : em->marks[em->mark_len - 1u];
    for (size_t i = em->len; i > begin; --i) {
        type_entry *entry = &em->items[i - 1u];
        if (entry->len == len && strncmp(entry->name, name, len) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool declare_type(emitter *em,
                         const char *name,
                         size_t len,
                         value_type type,
                         bool is_const,
                         size_t line,
                         size_t column) {
    if (lookup_type_in_scope(em, name, len) != NULL) {
        return emit_failf(em, line, column,
                          "the C backend cannot redeclare '%.*s' in the same block", (int)len, name);
    }
    if (em->len == em->cap) {
        size_t next = em->cap == 0u ? 32u : em->cap * 2u;
        type_entry *items = (type_entry *)realloc(em->items, next * sizeof(*items));
        if (items == NULL) {
            return emit_fail(em, line, column, "out of memory");
        }
        em->items = items;
        em->cap = next;
    }
    em->items[em->len++] = (type_entry){.name = name, .len = len, .type = type, .is_const = is_const};
    return true;
}

static bool infer_type(emitter *em, const node *expr, value_type *out) {
    switch (expr->kind) {
        case NODE_NUMBER_LITERAL:
            *out = TY_INT;
            return true;
        case NODE_BOOL_LITERAL:
            *out = TY_BOOL;
            return true;
        case NODE_RANGE_EXPR:
            *out = TY_RANGE;
            return true;
        case NODE_GROUP_EXPR:
            return infer_type(em, expr->as.group.inner, out);
        case NODE_UNARY_EXPR: {
            value_type operand;
            if (!infer_type(em, expr->as.unary.operand, &operand)) {
                return false;
            }
            if (expr->as.unary.op == OP_NOT) {
                if (operand != TY_BOOL) {
                    return emit_fail(em, expr->line, expr->column, "operand of ! must be bool");
                }
                *out = TY_BOOL;
                return true;
            }
            if (operand != TY_INT) {
                return emit_fail(em, expr->line, expr->column,
                                 "operand of unary - must be a number");
            }
            *out = TY_INT;
            return true;
        }
        case NODE_BINARY_EXPR: {
            value_type left;
            value_type right;
            if (!infer_type(em, expr->as.binary.left, &left) ||
                !infer_type(em, expr->as.binary.right, &right)) {
                return false;
            }
            switch (expr->as.binary.op) {
                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                    if (left != TY_INT || right != TY_INT) {
                        return emit_fail(em, expr->line, expr->column,
                                         "arithmetic operands must be numbers");
                    }
                    *out = TY_INT;
                    return true;
                case OP_LT:
                case OP_LE:
                case OP_GT:
                case OP_GE:
                    if (left != right || left == TY_RANGE) {
                        return emit_fail(em, expr->line, expr->column,
                                         "comparison needs two numbers or two bools");
                    }
                    *out = TY_BOOL;
                    return true;
                default:
                    if (left != right) {
                        return emit_fail(em, expr->line, expr->column,
                                         "both operands must have the same type");
                    }
                    *out = TY_BOOL;
                    return true;
            }
        }
        case NODE_IDENTIFIER: {
            type_entry *entry =
                lookup_type(em, expr->as.identifier.name, expr->as.identifier.len);
            if (entry == NULL) {
                return emit_failf(em, expr->line, expr->column, "undefined name: %.*s",
                                  (int)expr->as.identifier.len, expr->as.identifier.name);
            }
            *out = entry->type;
            return true;
        }
        default:
            return emit_fail(em, expr->line, expr->column, "expression has no value type");
    }
}

static bool emit_indent(emitter *em) {
    for (size_t i = 0; i < em->indent; ++i) {
        if (!mdr_buffer_append_str(em->out, "    ")) {
            return emit_fail(em, 1u, 1u, "out of memory");
        }
    }
    return true;
}

static bool emit_expr(emitter *em, const node *expr);
static bool emit_statement(emitter *em, const node *stmt);

static bool emit_expr(emitter *em, const node *expr) {
    switch (expr->kind) {
        case NODE_NUMBER_LITERAL:
            return mdr_buffer_appendf(em->out, "INT64_C(%" PRId64 ")", expr->as.int_value);
        case NODE_BOOL_LITERAL:
            return mdr_buffer_append_str(em->out, expr->as.bool_value ? "true" : "false");
        case NODE_IDENTIFIER:
            return mdr_buffer_appendf(em->out, "%.*s", (int)expr->as.identifier.len,
                                      expr->as.identifier.name);
        case NODE_GROUP_EXPR:
            return mdr_buffer_append_str(em->out, "(") &&
                   emit_expr(em, expr->as.group.inner) && mdr_buffer_append_str(em->out, ")");
        case NODE_UNARY_EXPR:
            return mdr_buffer_append_str(em->out, expr->as.unary.op == OP_NOT ? "!(" : "-(") &&
                   emit_expr(em, expr->as.unary.operand) && mdr_buffer_append_str(em->out, ")");
        case NODE_BINARY_EXPR: {
            const char *op = "+";
            switch (expr->as.binary.op) {
                case OP_ADD: op = "+"; break;
                case OP_SUB: op = "-"; break;
                case OP_MUL: op = "*"; break;
                case OP_DIV: op = "/"; break;
                case OP_EQ: op = "=="; break;
                case OP_NE: op = "!="; break;
                case OP_LT: op = "<"; break;
                case OP_LE: op = "<="; break;
                case OP_GT: op = ">"; break;
                case OP_GE: op = ">="; break;
                case OP_AND: op = "&&"; break;
                case OP_OR: op = "||"; break;
                default: op = "+"; break;
            }
            return mdr_buffer_append_str(em->out, "(") &&
                   emit_expr(em, expr->as.binary.left) &&
                   mdr_buffer_appendf(em->out, " %s ", op) &&
                   emit_expr(em, expr->as.binary.right) &&
                   mdr_buffer_append_str(em->out, ")");
        }
        case NODE_RANGE_EXPR: {
            if (expr->as.range.step != NULL && expr->as.range.step->kind != NODE_NUMBER_LITERAL) {
                return emit_fail(em, expr->line, expr->column,
                                 "the C backend needs an integer literal as the range step");
            }
            int64_t step = expr->as.range.step == NULL ? 1 : expr->as.range.step->as.int_value;
            return mdr_buffer_append_str(em->out, "mdr_range_make(") &&
                   emit_expr(em, expr->as.range.start) && mdr_buffer_append_str(em->out, ", ") &&
                   emit_expr(em, expr->as.range.end) &&
                   mdr_buffer_appendf(em->out, ", INT64_C(%" PRId64 "), %s)", step,
                                      expr->as.range.inclusive_end ? "true" : "false");
        }
        default:
            return emit_fail(em, expr->line, expr->column, "expression cannot be emitted as C");
    }
}

static bool emit_body(emitter *em, const node *stmt) {
    const node *block = stmt;
    if (stmt->kind != NODE_BLOCK_STMT) {
        if (!mdr_buffer_append_str(em->out, " {\n")) {
            return false;
        }
        em->indent += 1u;
        if (!push_scope(em)) {
            return false;
        }
        bool ok = emit_statement(em, stmt);
        pop_scope(em);
        em->indent -= 1u;
        if (!ok) {
            return false;
        }
        return emit_indent(em) && mdr_buffer_append_str(em->out, "}\n");
    }

    if (!mdr_buffer_append_str(em->out, " {\n")) {
        return false;
    }
    em->indent += 1u;
    if (!push_scope(em)) {
        return false;
    }
    for (size_t i = 0; i < block->as.block.statements.len; ++i) {
        if (!emit_statement(em, block->as.block.statements.items[i])) {
            pop_scope(em);
            return false;
        }
    }
    pop_scope(em);
    em->indent -= 1u;
    return emit_indent(em) && mdr_buffer_append_str(em->out, "}\n");
}

static bool emit_binding(emitter *em, const node *stmt) {
    value_type type;
    if (!infer_type(em, stmt->as.binding.initializer, &type)) {
        return false;
    }
    if (!emit_indent(em)) {
        return false;
    }
    if (!mdr_buffer_appendf(em->out, "%s%s %.*s = ", stmt->as.binding.is_const ? "const " : "",
                            type_name(type), (int)stmt->as.binding.len, stmt->as.binding.name)) {
        return false;
    }
    if (!emit_expr(em, stmt->as.binding.initializer)) {
        return false;
    }
    if (!mdr_buffer_append_str(em->out, ";\n")) {
        return false;
    }
    return declare_type(em, stmt->as.binding.name, stmt->as.binding.len, type,
                        stmt->as.binding.is_const, stmt->line, stmt->column);
}

static bool emit_assignment(emitter *em, const node *stmt) {
    type_entry *target = lookup_type(em, stmt->as.assign.name, stmt->as.assign.len);
    if (target == NULL) {
        return emit_failf(em, stmt->line, stmt->column, "undefined name: %.*s",
                          (int)stmt->as.assign.len, stmt->as.assign.name);
    }
    value_type value_kind;
    if (!infer_type(em, stmt->as.assign.value, &value_kind)) {
        return false;
    }
    if (target->is_const) {
        return emit_failf(em, stmt->line, stmt->column, "cannot assign to const: %.*s",
                          (int)stmt->as.assign.len, stmt->as.assign.name);
    }
    if (value_kind != target->type) {
        return emit_failf(em, stmt->line, stmt->column,
                          "cannot assign a %s to '%.*s' of type %s",
                          value_kind == TY_INT ? "number" : (value_kind == TY_BOOL ? "bool" : "range"),
                          (int)stmt->as.assign.len, stmt->as.assign.name,
                          target->type == TY_INT ? "number"
                                                 : (target->type == TY_BOOL ? "bool" : "range"));
    }
    if (stmt->as.assign.op != OP_ASSIGN && target->type != TY_INT) {
        return emit_failf(em, stmt->line, stmt->column,
                          "compound assignment needs a numeric target, '%.*s' is not",
                          (int)stmt->as.assign.len, stmt->as.assign.name);
    }
    const char *op = "=";
    switch (stmt->as.assign.op) {
        case OP_ADD: op = "+="; break;
        case OP_SUB: op = "-="; break;
        case OP_MUL: op = "*="; break;
        case OP_DIV: op = "/="; break;
        default: op = "="; break;
    }
    return emit_indent(em) &&
           mdr_buffer_appendf(em->out, "%.*s %s ", (int)stmt->as.assign.len,
                              stmt->as.assign.name, op) &&
           emit_expr(em, stmt->as.assign.value) && mdr_buffer_append_str(em->out, ";\n");
}

static bool emit_for(emitter *em, const node *stmt) {
    const node *range = stmt->as.for_stmt.iterable;
    if (range->kind != NODE_RANGE_EXPR) {
        return emit_fail(em, stmt->line, stmt->column,
                         "the C backend supports only a range in the for header");
    }
    if (range->as.range.start->kind != NODE_NUMBER_LITERAL ||
        range->as.range.end->kind != NODE_NUMBER_LITERAL ||
        (range->as.range.step != NULL && range->as.range.step->kind != NODE_NUMBER_LITERAL)) {
        return emit_fail(em, stmt->line, stmt->column,
                         "the C backend needs integer literals as range bounds");
    }
    int64_t start = range->as.range.start->as.int_value;
    int64_t end = range->as.range.end->as.int_value;
    int64_t step = range->as.range.step == NULL ? 1 : range->as.range.step->as.int_value;
    if (step <= 0) {
        return emit_fail(em, stmt->line, stmt->column, "range step must be positive");
    }
    if (!emit_indent(em) ||
        !mdr_buffer_appendf(em->out, "for (int64_t %.*s = INT64_C(%" PRId64 "); %.*s %s INT64_C(%" PRId64
                                     "); %.*s += INT64_C(%" PRId64 "))",
                            (int)stmt->as.for_stmt.len, stmt->as.for_stmt.name, start,
                            (int)stmt->as.for_stmt.len, stmt->as.for_stmt.name,
                            range->as.range.inclusive_end ? "<=" : "<", end,
                            (int)stmt->as.for_stmt.len, stmt->as.for_stmt.name, step)) {
        return false;
    }
    if (!push_scope(em)) {
        return false;
    }
    bool ok = declare_type(em, stmt->as.for_stmt.name, stmt->as.for_stmt.len, TY_INT, false,
                           stmt->line, stmt->column) &&
              emit_body(em, stmt->as.for_stmt.body);
    pop_scope(em);
    return ok;
}

static bool emit_statement(emitter *em, const node *stmt) {
    switch (stmt->kind) {
        case NODE_BLOCK_STMT:
            if (!emit_indent(em) || !mdr_buffer_append_str(em->out, "{\n")) {
                return false;
            }
            em->indent += 1u;
            if (!push_scope(em)) {
                return false;
            }
            for (size_t i = 0; i < stmt->as.block.statements.len; ++i) {
                if (!emit_statement(em, stmt->as.block.statements.items[i])) {
                    pop_scope(em);
                    return false;
                }
            }
            pop_scope(em);
            em->indent -= 1u;
            return emit_indent(em) && mdr_buffer_append_str(em->out, "}\n");
        case NODE_BINDING_STMT:
            return emit_binding(em, stmt);
        case NODE_ASSIGN_STMT:
            return emit_assignment(em, stmt);
        case NODE_IF_STMT: {
            value_type cond_type;
            if (!infer_type(em, stmt->as.if_stmt.condition, &cond_type)) {
                return false;
            }
            if (cond_type != TY_BOOL) {
                return emit_fail(em, stmt->line, stmt->column, "if condition must be bool");
            }
            if (!emit_indent(em) || !mdr_buffer_append_str(em->out, "if (") ||
                !emit_expr(em, stmt->as.if_stmt.condition) ||
                !mdr_buffer_append_str(em->out, ")") ||
                !emit_body(em, stmt->as.if_stmt.then_branch)) {
                return false;
            }
            if (stmt->as.if_stmt.else_branch == NULL) {
                return true;
            }
            return emit_indent(em) && mdr_buffer_append_str(em->out, "else") &&
                   emit_body(em, stmt->as.if_stmt.else_branch);
        }
        case NODE_WHILE_STMT: {
            value_type cond_type;
            if (!infer_type(em, stmt->as.while_stmt.condition, &cond_type)) {
                return false;
            }
            if (cond_type != TY_BOOL) {
                return emit_fail(em, stmt->line, stmt->column, "while condition must be bool");
            }
            return emit_indent(em) && mdr_buffer_append_str(em->out, "while (") &&
                   emit_expr(em, stmt->as.while_stmt.condition) &&
                   mdr_buffer_append_str(em->out, ")") &&
                   emit_body(em, stmt->as.while_stmt.body);
        }
        case NODE_LOOP_STMT:
            return emit_indent(em) && mdr_buffer_append_str(em->out, "for (;;)") &&
                   emit_body(em, stmt->as.loop_stmt.body);
        case NODE_FOR_STMT:
            return emit_for(em, stmt);
        case NODE_BREAK_STMT:
            return emit_indent(em) && mdr_buffer_append_str(em->out, "break;\n");
        case NODE_CONTINUE_STMT:
            return emit_indent(em) && mdr_buffer_append_str(em->out, "continue;\n");
        case NODE_PRINT_STMT: {
            value_type value_kind;
            if (!infer_type(em, stmt->as.print_stmt.value, &value_kind)) {
                return false;
            }
            const char *helper = "mdr_print_int";
            if (value_kind == TY_BOOL) {
                helper = "mdr_print_bool";
            } else if (value_kind == TY_RANGE) {
                helper = "mdr_print_range";
            }
            return emit_indent(em) && mdr_buffer_appendf(em->out, "%s(", helper) &&
                   emit_expr(em, stmt->as.print_stmt.value) &&
                   mdr_buffer_append_str(em->out, ");\n");
        }
        default:
            return emit_fail(em, stmt->line, stmt->column, "statement cannot be emitted as C");
    }
}

static const char *k_prologue =
    "/* Generated by mdr_mini from the Mdr Lang subset. */\n"
    "#include <stdbool.h>\n"
    "#include <stdint.h>\n"
    "#include <stdio.h>\n"
    "\n"
    "struct mdr_range {\n"
    "    int64_t start;\n"
    "    int64_t end;\n"
    "    int64_t step;\n"
    "    bool inclusive;\n"
    "};\n"
    "\n"
    "static inline struct mdr_range mdr_range_make(int64_t start, int64_t end, int64_t step,\n"
    "                                              bool inclusive) {\n"
    "    return (struct mdr_range){.start = start, .end = end, .step = step, .inclusive = inclusive};\n"
    "}\n"
    "\n"
    "static inline int64_t mdr_range_count(struct mdr_range range) {\n"
    "    if (range.step <= 0 || range.start > range.end) {\n"
    "        return 0;\n"
    "    }\n"
    "    int64_t span = range.end - range.start;\n"
    "    if (range.inclusive) {\n"
    "        return span / range.step + 1;\n"
    "    }\n"
    "    return (span + range.step - 1) / range.step;\n"
    "}\n"
    "\n"
    "static inline void mdr_print_int(int64_t value) {\n"
    "    printf(\"%lld\\n\", (long long)value);\n"
    "}\n"
    "\n"
    "static inline void mdr_print_bool(bool value) {\n"
    "    printf(\"%s\\n\", value ? \"true\" : \"false\");\n"
    "}\n"
    "\n"
    "static inline void mdr_print_range(struct mdr_range range) {\n"
    "    int64_t count = mdr_range_count(range);\n"
    "    printf(\"[\");\n"
    "    for (int64_t i = 0; i < count; ++i) {\n"
    "        if (i > 0) {\n"
    "            printf(\", \");\n"
    "        }\n"
    "        printf(\"%lld\", (long long)(range.start + i * range.step));\n"
    "    }\n"
    "    printf(\"]\\n\");\n"
    "}\n"
    "\n"
    "int main(void) {\n";

bool mini_emit_c_program(const node *program, mdr_buffer *out, mdr_mini_error *error) {
    emitter em = {.out = out, .error = error, .indent = 1u};
    bool ok = mdr_buffer_append_str(out, k_prologue);
    if (ok && !push_scope(&em)) {
        ok = false;
    }
    for (size_t i = 0; i < program->as.block.statements.len && ok; ++i) {
        ok = emit_statement(&em, program->as.block.statements.items[i]);
    }
    pop_scope(&em);
    free(em.items);
    free(em.marks);
    if (!ok) {
        return false;
    }
    return mdr_buffer_append_str(out, "    return 0;\n}\n");
}