/* Retiming Algorithm Header File
   @author: Allen Harrison Paul
*/

#ifndef RETIMING_H
#define RETIMING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES 100
#define INF 1000000

typedef struct {
    int u;
    int v;
    int weight;
} Edge;

typedef struct {
    int u;
    int v;
    int weight;
} Constraint;

typedef struct {
    int num_nodes;
    int delay[MAX_NODES + 1];
    int w[MAX_NODES + 1][MAX_NODES + 1];
    int edge_count;
    Edge edges[MAX_NODES * MAX_NODES];
    int max_clock_cycle;
} Circuit;

typedef struct {
    int W[MAX_NODES + 1][MAX_NODES + 1];
    int D[MAX_NODES + 1][MAX_NODES + 1];
    int r[MAX_NODES + 1];
    int min_feasible_c;
    int is_feasible;
} RetimingResult;

void init_circuit(Circuit *c, int num_nodes);
int add_edge(Circuit *c, int u, int v, int registers);
void compute_W_and_D(const Circuit *c, RetimingResult *res);
int solve_constraints(const Circuit *c, const RetimingResult *res, int clock_period, int r_out[]);
void run_retiming_algorithm(Circuit *c, RetimingResult *res);
int parse_circuit_file(const char *filename, Circuit *c);

void print_circuit_info(const Circuit *c);
void print_matrices(const Circuit *c, const RetimingResult *res);
void print_retiming_results(const Circuit *c, const RetimingResult *res);

#endif