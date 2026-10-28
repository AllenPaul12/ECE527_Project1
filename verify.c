/* Independent verifier for the Leiserson-Saxe retiming implementation.
 *
 * Links against retiming.c and parser.c but shares no algorithm with them:
 *   1. W/D oracle   : enumerate every simple path (DFS).
 *   2. Period oracle: brute-force every retiming vector in [-R, R]^(n-1).
 *   3. r validator  : retimed weights >= 0 and measured period <= c_min.
 *
 * Build: gcc -Wall -Wextra -O2 -std=c11 verify.c retiming.c parser.c -o verify
 * Run  : ./verify [num_random_tests] [seed]  [circuit files...]
 *
 * Results go to stderr because the algorithm's own progress output on
 * stdout is silenced during testing.
 */

#include "retiming.h"
#include <time.h>

#ifdef _WIN32
#define NULL_DEVICE "NUL"
#else
#define NULL_DEVICE "/dev/null"
#endif

#define BRUTE_RANGE 4   /* r(v) searched in [-4, 4]; v1 fixed to 0 */
#define BRUTE_MAX_N 6   /* brute force limited to small circuits   */

/* ---------- independent clock-period measurement ---------- */

/* Returns clock period of circuit retimed by r (r[1..n]).
 * -1: some retimed edge weight is negative (illegal retiming)
 * -2: zero-weight cycle (combinational loop)                      */
static int period_of(const Circuit *c, const int *r) {
    int n = c->num_nodes;
    int indeg[MAX_NODES + 1] = {0};
    int arr[MAX_NODES + 1];
    int queue[MAX_NODES + 1];
    int head = 0, tail = 0, done = 0, best = 0;

    for (int i = 0; i < c->edge_count; i++) {
        int wr = c->edges[i].weight + r[c->edges[i].v] - r[c->edges[i].u];
        if (wr < 0) return -1;
        if (wr == 0) indeg[c->edges[i].v]++;
    }
    for (int i = 1; i <= n; i++) {
        arr[i] = c->delay[i];
        if (indeg[i] == 0) queue[tail++] = i;
    }
    while (head < tail) {
        int u = queue[head++];
        done++;
        if (arr[u] > best) best = arr[u];
        for (int i = 0; i < c->edge_count; i++) {
            int v = c->edges[i].v;
            if (c->edges[i].u != u) continue;
            if (c->edges[i].weight + r[v] - r[u] != 0) continue;
            if (arr[u] + c->delay[v] > arr[v]) arr[v] = arr[u] + c->delay[v];
            if (--indeg[v] == 0) queue[tail++] = v;
        }
    }
    return (done < n) ? -2 : best;
}

/* ---------- brute-force optimal period ---------- */

static int bf_best;
static void bf_rec(const Circuit *c, int *r, int idx) {
    if (idx > c->num_nodes) {
        int p = period_of(c, r);
        if (p >= 0 && (bf_best < 0 || p < bf_best)) bf_best = p;
        return;
    }
    for (int x = -BRUTE_RANGE; x <= BRUTE_RANGE; x++) {
        r[idx] = x;
        bf_rec(c, r, idx + 1);
    }
}

static int brute_force_min_period(const Circuit *c) {
    int r[MAX_NODES + 1] = {0};
    bf_best = -1;
    r[1] = 0;               /* uniform shifts do not change anything */
    bf_rec(c, r, 2);
    return bf_best;
}

/* ---------- brute-force W and D via simple-path enumeration ---------- */

static int pW[MAX_NODES + 1][MAX_NODES + 1];
static int pD[MAX_NODES + 1][MAX_NODES + 1];

static void path_dfs(const Circuit *c, int src, int cur, int wsum, int dsum,
                     int *visited) {
    /* dsum already includes delay of cur */
    if (wsum < pW[src][cur] || (wsum == pW[src][cur] && dsum > pD[src][cur])) {
        pW[src][cur] = wsum;
        pD[src][cur] = dsum;
    }
    for (int i = 0; i < c->edge_count; i++) {
        if (c->edges[i].u != cur) continue;
        int v = c->edges[i].v;
        if (visited[v]) continue;
        visited[v] = 1;
        path_dfs(c, src, v, wsum + c->edges[i].weight, dsum + c->delay[v], visited);
        visited[v] = 0;
    }
}

static int check_WD(const Circuit *c, const RetimingResult *res) {
    int n = c->num_nodes, bad = 0;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= n; j++) { pW[i][j] = INF; pD[i][j] = 0; }
    for (int s = 1; s <= n; s++) {
        int visited[MAX_NODES + 1] = {0};
        visited[s] = 1;
        path_dfs(c, s, s, 0, c->delay[s], visited);
    }
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= n; j++) {
            if (res->W[i][j] != pW[i][j] || res->D[i][j] != pD[i][j]) {
                fprintf(stderr, "    W/D mismatch at (%d,%d): program W=%d D=%d, oracle W=%d D=%d\n",
                        i, j, res->W[i][j], res->D[i][j], pW[i][j], pD[i][j]);
                bad = 1;
            }
        }
    return !bad;
}

/* ---------- check one circuit; returns 1 if all checks pass ---------- */

