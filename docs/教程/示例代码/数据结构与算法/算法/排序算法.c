#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 辅助函数 ========== */

// 交换两个整数
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

// 打印数组
void print_array(int arr[], int n) {
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n - 1)
            printf(", ");
    }
    printf("]\n");
}

/* ========== 简单排序算法 ========== */

// 2.1 冒泡排序（Bubble Sort）
void bubble_sort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                swap(&arr[j], &arr[j + 1]);
            }
        }
    }
}

// 2.1.4 冒泡排序优化版本
void bubble_sort_optimized(int arr[], int n) {
    int swapped;
    for (int i = 0; i < n - 1; i++) {
        swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                swap(&arr[j], &arr[j + 1]);
                swapped = 1;
            }
        }
        // 如果这一轮没有发生交换，说明已经有序
        if (!swapped)
            break;
    }
}

// 2.2 选择排序（Selection Sort）
void selection_sort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j] < arr[min_idx]) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            swap(&arr[i], &arr[min_idx]);
        }
    }
}

// 2.3 插入排序（Insertion Sort）
void insertion_sort(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// 2.3.4 二分插入排序
void binary_insertion_sort(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int left = 0, right = i - 1;

        // 二分查找插入位置
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (arr[mid] > key) {
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        }

        // 移动元素
        for (int j = i - 1; j >= left; j--) {
            arr[j + 1] = arr[j];
        }
        arr[left] = key;
    }
}

// 2.4 希尔排序（Shell Sort）- 使用 Knuth 序列
void shell_sort(int arr[], int n) {
    // 生成 Knuth 序列: h = 1, 4, 13, 40, ...
    int gap = 1;
    while (gap < n / 3) {
        gap = gap * 3 + 1;
    }

    while (gap >= 1) {
        // 对每个子序列进行插入排序
        for (int i = gap; i < n; i++) {
            int key = arr[i];
            int j = i;
            while (j >= gap && arr[j - gap] > key) {
                arr[j] = arr[j - gap];
                j -= gap;
            }
            arr[j] = key;
        }
        gap /= 3;
    }
}

/* ========== 高效排序算法 ========== */

// 3.1 归并排序（Merge Sort）辅助函数 - 合并两个有序数组
void merge(int arr[], int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // 创建临时数组
    int *L = (int *)malloc((unsigned)n1 * sizeof(int));
    int *R = (int *)malloc((unsigned)n2 * sizeof(int));

    // 复制数据到临时数组
    for (int i = 0; i < n1; i++) {
        L[i] = arr[left + i];
    }
    for (int j = 0; j < n2; j++) {
        R[j] = arr[mid + 1 + j];
    }

    // 合并临时数组回原数组
    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }

    // 复制剩余元素
    while (i < n1) {
        arr[k++] = L[i++];
    }
    while (j < n2) {
        arr[k++] = R[j++];
    }

    free(L);
    free(R);
}

// 3.1 归并排序（Merge Sort）
void merge_sort_helper(int arr[], int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        merge_sort_helper(arr, left, mid);
        merge_sort_helper(arr, mid + 1, right);
        merge(arr, left, mid, right);
    }
}

void merge_sort(int arr[], int n) { merge_sort_helper(arr, 0, n - 1); }

// 3.2 快速排序（Quick Sort）- 分区操作
int partition(int arr[], int low, int high) {
    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return i + 1;
}

// 3.2 快速排序（Quick Sort）
void quick_sort_helper(int arr[], int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quick_sort_helper(arr, low, pi - 1);
        quick_sort_helper(arr, pi + 1, high);
    }
}

void quick_sort(int arr[], int n) { quick_sort_helper(arr, 0, n - 1); }

// 3.2.5 随机化快速排序
int random_partition(int arr[], int low, int high) {
    int random = low + rand() % (high - low + 1);
    swap(&arr[random], &arr[high]);
    return partition(arr, low, high);
}

void randomized_quick_sort_helper(int arr[], int low, int high) {
    if (low < high) {
        int pi = random_partition(arr, low, high);
        randomized_quick_sort_helper(arr, low, pi - 1);
        randomized_quick_sort_helper(arr, pi + 1, high);
    }
}

