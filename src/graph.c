#include <string.h>

#include "graph.h"
#include "validate_pipeline.h"

bool add_edge(job_node_t* graph, int from, int to) {
    for (int i = 0; i < graph[to].n_pred; i++) {
        if (graph[to].pred[i] == from) return true;
    }
        
    int* new_pred = realloc(graph[to].pred, (size_t)(graph[to].n_pred + 1) * sizeof(int));
    graph[to].pred = new_pred;
    
    int* new_succ = realloc(graph[from].succ, (size_t)(graph[from].n_succ + 1) * sizeof(int));
    graph[from].succ = new_succ;

    graph[to].pred[graph[to].n_pred++] = from;
    graph[from].succ[graph[from].n_succ++] = to;
    return true;
}

int stage_index(const pipeline_t* pipe, const char* name) {
    for (size_t i = 0; i < pipe->stages->num_items; i++) {
        if (strcmp(((stage_t*)pipe->stages->items[i])->name, name) == 0) {
            return (int)i;
        } 
    }
    return -1;
}

int job_index(const stage_t* s, const char* name) {
    for (size_t k = 0; k < s->jobs->num_items; k++) {
        if (strcmp(((job_t*)s->jobs->items[k])->name, name) == 0) {
            return (int)k;
        } 
    }
    return -1;
}

// Todo trabajo de la etapa 'from' precede a todo trabajo de la etapa 'to'
void link_stages(job_node_t* graph, const int* base, int from, int to) {
    for (int a = base[from]; a < base[from + 1]; a++) {
        for (int b = base[to]; b < base[to + 1]; b++) {
            add_edge(graph, a, b);
        }
    }
}

dep_graph_t* graph_build(const pipeline_t* pipe) {
    int ns = pipe->stages->num_items;

    dep_graph_t* graph = calloc(1, sizeof *graph);
    if (!graph) return NULL;

    graph->stage_base = malloc((size_t)(ns + 1) * sizeof(int));
    if (!graph->stage_base) { 
        free(graph);
        return NULL;
    }
    graph->n_stages = (size_t)ns;

    graph->stage_base[0] = 0;
    for (int i = 0; i < ns; i++) {
        graph->stage_base[i + 1] = graph->stage_base[i] + ((stage_t*)pipe->stages->items[i])->jobs->num_items;
    }

    graph->n_nodes = (size_t)graph->stage_base[ns];
    graph->nodes = calloc(graph->n_nodes ? graph->n_nodes : 1, sizeof(job_node_t));
    if (!graph->nodes) { 
        graph_destroy(graph);
        return NULL;
    }

    const int* base = graph->stage_base;
    job_node_t* nodes = graph->nodes;

    for (int i = 0; i < ns; i++) {
        stage_t* s = pipe->stages->items[i];
        for (size_t k = 0; k < s->jobs->num_items; k++) {
            nodes[base[i] + (int)k].def   = s->jobs->items[k];
            nodes[base[i] + (int)k].stage = i;
        }
    }

    for (int i = 0; i < ns; i++) {
        stage_t* s = pipe->stages->items[i];

        if (!s->has_depends_on) {
            if (i > 0) link_stages(nodes, base, i - 1, i);
        } else {
            for (size_t d = 0; d < s->depends_on->num_items; d++) {
                int from = stage_index(pipe, s->depends_on->items[d]);
                if (from >= 0) link_stages(nodes, base, from, i);
            }
        }

        for (size_t k = 0; k < s->jobs->num_items; k++) {
            job_t* j = s->jobs->items[k];
            for (size_t d = 0; d < j->depends_on->num_items; d++) {
                int from = job_index(s, j->depends_on->items[d]);
                if (from >= 0) add_edge(nodes, base[i] + from, base[i] + (int)k);
            }
        }
    }

    return graph;
}

void graph_destroy(dep_graph_t* graph) {
    if (!graph) return;
    for (size_t i = 0; i < graph->n_nodes; i++) {
        free(graph->nodes[i].pred);
        free(graph->nodes[i].succ);
    }
    free(graph->nodes);
    free(graph->topo);
    free(graph->stage_base);
    free(graph);
}

int find_cycles(const job_node_t* graph, int n, int* deg, int* order) {
    // aplica el algoritmo de khan para encontrar el orden topologico de los nodos
    // si el largo del resultado es diferente a la cantidad de vertices hay ciclos
    int head = 0, tail = 0;
    for (int i = 0; i < n; i++) {
        deg[i] = graph[i].n_pred;
        if (deg[i] == 0) order[tail++] = i;
    }
    while (head < tail) {
        int u = order[head++];
        for (int k = 0; k < graph[u].n_succ; k++) {
            if (--deg[graph[u].succ[k]] == 0) {
                order[tail++] = graph[u].succ[k];
            } 
        }
    }
    return tail;
}

bool graph_check_cycles(dep_graph_t* graph, array_t* errors) {
    size_t n = graph->n_nodes;
    job_node_t* nodes = graph->nodes;
    int* deg = malloc(n * sizeof(int));
    int* order = malloc(n * sizeof(int));
    if (!deg || !order) { 
        free(deg);
        free(order); return true;
    }

    if (find_cycles(graph->nodes, (int)n, deg, order) == (int)n) {
        free(deg);
        graph->topo = order;
        return false;
    }
    
    int start = 0;
    while (deg[start] == 0) start++;

    int* pos = calloc((size_t)n, sizeof(int));
    int* path = malloc((size_t)n * sizeof(int));
    int len = 0, cur = start;
    while (pos[cur] == 0) {
        pos[cur] = len + 1;
        path[len++] = cur;
        for (int k = 0; k < nodes[cur].n_pred; k++)
            if (deg[nodes[cur].pred[k]] > 0) { 
                cur = nodes[cur].pred[k];
                break;
            }
    }

    char msg[256] = "dependencia cíclica: ";
    for (int i = pos[cur] - 1; i < len; i++) {
        strncat(msg, nodes[path[i]].def->name, sizeof msg - strlen(msg) - 1);
        strncat(msg, " -> ", sizeof msg - strlen(msg) - 1);
    }
    strncat(msg, nodes[cur].def->name, sizeof msg - strlen(msg) - 1);
    add_error(errors, nodes[cur].def->line, "%s", msg);

    free(deg);
    free(order);
    return true;
}