static int verify_circuit(Circuit *c, const char *label, int verbose) {
    static RetimingResult res;
    int n = c->num_nodes;
    int ok = 1;
    int r0[MAX_NODES + 1] = {0};
    int orig = period_of(c, r0);

    if (orig == -2) {
        /* Combinational loop: program must report infeasible */
        run_retiming_algorithm(c, &res);
        if (res.is_feasible) {
            fprintf(stderr, "[FAIL] %s: combinational loop but program claims feasible\n", label);
            return 0;
        }
        if (verbose) fprintf(stderr, "[PASS] %s: combinational loop correctly rejected\n", label);
        return 1;
    }

    run_retiming_algorithm(c, &res);

    if (n <= BRUTE_MAX_N && !check_WD(c, &res)) {
        fprintf(stderr, "[FAIL] %s: W/D matrices differ from path-enumeration oracle\n", label);
        ok = 0;
    }

    /* The program only searches c <= max_clock_cycle. If that bound is below
     * the true optimum it should say infeasible; if >= optimum it must find it. */
    int opt = (n <= BRUTE_MAX_N) ? brute_force_min_period(c) : -1;

    if (!res.is_feasible) {
        if (opt >= 0 && opt <= c->max_clock_cycle) {
            fprintf(stderr, "[FAIL] %s: program says infeasible for c<=%d but optimum is %d\n",
                    label, c->max_clock_cycle, opt);
            ok = 0;
        } else if (verbose) {
            fprintf(stderr, "       %s: infeasible for the given bound (true optimum %d)\n", label, opt);
        }
        if (ok && verbose) fprintf(stderr, "[PASS] %s\n", label);
        return ok;
    }

    int ach = period_of(c, res.r);
    if (ach < 0) {
        fprintf(stderr, "[FAIL] %s: returned r is illegal (code %d)\n", label, ach);
        ok = 0;
    } else if (ach > res.min_feasible_c) {
        fprintf(stderr, "[FAIL] %s: r achieves period %d, program claims %d\n",
                label, ach, res.min_feasible_c);
        ok = 0;
    }
    if (opt >= 0 && opt != res.min_feasible_c) {
        fprintf(stderr, "[FAIL] %s: program c_min=%d but brute-force optimum=%d\n",
                label, res.min_feasible_c, opt);
        ok = 0;
    }
    if (ok && verbose)
        fprintf(stderr, "[PASS] %s: original=%d  c_min=%d  achieved=%d  brute-force optimum=%d\n",
                label, orig, res.min_feasible_c, ach, opt);
    return ok;
}

/* ---------- random circuit generation ---------- */

static void random_circuit(Circuit *c) {
    int n = 2 + rand() % 4;                 /* 2..5 nodes */
    init_circuit(c, n);
    for (int i = 1; i <= n; i++) c->delay[i] = 1 + rand() % 5;
    int m = n + rand() % (n + 1);           /* n..2n edges */
    for (int i = 0; i < m; i++)
        add_edge(c, 1 + rand() % n, 1 + rand() % n, rand() % 3);
    int r0[MAX_NODES + 1] = {0};
    int p = period_of(c, r0);
    c->max_clock_cycle = (p > 0) ? p : 10;  /* start search at original period */
}

int main(int argc, char *argv[]) {
    int num_random = 2000;
    unsigned seed = 12345;
    int first_file = 1;

    if (argc > 1 && sscanf(argv[1], "%d", &num_random) == 1) first_file = 2;
    if (argc > 2 && sscanf(argv[2], "%u", &seed) == 1) first_file = 3;

    static Circuit c;
    int fails = 0, total = 0;

    /* Silence the algorithm's own progress prints on stdout */
    if (!freopen(NULL_DEVICE, "w", stdout)) {
        fprintf(stderr, "warning: could not silence stdout\n");
    }

    fprintf(stderr, "=== Circuit files ===\n");
    const char *defaults[] = {"example_class.txt", "5_node_pipeline.txt", "multi_loop.txt"};
    int nfiles = argc - first_file;
    for (int i = 0; i < (nfiles > 0 ? nfiles : 3); i++) {
        const char *f = (nfiles > 0) ? argv[first_file + i] : defaults[i];
        if (!parse_circuit_file(f, &c)) {
            fprintf(stderr, "[FAIL] could not parse %s\n", f);
            fails++; total++;
            continue;
        }
        total++;
        if (!verify_circuit(&c, f, 1)) fails++;
    }

    /* One explicit combinational-loop case (program must reject it) */
    init_circuit(&c, 2);
    c.delay[1] = 1; c.delay[2] = 1; c.max_clock_cycle = 5;
    add_edge(&c, 1, 2, 0); add_edge(&c, 2, 1, 0);
    total++;
    if (!verify_circuit(&c, "combinational loop (expected error above)", 1)) fails++;

    fprintf(stderr, "\n=== Random circuits (valid circuits only, n=2..5, seed=%u) ===\n", seed);
    srand(seed);
    int rtotal = 0, rfail = 0;
    for (int t = 0; t < num_random; t++) {
        int r0[MAX_NODES + 1] = {0};
        do { random_circuit(&c); } while (period_of(&c, r0) == -2);
        char label[32];
        snprintf(label, sizeof label, "random #%d", t);
        rtotal++;
        if (!verify_circuit(&c, label, 0)) rfail++;
    }
    fprintf(stderr, "%d random circuits, %d failures\n", rtotal, rfail);

    fails += rfail; total += rtotal;
    fprintf(stderr, "\n=== SUMMARY: %d/%d checks passed ===\n", total - fails, total);
    return fails ? 1 : 0;
}
