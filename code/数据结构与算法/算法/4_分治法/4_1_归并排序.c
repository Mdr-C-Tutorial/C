/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>

int N[10] = {13, 5, 9, 6, 11, 2, 4, 7, 1, 8};

void merge(int A[], const int p, const int mid, const int r) {
    const int n1 = mid - p + 1;
    const int n2 = r - mid;
    int *L = malloc((n1 + 1) * sizeof(int));
    int *R = malloc((n2 + 1) * sizeof(int));
    for (int i = 0; i < n1; i++) {
        L[i] = A[p + i];
    }
    L[n1] = INT_MAX;
    for (int i = 0; i < n2; i++) {
        R[i] = A[(mid + 1) + i];
    }
    R[n2] = INT_MAX;
    int i = 0, j = 0, k = p;
#if 0
    while (i < n1 || j < n2) {
        if (L[i] <= R[j]) {
            A[k] = L[i];
            i += 1;
        } else {
            A[k] = R[j];
            j += 1;
        }
        k += 1;
    }
#else
    for (; k <= r; k++) {
        if (L[i] <= R[j]) {
            A[k] = L[i];
            i += 1;
        } else {
            A[k] = R[j];
            j += 1;
        }
    }
#endif
    free(L);
    free(R);
}

void merge_sort(int A[], int p, int r) {
    if (p < r) {
        int mid = (p + r) / 2;
        merge_sort(A, p, mid);
        merge_sort(A, mid + 1, r);
        merge(A, p, mid, r);
    }
}

int main(void) {
    merge_sort(N, 0, 9);
    for (int i = 0; i < 10; i++) {
        printf("%d ", N[i]);
    }
    return 0;
}
