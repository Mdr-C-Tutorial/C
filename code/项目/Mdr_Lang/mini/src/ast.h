#ifndef MDR_MINI_AST_H
#define MDR_MINI_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct arena_chunk arena_chunk;

typedef struct {
    arena_chunk *head;
} arena;

void arena_init(arena *a);
void *arena_alloc(arena *a, size_t size);
void arena_free(arena *a);

typedef enum {
    NODE_NUMBER_LITERAL = 0,
    NODE_BOOL_LITERAL,
    NODE_IDENTIFIER,
    NODE_BINARY_EXPR,
    NODE_UNARY_EXPR,
    NODE_RANGE_EXPR,
    NODE_GROUP_EXPR,
    NODE_BINDING_STMT,
    NODE_ASSIGN_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_LOOP_STMT,
    NODE_FOR_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,
    NODE_BLOCK_STMT,
    NODE_PRINT_STMT
} node_kind;

typedef enum {
    OP_ADD = 0,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_NEG,
    OP_NOT,
    OP_ASSIGN
} operator_kind;

typedef struct node node;
typedef struct node_list node_list;

struct node_list {
    node **items;
    size_t len;
    size_t cap;
};

struct node {
    node_kind kind;
    size_t line;
    size_t column;
    union {
        int64_t int_value;
        bool bool_value;
        struct {
            const char *name;
            size_t len;
        } identifier;
        struct {
            operator_kind op;
            node *left;
            node *right;
        } binary;
        struct {
            operator_kind op;
            node *operand;
        } unary;
        struct {
            node *start;
            node *end;
            node *step;
            bool inclusive_end;
        } range;
        struct {
            node *inner;
        } group;
        struct {
            const char *name;
            size_t len;
            node *initializer;
            bool is_const;
        } binding;
        struct {
            const char *name;
            size_t len;
            node *value;
            operator_kind op;
        } assign;
        struct {
            node *condition;
            node *then_branch;
            node *else_branch;
        } if_stmt;
        struct {
            node *condition;
            node *body;
        } while_stmt;
        struct {
            node *body;
        } loop_stmt;
        struct {
            const char *name;
            size_t len;
            node *iterable;
            node *body;
        } for_stmt;
        struct {
            node_list statements;
        } block;
        struct {
            node *value;
        } print_stmt;
    } as;
};

node *node_new(arena *a, node_kind kind);
bool node_list_push(arena *a, node_list *list, node *item);

#endif