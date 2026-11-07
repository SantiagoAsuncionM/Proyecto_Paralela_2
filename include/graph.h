#pragma once
#include "pipeline.h"

typedef struct job_node {
    const job_t* def;          //YAML definition
    int stage;        //Stage index

    int* pred;         //Predecessors: Indexes of jobs this job depends on
    int n_pred;       //predecessor jobs count
    int* succ;         //Succesors: Indexes of jobs that depends on this job
    int n_succ;       //Succesor jobs count  

    int pending;      //Predecessors pending count
    // exec_state_t    state;        //Job current state  
    struct timespec t_start, t_end;

    // step_exec_t    *steps;        //One per step
    // job_log_t       log;          //Job log
    array_t* out_vars;     //Variables set when job ended
    pid_t current_pid;  //Child process ID running, 0 if none
} job_node_t;

typedef struct {
    job_node_t* nodes;
    size_t n_nodes;
    int* topo; // es el orden topologico de las dependencias
    int* stage_base;
    size_t n_stages;
} dep_graph_t;


bool add_edge(job_node_t* g, int from, int to);
int stage_index(const pipeline_t* p, const char* name);
int job_index(const stage_t* s, const char* name);
void link_stages(job_node_t* g, const int* base, int from, int to);
dep_graph_t* graph_build(const pipeline_t* p);
void graph_destroy(dep_graph_t* g);
int find_cycles(const job_node_t* g, int n, int* deg, int* order);
bool graph_check_cycles(dep_graph_t* g, array_t* errors);
