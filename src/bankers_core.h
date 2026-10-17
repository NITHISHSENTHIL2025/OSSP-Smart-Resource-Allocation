#ifndef BANKERS_CORE_H
#define BANKERS_CORE_H
/* Bounds: 1 <= processes <= 20, 1 <= resource types <= 10. All vectors are flat row-major. */
int banker_validate(int n, int m, const int *available, const int *allocated, const int *maximum);
/* returns: 1 SAFE, 0 UNSAFE, -1 invalid. seq size n, worksteps size n*m. */
int banker_safety(int n, int m, const int *available, const int *allocated, const int *maximum, int *seq, int *worksteps);
/* request statuses: 0 granted, 1 exceeds remaining need, 2 exceeds available, 3 unsafe after tentative allocation, -1 invalid */
int banker_request(int n, int m, int *available, int *allocated, const int *maximum, int pid, const int *request, int *seq);
/* release statuses: 0 released, 1 exceeds allocation, -1 invalid */
int banker_release(int n, int m, int *available, int *allocated, const int *maximum, int pid, const int *release);
#endif
