#include <stdio.h>
#include <stdlib.h>

enum { T = 2, MAX_KEYS = 2 * T - 1, MAX_CHILDREN = 2 * T };

typedef struct BTreeNode {
    int n;
    int leaf;
    int keys[MAX_KEYS];
    struct BTreeNode *child[MAX_CHILDREN];
} BTreeNode;

typedef struct {
    BTreeNode *root;
} BTree;

static BTreeNode *bt_new_node(int leaf) {
    BTreeNode *x = (BTreeNode *)calloc(1, sizeof(*x));
    if (x == NULL) {
        return NULL;
    }
    x->leaf = leaf;
    return x;
}

static void bt_destroy_node(BTreeNode *x) {
    if (x == NULL) {
        return;
    }
    if (!x->leaf) {
        for (int i = 0; i <= x->n; ++i) {
            bt_destroy_node(x->child[i]);
        }
    }
    free(x);
}

static void bt_destroy(BTree *t) {
    bt_destroy_node(t->root);
    t->root = NULL;
}

static const BTreeNode *bt_search(const BTreeNode *x, int k, int *pos) {
    int i = 0;
    while (i < x->n && k > x->keys[i]) {
        ++i;
    }
    if (i < x->n && k == x->keys[i]) {
        if (pos != NULL) {
            *pos = i;
        }
        return x;
    }
    if (x->leaf) {
        return NULL;
    }
    return bt_search(x->child[i], k, pos);
}

static void bt_traverse(const BTreeNode *x) {
    if (x == NULL) {
        return;
    }
    for (int i = 0; i < x->n; ++i) {
        if (!x->leaf) {
            bt_traverse(x->child[i]);
        }
        printf("%d ", x->keys[i]);
    }
    if (!x->leaf) {
        bt_traverse(x->child[x->n]);
    }
}

