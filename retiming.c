/* Retiming Algorithm Implementation (Leiserson-Saxe)
   @author: Allen Harrison Paul
*/

#include "retiming.h"

/* ------------------------------------------------------------------
 * Helper type for computing W and D in a single Floyd-Warshall pass.
 *
 * Each path is weighted by the pair (registers, -delay), compared
 * lexicographically:
 *   - minimizing the first component gives W(u,v)
 *   - among minimum-register paths, minimizing the second component
 *     (i.e. maximizing delay) gives D(u,v)
 *
 * An edge u->v costs (w(e), -d(u)), so a path u -> ... -> v accumulates
 * the delays of every node except the last one. D(u,v) therefore adds
 * d(v) back at the end.
 * ------------------------------------------------------------------ */
typedef struct {
    int w;   /* register count */
    int nd;  /* negative accumulated delay (excluding the final node) */
} Pair;

static int pair_less(Pair a, Pair b) {
    return (a.w < b.w) || (a.w == b.w && a.nd < b.nd);
}

void init_circuit(Circuit *c, int num_nodes) {
    c->num_nodes = num_nodes;
    c->edge_count = 0;
    c->max_clock_cycle = 0;
    for (int i = 1; i <= num_nodes; i++) {
        c->delay[i] = 0;
        for (int j = 1; j <= num_nodes; j++) {
            c->w[i][j] = INF;
        }
    }
}

/* Returns 1 on success, 0 on error (bad indices or edge list full). */
int add_edge(Circuit *c, int u, int v, int registers) {
    if (u < 1 || u > c->num_nodes || v < 1 || v > c->num_nodes) {
        fprintf(stderr, "Error: edge (%d -> %d) has an out-of-range node\n", u, v);
        return 0;
    }
    if (c->edge_count >= MAX_NODES * MAX_NODES) {
        fprintf(stderr, "Error: too many edges\n");
        return 0;
    }

    /* For parallel edges, the matrix keeps the smallest register count */
    if (c->w[u][v] == INF || registers < c->w[u][v]) {
        c->w[u][v] = registers;
    }

    c->edges[c->edge_count].u = u;
    c->edges[c->edge_count].v = v;
    c->edges[c->edge_count].weight = registers;
    c->edge_count++;
    return 1;
}

void compute_W_and_D(const Circuit *c, RetimingResult *res) {
    int n = c->num_nodes;
    static Pair P[MAX_NODES + 1][MAX_NODES + 1];

    // Initialization: direct edges and trivial (empty) paths
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= n; j++) {
            if (i == j) {
                P[i][j].w = 0;
                P[i][j].nd = 0;
            } else if (c->w[i][j] != INF) {
                P[i][j].w = c->w[i][j];
                P[i][j].nd = -c->delay[i];
            } else {
                P[i][j].w = INF;
                P[i][j].nd = 0;
            }
        }
    }

    // Floyd-Warshall over lexicographic (registers, -delay) pairs
    for (int k = 1; k <= n; k++) {
        for (int i = 1; i <= n; i++) {
            if (P[i][k].w == INF) continue;
            for (int j = 1; j <= n; j++) {
                if (P[k][j].w == INF) continue;
                Pair cand;
                cand.w  = P[i][k].w  + P[k][j].w;
                cand.nd = P[i][k].nd + P[k][j].nd;
                if (pair_less(cand, P[i][j])) {
                    P[i][j] = cand;
                }
            }
        }
    }

    // Extract W and D
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= n; j++) {
            if (P[i][j].w == INF) {
                res->W[i][j] = INF;
                res->D[i][j] = 0;
            } else {
                res->W[i][j] = P[i][j].w;
                res->D[i][j] = c->delay[j] - P[i][j].nd;
            }
        }
    }
}

/* A cycle with zero registers is a combinational loop: no retiming can
 * fix it, and it would corrupt the D computation. Detect it up front. */
static int has_zero_register_cycle(const Circuit *c, const RetimingResult *res) {
    int n = c->num_nodes;
    for (int i = 1; i <= n; i++) {
        if (c->w[i][i] == 0) return 1;
        for (int j = i + 1; j <= n; j++) {
            if (res->W[i][j] == 0 && res->W[j][i] == 0) return 1;
        }
    }
    return 0;
}

int solve_constraints(const Circuit *c, const RetimingResult *res, int clock_period, int r_out[]) {
    int n = c->num_nodes;

    // Up to n^2 legality constraints plus up to n^2 clock constraints
    static Constraint constraints[2 * MAX_NODES * MAX_NODES];
    int num_constraints = 0;

    // Constraint Type 1: Legal Retiming r(u) - r(v) <= w(e)
    // Difference constraint r(u) - r(v) <= k becomes graph edge v -> u with weight k
    for (int u = 1; u <= n; u++) {
        for (int v = 1; v <= n; v++) {
            if (c->w[u][v] != INF) {
                constraints[num_constraints].u = v;
                constraints[num_constraints].v = u;
                constraints[num_constraints].weight = c->w[u][v];
                num_constraints++;
            }
        }
    }

    // Constraint Type 2: Clock Period Feasibility
    // D(u, v) > c  =>  r(u) - r(v) <= W(u, v) - 1
    for (int u = 1; u <= n; u++) {
        for (int v = 1; v <= n; v++) {
            if (res->W[u][v] != INF && res->D[u][v] > clock_period) {
                constraints[num_constraints].u = v;
                constraints[num_constraints].v = u;
                constraints[num_constraints].weight = res->W[u][v] - 1;
                num_constraints++;
            }
        }
    }

    // Bellman-Ford from a virtual source (all distances start at 0)
    int dist[MAX_NODES + 1];
    for (int i = 0; i <= n; i++) dist[i] = 0;

    for (int iter = 0; iter < n; iter++) {
        int changed = 0;
        for (int i = 0; i < num_constraints; i++) {
            int u = constraints[i].u;
            int v = constraints[i].v;
            int w = constraints[i].weight;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                changed = 1;
            }
        }
        if (!changed) break;
    }

    // Negative cycle detection
    for (int i = 0; i < num_constraints; i++) {
        int u = constraints[i].u;
        int v = constraints[i].v;
        int w = constraints[i].weight;
        if (dist[u] + w < dist[v]) {
            return 0; // Negative cycle -> infeasible
        }
    }

    for (int i = 1; i <= n; i++) {
        r_out[i] = dist[i];
    }
    return 1;
}

void run_retiming_algorithm(Circuit *c, RetimingResult *res) {
    compute_W_and_D(c, res);

    int r_temp[MAX_NODES + 1];
    res->is_feasible = 0;
    res->min_feasible_c = -1;

    if (has_zero_register_cycle(c, res)) {
        fprintf(stderr, "Error: circuit contains a cycle with zero registers "
                        "(combinational loop); no retiming exists.\n");
        return;
    }

    // Feasibility is monotone in c, so walk down until it first fails
    for (int target_c = c->max_clock_cycle; target_c >= 1; target_c--) {
        printf("  Testing feasibility for target clock period c = %d: ", target_c);
        if (solve_constraints(c, res, target_c, r_temp)) {
            printf("FEASIBLE\n");
            res->is_feasible = 1;
            res->min_feasible_c = target_c;
            for (int i = 1; i <= c->num_nodes; i++) {
                res->r[i] = r_temp[i];
            }
        } else {
            printf("INFEASIBLE\n");
            break;
        }
    }
}