void randomized_quick_sort(int arr[], int n) {
    randomized_quick_sort_helper(arr, 0, n - 1);
}

// 3.2.6 三路快速排序
void three_way_quick_sort_helper(int arr[], int low, int high) {
    if (low >= high)
        return;

    int pivot = arr[low];
    int lt = low;    // arr[low..lt-1] < pivot
    int i = low + 1; // arr[lt..i-1] == pivot
    int gt = high;   // arr[gt+1..high] > pivot

    while (i <= gt) {
        if (arr[i] < pivot) {
            swap(&arr[lt++], &arr[i++]);
        } else if (arr[i] > pivot) {
            swap(&arr[i], &arr[gt--]);
        } else {
            i++;
        }
    }

    three_way_quick_sort_helper(arr, low, lt - 1);
    three_way_quick_sort_helper(arr, gt + 1, high);
}

void three_way_quick_sort(int arr[], int n) {
    three_way_quick_sort_helper(arr, 0, n - 1);
}

// 3.3 堆排序（Heap Sort）- 堆化
void heapify(int arr[], int n, int i) {
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < n && arr[left] > arr[largest]) {
        largest = left;
    }

    if (right < n && arr[right] > arr[largest]) {
        largest = right;
    }

    if (largest != i) {
        swap(&arr[i], &arr[largest]);
        heapify(arr, n, largest);
    }
}

// 3.3 堆排序（Heap Sort）
void heap_sort(int arr[], int n) {
    // 构建最大堆
    for (int i = n / 2 - 1; i >= 0; i--) {
        heapify(arr, n, i);
    }

    // 一个个从堆中取出元素
    for (int i = n - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);
        heapify(arr, i, 0);
    }
}

/* ========== 特殊排序算法 ========== */

// 4.1 计数排序（Counting Sort）
void counting_sort(int arr[], int n) {
    if (n <= 1)
        return;

    // 找出数组中的最大值和最小值
    int max = arr[0], min = arr[0];
    for (int i = 1; i < n; i++) {
        if (arr[i] > max)
            max = arr[i];
        if (arr[i] < min)
            min = arr[i];
    }

    int range = max - min + 1;
    int *count = (int *)calloc((unsigned)range, sizeof(int));
    int *output = (int *)malloc((unsigned)n * sizeof(int));

    // 统计每个元素出现的次数
    for (int i = 0; i < n; i++) {
        count[arr[i] - min]++;
    }

    // 累加计数数组
    for (int i = 1; i < range; i++) {
        count[i] += count[i - 1];
    }

    // 构建输出数组
    for (int i = n - 1; i >= 0; i--) {
        output[count[arr[i] - min] - 1] = arr[i];
        count[arr[i] - min]--;
    }

    // 复制回原数组
    for (int i = 0; i < n; i++) {
        arr[i] = output[i];
    }

    free(count);
    free(output);
}

// 4.2 基数排序（Radix Sort）辅助函数 - 按某一位进行计数排序
void counting_sort_for_radix(int arr[], int n, int exp) {
    int *output = (int *)malloc((unsigned)n * sizeof(int));
    int count[10] = {0};

    // 统计每个数字出现的次数
    for (int i = 0; i < n; i++) {
        count[(arr[i] / exp) % 10]++;
    }

    // 累加
    for (int i = 1; i < 10; i++) {
        count[i] += count[i - 1];
    }

    // 构建输出数组
    for (int i = n - 1; i >= 0; i--) {
        int digit = (arr[i] / exp) % 10;
        output[count[digit] - 1] = arr[i];
        count[digit]--;
    }

    // 复制回原数组
    for (int i = 0; i < n; i++) {
        arr[i] = output[i];
    }

    free(output);
}

// 4.2 基数排序（Radix Sort）- LSD
void radix_sort(int arr[], int n) {
    if (n <= 1)
        return;

    // 找出最大值以确定位数
    int max = arr[0];
    for (int i = 1; i < n; i++) {
        if (arr[i] > max)
            max = arr[i];
    }

    // 对每一位进行计数排序
    for (int exp = 1; max / exp > 0; exp *= 10) {
        counting_sort_for_radix(arr, n, exp);
    }
}