static void split_child(BTreeNode *x, int i) {
    BTreeNode *y = x->child[i];
    BTreeNode *z = bt_new_node(y->leaf);
    if (z == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    z->n = T - 1;
    for (int j = 0; j < T - 1; ++j) {
        z->keys[j] = y->keys[j + T];
    }
    if (!y->leaf) {
        for (int j = 0; j < T; ++j) {
            z->child[j] = y->child[j + T];
        }
    }

    y->n = T - 1;

    for (int j = x->n; j >= i + 1; --j) {
        x->child[j + 1] = x->child[j];
    }
    x->child[i + 1] = z;

    for (int j = x->n - 1; j >= i; --j) {
        x->keys[j + 1] = x->keys[j];
    }
    x->keys[i] = y->keys[T - 1];
    x->n += 1;
}

static void insert_nonfull(BTreeNode *x, int k) {
    int i = x->n - 1;

    if (x->leaf) {
        while (i >= 0 && k < x->keys[i]) {
            x->keys[i + 1] = x->keys[i];
            --i;
        }
        x->keys[i + 1] = k;
        x->n += 1;
        return;
    }

    while (i >= 0 && k < x->keys[i]) {
        --i;
    }
    ++i;

    if (x->child[i]->n == MAX_KEYS) {
        split_child(x, i);
        if (k > x->keys[i]) {
            ++i;
        }
    }
    insert_nonfull(x->child[i], k);
}

static void bt_insert(BTree *t, int k) {
    if (t->root == NULL) {
        t->root = bt_new_node(1);
        if (t->root == NULL) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        t->root->keys[0] = k;
        t->root->n = 1;
        return;
    }

    if (bt_search(t->root, k, NULL) != NULL) {
        return;
    }

    BTreeNode *r = t->root;
    if (r->n == MAX_KEYS) {
        BTreeNode *s = bt_new_node(0);
        if (s == NULL) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        t->root = s;
        s->child[0] = r;
        split_child(s, 0);

        int i = 0;
        if (k > s->keys[0]) {
            i = 1;
        }
        insert_nonfull(s->child[i], k);
    } else {
        insert_nonfull(r, k);
    }
}

static int find_key(const BTreeNode *x, int k) {
    int idx = 0;
    while (idx < x->n && x->keys[idx] < k) {
        ++idx;
    }
    return idx;
}

static int get_pred(BTreeNode *x, int idx) {
    BTreeNode *cur = x->child[idx];
    while (!cur->leaf) {
        cur = cur->child[cur->n];
    }
    return cur->keys[cur->n - 1];
}

static int get_succ(BTreeNode *x, int idx) {
    BTreeNode *cur = x->child[idx + 1];
    while (!cur->leaf) {
        cur = cur->child[0];
    }
    return cur->keys[0];
}

static void remove_from_leaf(BTreeNode *x, int idx) {
    for (int i = idx + 1; i < x->n; ++i) {
        x->keys[i - 1] = x->keys[i];
    }
    x->n -= 1;
}

static void merge(BTreeNode *x, int idx) {
    BTreeNode *child = x->child[idx];
    BTreeNode *sibling = x->child[idx + 1];

    child->keys[T - 1] = x->keys[idx];
    for (int i = 0; i < sibling->n; ++i) {
        child->keys[i + T] = sibling->keys[i];
    }
    if (!child->leaf) {
        for (int i = 0; i <= sibling->n; ++i) {
            child->child[i + T] = sibling->child[i];
        }
    }
    child->n += sibling->n + 1;

    for (int i = idx + 1; i < x->n; ++i) {
        x->keys[i - 1] = x->keys[i];
    }
    for (int i = idx + 2; i <= x->n; ++i) {
        x->child[i - 1] = x->child[i];
    }
    x->n -= 1;
    free(sibling);
}

static void borrow_from_prev(BTreeNode *x, int idx) {
    BTreeNode *child = x->child[idx];
    BTreeNode *sibling = x->child[idx - 1];

    for (int i = child->n - 1; i >= 0; --i) {
        child->keys[i + 1] = child->keys[i];
    }
    if (!child->leaf) {
        for (int i = child->n; i >= 0; --i) {
            child->child[i + 1] = child->child[i];
        }
    }

    child->keys[0] = x->keys[idx - 1];
    if (!child->leaf) {
        child->child[0] = sibling->child[sibling->n];
    }

    x->keys[idx - 1] = sibling->keys[sibling->n - 1];
    child->n += 1;
    sibling->n -= 1;
}

static void borrow_from_next(BTreeNode *x, int idx) {
    BTreeNode *child = x->child[idx];
    BTreeNode *sibling = x->child[idx + 1];

    child->keys[child->n] = x->keys[idx];
    if (!child->leaf) {
        child->child[child->n + 1] = sibling->child[0];
    }
    x->keys[idx] = sibling->keys[0];

    for (int i = 1; i < sibling->n; ++i) {
        sibling->keys[i - 1] = sibling->keys[i];
    }
    if (!sibling->leaf) {
        for (int i = 1; i <= sibling->n; ++i) {
            sibling->child[i - 1] = sibling->child[i];
        }
    }

    child->n += 1;
    sibling->n -= 1;
}

static void fill(BTreeNode *x, int idx) {
    if (idx != 0 && x->child[idx - 1]->n >= T) {
        borrow_from_prev(x, idx);
    } else if (idx != x->n && x->child[idx + 1]->n >= T) {
        borrow_from_next(x, idx);
    } else if (idx != x->n) {
        merge(x, idx);
    } else {
        merge(x, idx - 1);
    }
}

static void delete_from_node(BTreeNode *x, int k);

static void remove_from_nonleaf(BTreeNode *x, int idx) {
    int k = x->keys[idx];

    if (x->child[idx]->n >= T) {
        int pred = get_pred(x, idx);
        x->keys[idx] = pred;
        delete_from_node(x->child[idx], pred);
    } else if (x->child[idx + 1]->n >= T) {
        int succ = get_succ(x, idx);
        x->keys[idx] = succ;
        delete_from_node(x->child[idx + 1], succ);
    } else {
        merge(x, idx);
        delete_from_node(x->child[idx], k);
    }
}

static void delete_from_node(BTreeNode *x, int k) {
    int idx = find_key(x, k);

    if (idx < x->n && x->keys[idx] == k) {
        if (x->leaf) {
            remove_from_leaf(x, idx);
        } else {
            remove_from_nonleaf(x, idx);
        }
        return;
    }

    if (x->leaf) {
        return;
    }

    int was_last = (idx == x->n);
    if (x->child[idx]->n < T) {
        fill(x, idx);
    }

    if (was_last && idx > x->n) {
        delete_from_node(x->child[idx - 1], k);
    } else {
        delete_from_node(x->child[idx], k);
    }
}

static void bt_delete(BTree *t, int k) {
    if (t->root == NULL) {
        return;
    }

    delete_from_node(t->root, k);

    if (t->root->n == 0) {
        BTreeNode *old = t->root;
        if (old->leaf) {
            t->root = NULL;
        } else {
            t->root = old->child[0];
        }
        free(old);
    }
}

int main(void) {
    BTree t = {0};
    int data[] = {10, 20, 5, 6, 12, 30, 7, 17};
    int n = (int)(sizeof(data) / sizeof(data[0]));

    for (int i = 0; i < n; ++i) {
        bt_insert(&t, data[i]);
    }

    printf("inorder: ");
    bt_traverse(t.root);
    printf("\n");

    int q1 = 6;
    int q2 = 15;
    printf("search %d: %s\n", q1, bt_search(t.root, q1, NULL) != NULL ? "found" : "not found");
    printf("search %d: %s\n", q2, bt_search(t.root, q2, NULL) != NULL ? "found" : "not found");

    int removed[] = {6, 13, 7, 4, 2, 16};
    int m = (int)(sizeof(removed) / sizeof(removed[0]));
    for (int i = 0; i < m; ++i) {
        printf("delete %d\n", removed[i]);
        bt_delete(&t, removed[i]);
        printf("inorder: ");
        bt_traverse(t.root);
        printf("\n");
    }

    int q3 = 6;
    int q4 = 7;
    printf("search %d: %s\n", q3, bt_search(t.root, q3, NULL) != NULL ? "found" : "not found");
    printf("search %d: %s\n", q4, bt_search(t.root, q4, NULL) != NULL ? "found" : "not found");

    bt_destroy(&t);
    return 0;
}
