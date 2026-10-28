/* Circuit Parser Implementation
   @author: Allen Harrison Paul
*/

#include "retiming.h"

int parse_circuit_file(const char *filename, Circuit *c) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return 0;
    }

    char line[512];
    int num_nodes = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *comment = strstr(line, "//");
        if (comment) *comment = '\0';

        if (strstr(line, "TotalNodes")) {
            sscanf(line, " TotalNodes = %d", &num_nodes);
            init_circuit(c, num_nodes);
        } else if (strstr(line, "NodeDelays")) {
            char *p = strchr(line, '=');
            if (p) {
                p++;
                for (int i = 1; i <= num_nodes; i++) {
                    c->delay[i] = (int)strtol(p, &p, 10);
                    if (*p == ',') p++;
                }
            }
        } else if (strstr(line, "Edge")) {
    int u, v, w;
    if ((sscanf(line, " Edge%d_%d = %d", &u, &v, &w) == 3 ||
         sscanf(line, " Edge%1d%1d = %d", &u, &v, &w) == 3) &&
        u >= 1 && u <= num_nodes && v >= 1 && v <= num_nodes) {
        if (!add_edge(c, u, v, w)) { fclose(fp); return 0; }
    }
        } else if (strstr(line, "MaxClockCycle")) {
            sscanf(line, " MaxClockCycle = %d", &c->max_clock_cycle);
        }
    }

    fclose(fp);
    return (num_nodes > 0);
}