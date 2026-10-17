#include "bankers_core.h"
#include <limits.h>
#include <stddef.h>
#define MAX_P 20
#define MAX_R 10
#define MAX_UNIT 1000000

int banker_validate(int n, int m, const int *available, const int *allocated, const int *maximum) {
    if (n < 1 || n > MAX_P || m < 1 || m > MAX_R ||
        available == NULL || allocated == NULL || maximum == NULL) return 0;
    for (int j = 0; j < m; j++) {
        long long total = available[j];
        if (total < 0 || total > MAX_UNIT) return 0;
        for (int i = 0; i < n; i++) {
            int a = allocated[i * m + j], mx = maximum[i * m + j];
            if (a < 0 || mx < 0 || a > mx || a > MAX_UNIT || mx > MAX_UNIT) return 0;
            total += a;
        }
        if (total > INT_MAX) return 0;
        for (int i = 0; i < n; i++) {
            if ((long long)maximum[i * m + j] > total) return 0;
        }
    }
    return 1;
}

int banker_safety(int n, int m, const int *available, const int *allocated,
                  const int *maximum, int *seq, int *worksteps) {
    if (!banker_validate(n,m,available,allocated,maximum) || seq == NULL || worksteps == NULL) return -1;
    int work[MAX_R] = {0}, finished[MAX_P] = {0}, length = 0;
    for (int j = 0; j < m; j++) work[j] = available[j];
    while (length < n) {
        int found = 0;
        for (int i = 0; i < n; i++) {
            if (finished[i]) continue;
            int possible = 1;
            for (int j = 0; j < m; j++) {
                int need = maximum[i*m+j] - allocated[i*m+j];
                if (need > work[j]) { possible = 0; break; }
            }
            if (!possible) continue;
            finished[i] = 1;
            seq[length] = i;
            for (int j = 0; j < m; j++) {
                work[j] += allocated[i*m+j];
                worksteps[length*m+j] = work[j];
            }
            length++;
            found = 1;
        }
        if (!found) return 0;
    }
    return 1;
}

int banker_request(int n, int m, int *available, int *allocated, const int *maximum,
                   int pid, const int *request, int *seq) {
    if (!banker_validate(n,m,available,allocated,maximum) || request == NULL || seq == NULL || pid < 0 || pid >= n) return -1;
    for (int j = 0; j < m; j++) {
        int r = request[j];
        if (r < 0 || r > MAX_UNIT) return -1;
        if (r > maximum[pid*m+j] - allocated[pid*m+j]) return 1;
    }
    for (int j = 0; j < m; j++) if (request[j] > available[j]) return 2;
    /* Tentative allocation - rollback on unsafe result. */
    for (int j = 0; j < m; j++) {
        available[j] -= request[j];
        allocated[pid*m+j] += request[j];
    }
    int worksteps[MAX_P*MAX_R] = {0};
    int safe = banker_safety(n,m,available,allocated,maximum,seq,worksteps);
    if (safe == 1) return 0;
    for (int j = 0; j < m; j++) {
        available[j] += request[j];
        allocated[pid*m+j] -= request[j];
    }
    return safe == 0 ? 3 : -1;
}

int banker_release(int n, int m, int *available, int *allocated, const int *maximum,
                   int pid, const int *release) {
    if (!banker_validate(n,m,available,allocated,maximum) || release == NULL || pid < 0 || pid >= n) return -1;
    for (int j = 0; j < m; j++) {
        if (release[j] < 0 || release[j] > MAX_UNIT) return -1;
        if (release[j] > allocated[pid*m+j]) return 1;
    }
    for (int j = 0; j < m; j++) {
        available[j] += release[j];
        allocated[pid*m+j] -= release[j];
    }
    return 0;
}
