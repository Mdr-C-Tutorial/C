#include "stats.h"

#include <math.h>

double mean(const double *values, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += values[i];
    }
    return sum / n;
}

double spread(const double *values, int n) {
    double m = mean(values, n);
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += pow(values[i] - m, 2.0);
    }
    return sqrt(sum / n);
}
