/* Main Function for Retiming Algorithm
   @author: Allen Harrison Paul
*/

#include "retiming.h"

void print_circuit_info(const Circuit *c) {
    printf("\n=== Circuit Specifications ===\n");
    printf("Total Nodes: %d\n", c->num_nodes);
    printf("Node Delays: ");
    for (int i = 1; i <= c->num_nodes; i++) {
        printf("d(v%d)=%d ", i, c->delay[i]);
    }
    printf("\nInitial Max Clock Cycle Constraint (c): %d\n", c->max_clock_cycle);
    printf("Edges and Initial Register Counts w(e):\n");
    for (int i = 0; i < c->edge_count; i++) {
        printf("  Edge (%d -> %d): %d register(s)\n", c->edges[i].u, c->edges[i].v, c->edges[i].weight);
    }
}

void print_matrices(const Circuit *c, const RetimingResult *res) {
    int n = c->num_nodes;
    printf("\n--- Matrix W (Minimum Register Count on Paths) ---\n     ");
    for (int j = 1; j <= n; j++) printf("[%d]  ", j);
    printf("\n");
    for (int i = 1; i <= n; i++) {
        printf("[%d] ", i);
        for (int j = 1; j <= n; j++) {
            if (res->W[i][j] == INF) printf(" INF ");
            else printf("%4d ", res->W[i][j]);
        }
        printf("\n");
    }

    printf("\n--- Matrix D (Maximum Delay on Critical Paths) ---\n     ");
    for (int j = 1; j <= n; j++) printf("[%d]  ", j);
    printf("\n");
    for (int i = 1; i <= n; i++) {
        printf("[%d] ", i);
        for (int j = 1; j <= n; j++) {
            printf("%4d ", res->D[i][j]);
        }
        printf("\n");
    }
}

void print_retiming_results(const Circuit *c, const RetimingResult *res) {
    if (!res->is_feasible) {
        printf("\nResult: No feasible retiming configuration found for c <= %d\n", c->max_clock_cycle);
        return;
    }

    printf("\n=======================================================\n");
    printf("           RETIMING RESULTS (OPTIMAL CLOCK)\n");
    printf("=======================================================\n");
    printf(" Minimum Feasible Clock Period (c_min): %d\n\n", res->min_feasible_c);
    printf(" Retiming Vector r(v):\n");
    for (int i = 1; i <= c->num_nodes; i++) {
        printf("   Node %d: r = %2d\n", i, res->r[i]);
    }

    printf("\n Retimed Edge Register Weights w_r(e):\n");
    for (int i = 0; i < c->edge_count; i++) {
        int u = c->edges[i].u;
        int v = c->edges[i].v;
        int w_orig = c->edges[i].weight;
        int w_retimed = w_orig + res->r[v] - res->r[u];
        printf("   Edge (%d -> %d): Original w = %d  -->  Retimed w_r = %d\n", 
               u, v, w_orig, w_retimed);
    }
    printf("=======================================================\n\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <circuit_file.txt>\n", argv[0]);
        return 1;
    }

    Circuit c;
    RetimingResult res;

    printf("Loading input circuit file: %s...\n", argv[1]);
    if (!parse_circuit_file(argv[1], &c)) {
        fprintf(stderr, "Failed to parse input circuit file.\n");
        return 1;
    }

    print_circuit_info(&c);

    printf("\nExecuting Leiserson-Saxe Retiming Step-by-Step...\n");
    run_retiming_algorithm(&c, &res);

    print_matrices(&c, &res);
    print_retiming_results(&c, &res);

    return 0;
}