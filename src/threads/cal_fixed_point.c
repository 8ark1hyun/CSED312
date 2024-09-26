#include "/root/pintos/src/threads/cal_fixed_point.h"

/* 정수를 고정 소수점으로 변환 */
int convert_to_fixed_point(int n) {
    return n * FIXED_POINT_SHIFT;
}

/* 고정 소수점을 정수로 변환 (소수점 이하 내림) */
int convert_to_int(int x) {
    return x / FIXED_POINT_SHIFT;
}

/* 고정 소수점을 정수로 변환 (반올림) */
int convert_to_int_nearest(int x) {
    if (x >= 0) 
        return (x + FIXED_POINT_SHIFT / 2) / FIXED_POINT_SHIFT;
    else 
        return (x - FIXED_POINT_SHIFT / 2) / FIXED_POINT_SHIFT;
}

/* 두 고정 소수점 값을 더함 */
int add_fixed_point(int x, int y) {
    return x + y;
}

/* 두 고정 소수점 값을 뺌 */
int subtract_fixed_point(int x, int y) {
    return x - y;
}

/* 고정 소수점에 정수를 더함 */
int add_int_fixed_point(int x, int n) {
    return x + n * FIXED_POINT_SHIFT;
}

/* 고정 소수점에서 정수를 뺌 */
int subtract_int_fixed_point(int x, int n) {
    return x - n * FIXED_POINT_SHIFT;
}

/* 두 고정 소수점 값을 곱함 */
int multiply_fixed_point(int x, int y) {
    return ((int64_t)x) * y / FIXED_POINT_SHIFT;
}

/* 고정 소수점에 정수를 곱함 */
int multiply_int_fixed_point(int x, int n) {
    return x * n;
}

/* 두 고정 소수점 값을 나눔 */
int divide_fixed_point(int x, int y) {
    return ((int64_t)x) * FIXED_POINT_SHIFT / y;
}

/* 고정 소수점을 정수로 나눔 */
int divide_int_fixed_point(int x, int n) {
    return x / n;
}
