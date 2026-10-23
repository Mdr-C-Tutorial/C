#include "mini_internal.h"

#include <inttypes.h>

typedef enum {
    EXEC_NORMAL = 0,
    EXEC_BREAK,
    EXEC_CONTINUE
} exec_status;

typedef struct {
    const char *name;
    size_t len;
    mini_value value;
    bool is_const;
} env_entry;

typedef struct {
    env_entry *items;
    size_t len;
    size_t cap;
    size_t *marks;
    size_t mark_len;
    size_t mark_cap;
    mdr_buffer *out;
    mdr_mini_error *error;
    bool failed;
} interp;

static bool fail(interp *ip, bool runtime, size_t line, size_t column, const char *message) {
    if (!ip->failed) {
        mini_set_error(ip->error, runtime, line, column, "%s", message);
        ip->failed = true;
    }
    return false;
}

static bool failf(interp *ip, size_t line, size_t column, const char *format, ...) {
    if (!ip->failed) {
        char message[256];
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        mini_set_error(ip->error, true, line, column, "%s", message);
        ip->failed = true;
    }
    return false;
}

static bool push_scope(interp *ip) {
    if (ip->mark_len == ip->mark_cap) {
        size_t next = ip->mark_cap == 0u ? 8u : ip->mark_cap * 2u;
        size_t *marks = (size_t *)realloc(ip->marks, next * sizeof(*marks));
        if (marks == NULL) {
            return fail(ip, false, 1u, 1u, "out of memory");
        }
        ip->marks = marks;
        ip->mark_cap = next;
    }
    ip->marks[ip->mark_len++] = ip->len;
    return true;
}

static void pop_scope(interp *ip) {
    if (ip->mark_len > 0u) {
        ip->len = ip->marks[--ip->mark_len];
    }
}

