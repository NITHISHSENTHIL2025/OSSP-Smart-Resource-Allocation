/*
 * OSSP Hackathon 2026 - Problem 13
 * Smart Resource Allocation using Banker's Algorithm
 * Interactive, stateful Linux C11 console application.
 * No fixed data is loaded by default. Enter your own values at startup.
 * Compile: gcc -std=c11 -Wall -Wextra -pedantic -O2 src/bankers.c -o banker
 * Run: ./banker
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define MAX_P 20
#define MAX_R 10
#define LIMIT 1000000
#define HISTORY 100
#define STATE_FILE "banker_state.txt"

typedef struct {
    int n, m;
    int available[MAX_R];
    int total[MAX_R];
    int allocation[MAX_P][MAX_R];
    int maximum[MAX_P][MAX_R];
    int need[MAX_P][MAX_R];
} State;

typedef struct {
    int safe, length;
    int sequence[MAX_P];
    int work_after[MAX_P][MAX_R];
} Safety;

static State state;
static int loaded = 0;
static char history[HISTORY][220];
static int hist_count = 0;

static void add_history(const char *s) {
    if (hist_count == HISTORY) {
        memmove(history[0], history[1], (HISTORY - 1) * sizeof(history[0]));
        hist_count--;
    }
    snprintf(history[hist_count++], sizeof(history[0]), "%s", s);
}

/* Strict whole-line input: prevents scanf leaving broken input in stdin. */
static int read_line(char *out, size_t cap) {
    if (!fgets(out, (int)cap, stdin)) return 0;
    size_t n = strlen(out);
    if (n && out[n - 1] == '\n') out[n - 1] = '\0';
    else if (n == cap - 1) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
        puts("Too much input on one line. Please retry.");
        return -1;
    }
    return 1;
}

static int parse_vector(const char *text, int count, int *values) {
    const char *p = text;
    for (int i = 0; i < count; ++i) {
        while (isspace((unsigned char)*p)) ++p;
        if (!*p || *p == '-' || *p == '+') return 0;
        errno = 0;
        char *end;
        long value = strtol(p, &end, 10);
        if (errno || end == p || value < 0 || value > LIMIT) return 0;
        if (*end && !isspace((unsigned char)*end)) return 0;
        values[i] = (int)value;
        p = end;
    }
    while (isspace((unsigned char)*p)) ++p;
    return *p == '\0';
}

static int ask_values(const char *prompt, int count, int *values) {
    char line[512];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        int r = read_line(line, sizeof(line));
        if (r == 0) return 0;
        if (r == 1 && parse_vector(line, count, values)) return 1;
        printf("  INVALID. Enter exactly %d non-negative integers (0-%d), separated by spaces.\n", count, LIMIT);
    }
}

static int ask_int(const char *prompt, int minimum, int maximum, int *answer) {
    int value;
    for (;;) {
        if (!ask_values(prompt, 1, &value)) return 0;
        if (value >= minimum && value <= maximum) {
            *answer = value;
            return 1;
        }
        printf("  Enter a value between %d and %d.\n", minimum, maximum);
    }
}

static void vector_print(const int *v, int m) {
    printf("[");
    for (int j = 0; j < m; ++j) {
        printf("%d", v[j]);
        if (j != m - 1) printf(" ");
    }
    printf("]");
}

static void recalculate(State *s) {
    for (int j = 0; j < s->m; ++j) {
        s->total[j] = s->available[j];
        for (int i = 0; i < s->n; ++i) s->total[j] += s->allocation[i][j];
    }
    for (int i = 0; i < s->n; ++i)
        for (int j = 0; j < s->m; ++j)
            s->need[i][j] = s->maximum[i][j] - s->allocation[i][j];
}

static int validate(const State *s, char *reason, size_t n_reason) {
    if (s->n < 1 || s->n > MAX_P || s->m < 1 || s->m > MAX_R) {
        snprintf(reason, n_reason, "Invalid number of processes or resource types.");
        return 0;
    }
    for (int j = 0; j < s->m; ++j) {
        long long total = s->available[j];
        if (total < 0 || total > LIMIT) {
            snprintf(reason, n_reason, "Available R%d out of allowed range.", j);
            return 0;
        }
        for (int i = 0; i < s->n; ++i) {
            int a = s->allocation[i][j], m = s->maximum[i][j];
            if (a < 0 || a > LIMIT || m < 0 || m > LIMIT || a > m) {
                snprintf(reason, n_reason, "P%d R%d: Allocation must be <= Max.", i, j);
                return 0;
            }
            total += a;
        }
        if (total > INT_MAX) {
            snprintf(reason, n_reason, "Total resources overflow.");
            return 0;
        }
        for (int i = 0; i < s->n; ++i) {
            if ((long long)s->maximum[i][j] > total) {
                snprintf(reason, n_reason, "P%d R%d: Max demand exceeds total system resources (%lld).", i, j, total);
                return 0;
            }
        }
    }
    return 1;
}