// 4.3 桶排序（Bucket Sort）
void bucket_sort(int arr[], int n) {
    if (n <= 1)
        return;

    // 找出最大值和最小值
    int max = arr[0], min = arr[0];
    for (int i = 1; i < n; i++) {
        if (arr[i] > max)
            max = arr[i];
        if (arr[i] < min)
            min = arr[i];
    }

    // 确定桶的数量
    int bucket_count = n;
    int range = max - min + 1;

    // 创建桶
    typedef struct {
        int *data;
        int size;
        int capacity;
    } Bucket;

    Bucket *buckets = (Bucket *)malloc((unsigned)bucket_count * sizeof(Bucket));
    for (int i = 0; i < bucket_count; i++) {
        buckets[i].data = (int *)malloc(10 * sizeof(int));
        buckets[i].size = 0;
        buckets[i].capacity = 10;
    }

    // 将元素分配到桶中
    for (int i = 0; i < n; i++) {
        int bucket_idx =
            (int)((long long)(arr[i] - min) * bucket_count / range);
        if (bucket_idx >= bucket_count)
            bucket_idx = bucket_count - 1;

        // 如果桶满了，扩容
        if (buckets[bucket_idx].size >= buckets[bucket_idx].capacity) {
            buckets[bucket_idx].capacity *= 2;
            buckets[bucket_idx].data = (int *)realloc(
                buckets[bucket_idx].data,
                (unsigned)buckets[bucket_idx].capacity * sizeof(int));
        }

        buckets[bucket_idx].data[buckets[bucket_idx].size++] = arr[i];
    }

    // 对每个桶进行排序（使用插入排序）
    for (int i = 0; i < bucket_count; i++) {
        insertion_sort(buckets[i].data, buckets[i].size);
    }

    // 合并桶
    int idx = 0;
    for (int i = 0; i < bucket_count; i++) {
        for (int j = 0; j < buckets[i].size; j++) {
            arr[idx++] = buckets[i].data[j];
        }
        free(buckets[i].data);
    }

    free(buckets);
}

/* ========== 测试函数 ========== */

// 测试排序算法
void test_sort_algorithm(const char *name, void (*sort_func)(int[], int),
                         int arr[], int n) {
    int *test_arr = (int *)malloc((unsigned)n * sizeof(int));
    memcpy(test_arr, arr, (unsigned)n * sizeof(int));

    printf("\n%s:\n", name);
    printf("排序前: ");
    print_array(test_arr, n);

    sort_func(test_arr, n);

    printf("排序后: ");
    print_array(test_arr, n);

    free(test_arr);
}

int main() {
    int arr[] = {64, 34, 25, 12, 22, 11, 90, 88, 45, 50, 23, 36, 18, 77};
    int n = sizeof(arr) / sizeof(arr[0]);

    printf("=== 排序算法测试 ===\n");

    // 简单排序算法
    test_sort_algorithm("冒泡排序", bubble_sort, arr, n);
    test_sort_algorithm("冒泡排序（优化）", bubble_sort_optimized, arr, n);
    test_sort_algorithm("选择排序", selection_sort, arr, n);
    test_sort_algorithm("插入排序", insertion_sort, arr, n);
    test_sort_algorithm("二分插入排序", binary_insertion_sort, arr, n);
    test_sort_algorithm("希尔排序", shell_sort, arr, n);

    // 高效排序算法
    test_sort_algorithm("归并排序", merge_sort, arr, n);
    test_sort_algorithm("快速排序", quick_sort, arr, n);
    test_sort_algorithm("随机化快速排序", randomized_quick_sort, arr, n);
    test_sort_algorithm("三路快速排序", three_way_quick_sort, arr, n);
    test_sort_algorithm("堆排序", heap_sort, arr, n);

    // 特殊排序算法
    test_sort_algorithm("计数排序", counting_sort, arr, n);
    test_sort_algorithm("基数排序", radix_sort, arr, n);
    test_sort_algorithm("桶排序", bucket_sort, arr, n);

    return 0;
}
