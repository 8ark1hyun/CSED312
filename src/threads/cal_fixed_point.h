#ifndef THREADS_CAL_FIXED_POINT_H
#define THREADS_CAL_FIXED_POINT_H

#define FIXED_POINT_SHIFT (1 << 14) // 소수 부분을 위한 고정 소수점 변환값
#define MAX_INT ((1 << 31) - 1)
#define MIN_INT (-(1 << 31))

/* 고정 소수점 연산 함수들의 프로토타입 선언 */
int convert_to_fixed_point(int n);         // 정수를 고정 소수점으로 변환
int convert_to_int(int x);                 // 고정 소수점을 정수로 변환 (내림)
int convert_to_int_nearest(int x);         // 고정 소수점을 정수로 변환 (반올림)
int add_fixed_point(int x, int y);         // 두 고정 소수점을 더함
int subtract_fixed_point(int x, int y);    // 두 고정 소수점을 뺌
int add_int_fixed_point(int x, int n);     // 고정 소수점에 정수를 더함
int subtract_int_fixed_point(int x, int n);// 고정 소수점에서 정수를 뺌
int multiply_fixed_point(int x, int y);    // 두 고정 소수점을 곱함
int multiply_int_fixed_point(int x, int n);// 고정 소수점에 정수를 곱함
int divide_fixed_point(int x, int y);      // 두 고정 소수점을 나눔
int divide_int_fixed_point(int x, int n);  // 고정 소수점을 정수로 나눔

#endif /* threads/cal_fixed_point.h */