static void show_state(const State *s) {
    printf("\n=============== LIVE RESOURCE STATE ===============\n");
    printf("Processes: %d | Resource types: %d\n", s->n, s->m);
    printf("Available: "); vector_print(s->available, s->m); printf("\n");
    printf("Total:     "); vector_print(s->total, s->m); printf("\n");
    printf("%-9s %-22s %-22s %-22s\n", "Process", "Allocation", "Maximum", "Need");
    puts("-------------------------------------------------------------------");
    for (int i = 0; i < s->n; ++i) {
        char a[128] = "", b[128] = "", c[128] = "", temp[20];
        for (int j = 0; j < s->m; ++j) {
            snprintf(temp, sizeof(temp), "%d%s", s->allocation[i][j], j + 1 == s->m ? "" : " "); strcat(a, temp);
            snprintf(temp, sizeof(temp), "%d%s", s->maximum[i][j], j + 1 == s->m ? "" : " "); strcat(b, temp);
            snprintf(temp, sizeof(temp), "%d%s", s->need[i][j], j + 1 == s->m ? "" : " "); strcat(c, temp);
        }
        printf("P%-8d %-22s %-22s %-22s\n", i, a, b, c);
    }
}

/* The safety algorithm simulates completion without changing State. */
static Safety safety(const State *s) {
    Safety result = {0};
    int work[MAX_R] = {0}, finished[MAX_P] = {0};
    for (int j = 0; j < s->m; ++j) work[j] = s->available[j];
    for (;;) {
        int progress = 0;
        for (int i = 0; i < s->n; ++i) {
            if (finished[i]) continue;
            int allowed = 1;
            for (int j = 0; j < s->m; ++j)
                if (s->need[i][j] > work[j]) allowed = 0;
            if (!allowed) continue;
            for (int j = 0; j < s->m; ++j) {
                work[j] += s->allocation[i][j];
                result.work_after[result.length][j] = work[j];
            }
            result.sequence[result.length++] = i;
            finished[i] = 1;
            progress = 1;
        }
        if (!progress || result.length == s->n) break;
    }
    result.safe = (result.length == s->n);
    return result;
}

static void show_safety(const State *s) {
    Safety r = safety(s);
    puts("\n================ SAFETY CHECK =====================");
    printf("Starting Work = "); vector_print(s->available, s->m); printf("\n");
    for (int k = 0; k < r.length; ++k) {
        printf("P%d can finish; releases Allocation; Work = ", r.sequence[k]);
        vector_print(r.work_after[k], s->m); printf("\n");
    }
    if (r.safe) {
        printf("RESULT: SAFE STATE\nSafe sequence: ");
        for (int k = 0; k < r.length; ++k) printf("%sP%d", k ? " -> " : "", r.sequence[k]);
        printf("\n");
    } else {
        printf("RESULT: UNSAFE STATE. Only %d of %d processes can finish.\n", r.length, s->n);
        puts("No complete safe sequence exists for this allocation state.");
    }
}

static void enter_custom(void) {
    State next = {0};
    if (!ask_int("Number of processes (1-20): ", 1, MAX_P, &next.n)) return;
    if (!ask_int("Number of resource types (1-10): ", 1, MAX_R, &next.m)) return;
    puts("Enter all vectors as space-separated values, e.g. 3 3 2");
    if (!ask_values("AVAILABLE resources: ", next.m, next.available)) return;
    for (int i = 0; i < next.n; ++i) {
        char prompt[100];
        printf("\n--- Process P%d ---\n", i);
        snprintf(prompt, sizeof(prompt), "P%d ALLOCATION: ", i);
        if (!ask_values(prompt, next.m, next.allocation[i])) return;
        snprintf(prompt, sizeof(prompt), "P%d MAXIMUM:    ", i);
        if (!ask_values(prompt, next.m, next.maximum[i])) return;
    }
    char reason[200];
    if (!validate(&next, reason, sizeof(reason))) {
        printf("DATASET REJECTED: %s\nYour previous state was NOT changed.\n", reason);
        add_history("Custom dataset rejected (original state retained)");
        return;
    }
    recalculate(&next);
    state = next;
    loaded = 1;
    add_history("Custom dataset entered successfully");
    puts("\nDATASET ACCEPTED! Your custom values are now active.");
    show_state(&state);
    show_safety(&state);
}