static env_entry *lookup(interp *ip, const char *name, size_t len) {
    for (size_t i = ip->len; i > 0u; --i) {
        env_entry *entry = &ip->items[i - 1u];
        if (entry->len == len && strncmp(entry->name, name, len) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool define(interp *ip,
                   const char *name,
                   size_t len,
                   mini_value value,
                   bool is_const,
                   size_t line,
                   size_t column) {
    if (ip->len == ip->cap) {
        size_t next = ip->cap == 0u ? 32u : ip->cap * 2u;
        env_entry *items = (env_entry *)realloc(ip->items, next * sizeof(*items));
        if (items == NULL) {
            return fail(ip, false, line, column, "out of memory");
        }
        ip->items = items;
        ip->cap = next;
    }
    ip->items[ip->len++] =
        (env_entry){.name = name, .len = len, .value = value, .is_const = is_const};
    return true;
}

static int64_t range_count(const mini_range *range) {
    if (range->step <= 0 || range->start > range->end) {
        return 0;
    }
    int64_t span = range->end - range->start;
    if (range->inclusive) {
        return span / range->step + 1;
    }
    return (span + range->step - 1) / range->step;
}

static int64_t range_at(const mini_range *range, int64_t index) {
    return range->start + index * range->step;
}

static bool print_value(interp *ip, mini_value value) {
    switch (value.kind) {
        case MINI_VAL_INT:
            return mdr_buffer_appendf(ip->out, "%" PRId64 "\n", value.as.integer);
        case MINI_VAL_BOOL:
            return mdr_buffer_appendf(ip->out, "%s\n", value.as.boolean ? "true" : "false");
        case MINI_VAL_RANGE: {
            if (!mdr_buffer_append_str(ip->out, "[")) {
                return fail(ip, false, 1u, 1u, "out of memory");
            }
            int64_t count = range_count(&value.as.range);
            for (int64_t i = 0; i < count; ++i) {
                if (i > 0 && !mdr_buffer_append_str(ip->out, ", ")) {
                    return fail(ip, false, 1u, 1u, "out of memory");
                }
                if (!mdr_buffer_appendf(ip->out, "%" PRId64, range_at(&value.as.range, i))) {
                    return fail(ip, false, 1u, 1u, "out of memory");
                }
            }
            if (!mdr_buffer_append_str(ip->out, "]\n")) {
                return fail(ip, false, 1u, 1u, "out of memory");
            }
            return true;
        }
    }
    return fail(ip, false, 1u, 1u, "unknown value");
}

static bool eval_expr(interp *ip, const node *expr, mini_value *out);

static bool eval_logical(interp *ip, const node *expr, mini_value *out) {
    mini_value left;
    if (!eval_expr(ip, expr->as.binary.left, &left)) {
        return false;
    }
    if (left.kind != MINI_VAL_BOOL) {
        return failf(ip, expr->line, expr->column, "left operand of %s must be bool",
                     expr->as.binary.op == OP_AND ? "&&" : "||");
    }
    bool short_circuit = expr->as.binary.op == OP_AND ? !left.as.boolean : left.as.boolean;
    if (short_circuit) {
        out->kind = MINI_VAL_BOOL;
        out->as.boolean = left.as.boolean;
        return true;
    }
    mini_value right;
    if (!eval_expr(ip, expr->as.binary.right, &right)) {
        return false;
    }
    if (right.kind != MINI_VAL_BOOL) {
        return failf(ip, expr->line, expr->column, "right operand of %s must be bool",
                     expr->as.binary.op == OP_AND ? "&&" : "||");
    }
    out->kind = MINI_VAL_BOOL;
    out->as.boolean = right.as.boolean;
    return true;
}

static bool eval_comparison(interp *ip, const node *expr, mini_value *out) {
    mini_value left;
    mini_value right;
    if (!eval_expr(ip, expr->as.binary.left, &left) ||
        !eval_expr(ip, expr->as.binary.right, &right)) {
        return false;
    }
    if (left.kind != right.kind || left.kind == MINI_VAL_RANGE) {
        return fail(ip, true, expr->line, expr->column,
                    "comparison needs two numbers or two bools");
    }
    bool result = false;
    if (left.kind == MINI_VAL_INT) {
        int64_t a = left.as.integer;
        int64_t b = right.as.integer;
        switch (expr->as.binary.op) {
            case OP_EQ: result = a == b; break;
            case OP_NE: result = a != b; break;
            case OP_LT: result = a < b; break;
            case OP_LE: result = a <= b; break;
            case OP_GT: result = a > b; break;
            case OP_GE: result = a >= b; break;
            default: break;
        }
    } else {
        if (expr->as.binary.op != OP_EQ && expr->as.binary.op != OP_NE) {
            return fail(ip, true, expr->line, expr->column, "only == and != apply to bool");
        }
        result = expr->as.binary.op == OP_EQ ? left.as.boolean == right.as.boolean
                                             : left.as.boolean != right.as.boolean;
    }
    out->kind = MINI_VAL_BOOL;
    out->as.boolean = result;
    return true;
}

static bool eval_arithmetic(interp *ip, const node *expr, mini_value *out) {
    mini_value left;
    mini_value right;
    if (!eval_expr(ip, expr->as.binary.left, &left) ||
        !eval_expr(ip, expr->as.binary.right, &right)) {
        return false;
    }
    if (left.kind != MINI_VAL_INT || right.kind != MINI_VAL_INT) {
        return fail(ip, true, expr->line, expr->column, "arithmetic operands must be numbers");
    }
    int64_t a = left.as.integer;
    int64_t b = right.as.integer;
    out->kind = MINI_VAL_INT;
    switch (expr->as.binary.op) {
        case OP_ADD:
            out->as.integer = a + b;
            return true;
        case OP_SUB:
            out->as.integer = a - b;
            return true;
        case OP_MUL:
            out->as.integer = a * b;
            return true;
        case OP_DIV:
            if (b == 0) {
                return fail(ip, true, expr->line, expr->column, "division by zero");
            }
            out->as.integer = a / b;
            return true;
        default:
            return fail(ip, true, expr->line, expr->column, "unknown operator");
    }
}

static bool eval_expr(interp *ip, const node *expr, mini_value *out) {
    switch (expr->kind) {
        case NODE_NUMBER_LITERAL:
            out->kind = MINI_VAL_INT;
            out->as.integer = expr->as.int_value;
            return true;
        case NODE_BOOL_LITERAL:
            out->kind = MINI_VAL_BOOL;
            out->as.boolean = expr->as.bool_value;
            return true;
        case NODE_IDENTIFIER: {
            env_entry *entry = lookup(ip, expr->as.identifier.name, expr->as.identifier.len);
            if (entry == NULL) {
                return failf(ip, expr->line, expr->column, "undefined name: %.*s",
                             (int)expr->as.identifier.len, expr->as.identifier.name);
            }
            *out = entry->value;
            return true;
        }
        case NODE_GROUP_EXPR:
            return eval_expr(ip, expr->as.group.inner, out);
        case NODE_UNARY_EXPR: {
            mini_value operand;
            if (!eval_expr(ip, expr->as.unary.operand, &operand)) {
                return false;
            }
            if (expr->as.unary.op == OP_NOT) {
                if (operand.kind != MINI_VAL_BOOL) {
                    return fail(ip, true, expr->line, expr->column,
                                "operand of ! must be bool");
                }
                out->kind = MINI_VAL_BOOL;
                out->as.boolean = !operand.as.boolean;
                return true;
            }
            if (operand.kind != MINI_VAL_INT) {
                return fail(ip, true, expr->line, expr->column,
                            "operand of unary - must be a number");
            }
            out->kind = MINI_VAL_INT;
            out->as.integer = -operand.as.integer;
            return true;
        }
        case NODE_BINARY_EXPR:
            if (expr->as.binary.op == OP_AND || expr->as.binary.op == OP_OR) {
                return eval_logical(ip, expr, out);
            }
            if (expr->as.binary.op >= OP_EQ && expr->as.binary.op <= OP_GE) {
                return eval_comparison(ip, expr, out);
            }
            return eval_arithmetic(ip, expr, out);
        case NODE_RANGE_EXPR: {
            mini_value start;
            mini_value end;
            mini_value step = {.kind = MINI_VAL_INT, .as.integer = 1};
            if (!eval_expr(ip, expr->as.range.start, &start) ||
                !eval_expr(ip, expr->as.range.end, &end)) {
                return false;
            }
            if (expr->as.range.step != NULL && !eval_expr(ip, expr->as.range.step, &step)) {
                return false;
            }
            if (start.kind != MINI_VAL_INT || end.kind != MINI_VAL_INT ||
                step.kind != MINI_VAL_INT) {
                return fail(ip, true, expr->line, expr->column, "range bounds must be numbers");
            }
            if (step.as.integer <= 0) {
                return fail(ip, true, expr->line, expr->column, "range step must be positive");
            }
            out->kind = MINI_VAL_RANGE;
            out->as.range.start = start.as.integer;
            out->as.range.end = end.as.integer;
            out->as.range.step = step.as.integer;
            out->as.range.inclusive = expr->as.range.inclusive_end;
            return true;
        }
        default:
            return fail(ip, true, expr->line, expr->column, "expression cannot be evaluated");
    }
}

static exec_status exec_stmt(interp *ip, const node *stmt);

static exec_status exec_block(interp *ip, const node *block) {
    if (!push_scope(ip)) {
        return EXEC_NORMAL;
    }
    exec_status status = EXEC_NORMAL;
    for (size_t i = 0; i < block->as.block.statements.len; ++i) {
        status = exec_stmt(ip, block->as.block.statements.items[i]);
        if (status != EXEC_NORMAL || ip->failed) {
            break;
        }
    }
    pop_scope(ip);
    return status;
}

static bool eval_condition(interp *ip, const node *expr, const char *context, bool *out) {
    mini_value value;
    if (!eval_expr(ip, expr, &value)) {
        return false;
    }
    if (value.kind != MINI_VAL_BOOL) {
        return failf(ip, expr->line, expr->column, "%s must be bool", context);
    }
    *out = value.as.boolean;
    return true;
}

static exec_status exec_assignment(interp *ip, const node *stmt) {
    mini_value value;
    if (!eval_expr(ip, stmt->as.assign.value, &value)) {
        return EXEC_NORMAL;
    }
    env_entry *entry = lookup(ip, stmt->as.assign.name, stmt->as.assign.len);
    if (entry == NULL) {
        failf(ip, stmt->line, stmt->column, "undefined name: %.*s", (int)stmt->as.assign.len,
              stmt->as.assign.name);
        return EXEC_NORMAL;
    }
    if (entry->is_const) {
        failf(ip, stmt->line, stmt->column, "cannot assign to const: %.*s", (int)entry->len,
              entry->name);
        return EXEC_NORMAL;
    }
    if (stmt->as.assign.op == OP_ASSIGN) {
        entry->value = value;
        return EXEC_NORMAL;
    }
    if (entry->value.kind != MINI_VAL_INT || value.kind != MINI_VAL_INT) {
        fail(ip, true, stmt->line, stmt->column, "compound assignment needs numbers");
        return EXEC_NORMAL;
    }
    int64_t a = entry->value.as.integer;
    int64_t b = value.as.integer;
    int64_t result = a;
    switch (stmt->as.assign.op) {
        case OP_ADD: result = a + b; break;
        case OP_SUB: result = a - b; break;
        case OP_MUL: result = a * b; break;
        case OP_DIV:
            if (b == 0) {
                fail(ip, true, stmt->line, stmt->column, "division by zero");
                return EXEC_NORMAL;
            }
            result = a / b;
            break;
        default: break;
    }
    entry->value.kind = MINI_VAL_INT;
    entry->value.as.integer = result;
    return EXEC_NORMAL;
}

static exec_status exec_stmt(interp *ip, const node *stmt) {
    switch (stmt->kind) {
        case NODE_BLOCK_STMT:
            return exec_block(ip, stmt);
        case NODE_BINDING_STMT: {
            mini_value value;
            if (!eval_expr(ip, stmt->as.binding.initializer, &value)) {
                return EXEC_NORMAL;
            }
            define(ip, stmt->as.binding.name, stmt->as.binding.len, value,
                   stmt->as.binding.is_const, stmt->line, stmt->column);
            return EXEC_NORMAL;
        }
        case NODE_ASSIGN_STMT:
            return exec_assignment(ip, stmt);
        case NODE_IF_STMT: {
            bool cond = false;
            if (!eval_condition(ip, stmt->as.if_stmt.condition, "if condition", &cond)) {
                return EXEC_NORMAL;
            }
            if (cond) {
                return exec_stmt(ip, stmt->as.if_stmt.then_branch);
            }
            if (stmt->as.if_stmt.else_branch != NULL) {
                return exec_stmt(ip, stmt->as.if_stmt.else_branch);
            }
            return EXEC_NORMAL;
        }
        case NODE_WHILE_STMT:
            for (;;) {
                bool cond = false;
                if (!eval_condition(ip, stmt->as.while_stmt.condition, "while condition", &cond)) {
                    return EXEC_NORMAL;
                }
                if (!cond) {
                    return EXEC_NORMAL;
                }
                exec_status status = exec_stmt(ip, stmt->as.while_stmt.body);
                if (status == EXEC_BREAK) {
                    return EXEC_NORMAL;
                }
                if (status == EXEC_CONTINUE) {
                    continue;
                }
                if (ip->failed) {
                    return EXEC_NORMAL;
                }
            }
        case NODE_LOOP_STMT:
            for (;;) {
                exec_status status = exec_stmt(ip, stmt->as.loop_stmt.body);
                if (status == EXEC_BREAK) {
                    return EXEC_NORMAL;
                }
                if (status == EXEC_CONTINUE) {
                    continue;
                }
                if (ip->failed) {
                    return EXEC_NORMAL;
                }
            }
        case NODE_FOR_STMT: {
            mini_value iterable;
            if (!eval_expr(ip, stmt->as.for_stmt.iterable, &iterable)) {
                return EXEC_NORMAL;
            }
            if (iterable.kind != MINI_VAL_RANGE) {
                fail(ip, true, stmt->line, stmt->column, "for source must be a range");
                return EXEC_NORMAL;
            }
            int64_t count = range_count(&iterable.as.range);
            for (int64_t i = 0; i < count; ++i) {
                if (!push_scope(ip)) {
                    return EXEC_NORMAL;
                }
                mini_value item = {.kind = MINI_VAL_INT,
                                   .as.integer = range_at(&iterable.as.range, i)};
                define(ip, stmt->as.for_stmt.name, stmt->as.for_stmt.len, item, false,
                       stmt->line, stmt->column);
                exec_status status = exec_stmt(ip, stmt->as.for_stmt.body);
                pop_scope(ip);
                if (status == EXEC_BREAK) {
                    return EXEC_NORMAL;
                }
                if (status == EXEC_CONTINUE) {
                    continue;
                }
                if (ip->failed) {
                    return EXEC_NORMAL;
                }
            }
            return EXEC_NORMAL;
        }
        case NODE_BREAK_STMT:
            return EXEC_BREAK;
        case NODE_CONTINUE_STMT:
            return EXEC_CONTINUE;
        case NODE_PRINT_STMT: {
            mini_value value;
            if (!eval_expr(ip, stmt->as.print_stmt.value, &value)) {
                return EXEC_NORMAL;
            }
            print_value(ip, value);
            return EXEC_NORMAL;
        }
        default:
            return EXEC_NORMAL;
    }
}

bool mini_run_program(const node *program, mdr_buffer *out, mdr_mini_error *error) {
    interp ip = {.out = out, .error = error};
    (void)exec_block(&ip, program);
    free(ip.items);
    free(ip.marks);
    return !ip.failed;
}