static void example(void) {
    State s = {0};
    s.n = 5; s.m = 3;
    int av[] = {3, 3, 2};
    int a[5][3] = {{0,1,0},{2,0,0},{3,0,2},{2,1,1},{0,0,2}};
    int max[5][3] = {{7,5,3},{3,2,2},{9,0,2},{2,2,2},{4,3,3}};
    for (int j = 0; j < 3; ++j) s.available[j] = av[j];
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 3; ++j) {
            s.allocation[i][j] = a[i][j];
            s.maximum[i][j] = max[i][j];
        }
    recalculate(&s);
    state = s; loaded = 1;
    add_history("Example dataset loaded explicitly by user");
    puts("Sample dataset loaded (only because you selected it).");
    show_state(&state);
}

static void do_request(void) {
    int p, req[MAX_R];
    if (!ask_int("Process ID (P0=0, P1=1, ...): ", 0, state.n - 1, &p)) return;
    if (!ask_values("REQUEST vector: ", state.m, req)) return;
    char record[220];
    for (int j = 0; j < state.m; ++j) {
        if (req[j] > state.need[p][j]) {
            printf("DENIED: Request for R%d (%d) exceeds remaining Need (%d).\n", j, req[j], state.need[p][j]);
            snprintf(record, sizeof(record), "P%d request denied: exceeds Need", p); add_history(record);
            return;
        }
    }
    for (int j = 0; j < state.m; ++j) {
        if (req[j] > state.available[j]) {
            printf("DENIED: Request for R%d (%d) exceeds Available (%d).\n", j, req[j], state.available[j]);
            snprintf(record, sizeof(record), "P%d request denied: insufficient Available", p); add_history(record);
            return;
        }
    }
    State temporary = state;
    for (int j = 0; j < state.m; ++j) {
        temporary.available[j] -= req[j];
        temporary.allocation[p][j] += req[j];
        temporary.need[p][j] -= req[j];
    }
    Safety result = safety(&temporary);
    if (!result.safe) {
        puts("DENIED: UNSAFE after tentative allocation. Original state preserved (rollback).");
        printf("Available is still "); vector_print(state.available, state.m); printf("\n");
        snprintf(record, sizeof(record), "P%d request denied: unsafe; rollback", p); add_history(record);
        return;
    }
    state = temporary;
    puts("GRANTED: The request leaves the system in a SAFE state.");
    printf("New Available: "); vector_print(state.available, state.m); printf("\n");
    printf("Safe sequence: ");
    for (int k = 0; k < result.length; ++k) printf("%sP%d", k ? " -> " : "", result.sequence[k]);
    printf("\n");
    snprintf(record, sizeof(record), "P%d request granted", p); add_history(record);
}

static void do_release(void) {
    int p, rel[MAX_R];
    if (!ask_int("Process ID: ", 0, state.n - 1, &p)) return;
    if (!ask_values("RELEASE vector: ", state.m, rel)) return;
    char record[220];
    for (int j = 0; j < state.m; ++j)
        if (rel[j] > state.allocation[p][j]) {
            printf("DENIED: P%d only holds %d of R%d.\n", p, state.allocation[p][j], j);
            snprintf(record, sizeof(record), "P%d release denied: exceeds Allocation", p); add_history(record);
            return;
        }
    for (int j = 0; j < state.m; ++j) {
        state.available[j] += rel[j];
        state.allocation[p][j] -= rel[j];
        state.need[p][j] += rel[j];
    }
    puts("RELEASE ACCEPTED: Resources returned to the available pool.");
    printf("Available: "); vector_print(state.available, state.m); printf("\n");
    snprintf(record, sizeof(record), "P%d released resources", p); add_history(record);
}

static void save_state(void) {
    FILE *f = fopen(STATE_FILE, "w");
    if (!f) { perror("Saving failed"); return; }
    fprintf(f, "BANKER_V2\n%d %d\n", state.n, state.m);
    for (int j = 0; j < state.m; ++j) fprintf(f, "%d%c", state.available[j], j + 1 == state.m ? '\n' : ' ');
    for (int i = 0; i < state.n; ++i) {
        for (int j = 0; j < state.m; ++j) fprintf(f, "%d%c", state.allocation[i][j], j + 1 == state.m ? '\n' : ' ');
        for (int j = 0; j < state.m; ++j) fprintf(f, "%d%c", state.maximum[i][j], j + 1 == state.m ? '\n' : ' ');
    }
    if (fclose(f) != 0) { perror("Could not finish saving"); return; }
    printf("SAVED to %s. Continue using the menu.\n", STATE_FILE);
    add_history("State saved to banker_state.txt");
}

static void load_state(void) {
    FILE *f = fopen(STATE_FILE, "r");
    if (!f) { perror("Loading failed"); return; }
    State s = {0};
    char header[32];
    int ok = fscanf(f, "%31s%d%d", header, &s.n, &s.m) == 3 && strcmp(header, "BANKER_V2") == 0;
    if (ok && (s.n < 1 || s.n > MAX_P || s.m < 1 || s.m > MAX_R)) ok = 0;
    if (ok) {
        for (int j = 0; j < s.m; ++j) if (fscanf(f, "%d", &s.available[j]) != 1) ok = 0;
        for (int i = 0; i < s.n; ++i) {
            for (int j = 0; j < s.m; ++j) if (fscanf(f, "%d", &s.allocation[i][j]) != 1) ok = 0;
            for (int j = 0; j < s.m; ++j) if (fscanf(f, "%d", &s.maximum[i][j]) != 1) ok = 0;
        }
    }
    if (ok) {
        char extra[2];
        if (fscanf(f, "%1s", extra) == 1) ok = 0;
    }
    fclose(f);
    char reason[200];
    if (!ok || !validate(&s, reason, sizeof(reason))) {
        puts("LOAD DENIED: Missing, malformed or invalid state. Current state preserved.");
        return;
    }
    recalculate(&s);
    state = s; loaded = 1;
    puts("State restored from banker_state.txt.");
    add_history("State loaded from banker_state.txt");
    show_state(&state);
}

static void show_history(void) {
    puts("\n================ SESSION HISTORY =================");
    if (!hist_count) { puts("No operations yet."); return; }
    for (int i = 0; i < hist_count; ++i) printf("%2d. %s\n", i + 1, history[i]);
}

static void menu(void) {
    puts("\n=================== MAIN MENU ====================");
    puts(" 1. Show resource matrices / live state");
    puts(" 2. Check SAFE / UNSAFE + step-by-step sequence");
    puts(" 3. REQUEST resources (grant or deny)");
    puts(" 4. RELEASE resources");
    puts(" 5. REPLACE with NEW custom dataset");
    puts(" 6. LOAD example dataset (optional)");
    puts(" 7. SAVE state to disk");
    puts(" 8. RESTORE saved state from disk");
    puts(" 9. View this session's operation history");
    puts(" 0. EXIT");
}

int main(void) {
    int choice = 0;
    puts("===================================================");
    puts(" OSSP HACKATHON | SMART RESOURCE ALLOCATION MANAGER");
    puts(" Banker's Algorithm | Interactive Linux C Program");
    puts("===================================================");
    puts("No built-in input will run automatically.");
    puts("1 = Enter MY OWN inputs   2 = Optional sample   3 = Restore saved inputs   0 = Exit");
    if (!ask_int("Choose how to start: ", 0, 3, &choice)) return 0;
    if (choice == 0) return 0;
    if (choice == 1) enter_custom();
    if (choice == 2) example();
    if (choice == 3) load_state();

    while (1) {
        menu();
        if (!ask_int("Your choice: ", 0, 9, &choice)) { puts("Input ended. Goodbye."); return 0; }
        if (choice == 0) { puts("Goodbye! Resource manager closed."); return 0; }
        if (!loaded && choice != 5 && choice != 6 && choice != 8 && choice != 9) {
            puts("No dataset is loaded. Choose 5 for custom input, 6 for optional example, or 8 to restore.");
            continue;
        }
        switch (choice) {
            case 1: show_state(&state); break;
            case 2: show_safety(&state); break;
            case 3: do_request(); break;
            case 4: do_release(); break;
            case 5: enter_custom(); break;
            case 6: example(); break;
            case 7: save_state(); break;
            case 8: load_state(); break;
            case 9: show_history(); break;
            default: break;
        }
    }
}
