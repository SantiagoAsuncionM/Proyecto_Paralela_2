//
// Created by Sleyter Angulo on 9/17/26.
// Actualizado por Fabricio Arias y Santiago Asuncion
//
#define _POSIX_C_SOURCE 200809L

#include "net_util.h"
#include "protocol.h"
#include "run_registry.h"
#include "parser.h"
#include "validate_pipeline.h"
#include "graph.h"
#include "graph.h"
#include "scheduler.h"
#include "executor.h"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#define DEFAULT_PORT 8080
#define LISTEN_BACKLOG 64

static volatile sig_atomic_t g_running = 1;


static int g_listen_fd = -1;
static int g_closing = 0;
static pthread_mutex_t g_closing_mutex = PTHREAD_MUTEX_INITIALIZER;

static int active_handlers = 0;
static pthread_mutex_t handlers_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t handlers_zero;
static scheduler_t scheduler;
static const char* work_directory = NULL;

static int is_closing(void) {
    pthread_mutex_lock(&g_closing_mutex);
    int closing = g_closing;
    pthread_mutex_unlock(&g_closing_mutex);
    return closing;
}

static void request_shutdown(void) {
    pthread_mutex_lock(&g_closing_mutex);
    g_closing = 1;
    pthread_mutex_unlock(&g_closing_mutex);

    if (g_listen_fd >= 0)
        shutdown(g_listen_fd, SHUT_RDWR);
}

static void handler_enter(void) {
    pthread_mutex_lock(&handlers_mutex);
    active_handlers++;
    pthread_mutex_unlock(&handlers_mutex);
}

static void handler_exit(void) {
    pthread_mutex_lock(&handlers_mutex);
    active_handlers -= 1;
    if (active_handlers == 0 && is_closing()) {
        sem_post(&handlers_zero);
    }
    pthread_mutex_unlock(&handlers_mutex);
}

static void wait_handlers(void) {
    for (;;) {
        pthread_mutex_lock(&handlers_mutex);
        int handlers_count = active_handlers;
        pthread_mutex_unlock(&handlers_mutex);
        if (handlers_count == 0)
            return;
        while (sem_wait(&handlers_zero) != 0 && errno == EINTR) {

        }
    }
}

static void on_sigint(int signum)
{
    (void)signum;
    g_running = 0;
}

static int install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;

    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;

    if (sigaction(SIGPIPE, &sa, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

void show_errors(array_t* errors, char* response, size_t response_size){
  
    size_t used = (size_t)snprintf(response, response_size, "ERROR %zu\n" , errors->num_items);

    for(size_t i =0; i<errors->num_items && used<response_size; i++){

      int n = snprintf(response + used, response_size-used, "%s\n", (char*)errors->items[i]);

      if(n<0){
        break;
      }

      used += (size_t)n;

    }



}





void h_submit(cmd_t* cmd, char* response, size_t response_size) {

    if (is_closing()) { 
        snprintf(response, response_size, "ERROR servidor cerrando\n");
        return;
    }

 
    //si no se encuentra el archivo
    if(access(cmd->args[0], R_OK) != 0){

        snprintf(response, response_size, "ERROR no existe el archivo '%s'\n", cmd->args[0]);
        return;
    }  

    //leero el archivo y crear la lista de errores
    array_t* errors = NULL;
    pipeline_t* pipeline = load_pipeline(cmd->args[0], &errors);

    if(pipeline != NULL){
        validate_names_and_refs(pipeline, errors);
    }

    //crear el grafo
    dep_graph_t* graph = NULL;

    if(pipeline != NULL && errors->num_items == 0){

        graph = graph_build(pipeline);
        if(graph != NULL){
            graph_check_cycles(graph, errors);
        }


    }

    //si algo fallo
    if(pipeline == NULL || graph == NULL || errors->num_items > 0){
        
        if(errors->num_items > 0){
            show_errors(errors, response, response_size);
        }
        else{

          snprintf(response, response_size, "ERROR al procesar el archivo\n");
          graph_destroy(graph);
          array_destroy(errors);  
          return;
        }
    }

    //crear la run 
    run_t* run = run_create(pipeline, graph);
    if(run == NULL){
        graph_destroy(graph);
        snprintf(response, response_size, "ERROR, sin memoira\n");
        return;
    }

    int id = run_add(run);
    if(id < 0){
       run_destroy(run);
       snprintf(response, response_size, "ERROR, registro sin espacio\n");
       return;
    }

    char run_directory[1024];
    snprintf(run_directory, sizeof run_directory, "%s/run_%d", work_directory, id);
    if (!executor_submit_run(&scheduler, run, run_directory)) {
        snprintf(response, response_size, "ERROR al iniciar la ejecucion %d\n", id);
        return;
    }

    snprintf(response, response_size, "OK %d\n", id);
}

void h_status(cmd_t* cmd, char* response, size_t response_size) {
    
    int id = (int)strtol(cmd->args[0], NULL, 10);

    if(run_status(id, response, response_size) < 0){
    snprintf(response, response_size, "ERROR id desconocido\n");

    }

    if (is_closing()) {
        snprintf(response, response_size, "ERROR servidor cerrando\n");
        return;
    }
    
}


void h_shutdown(char* response, size_t response_size) {
    request_shutdown();
    snprintf(response, response_size, "BYE\n");
}

// Sin FOLLOW manda la bitacora que hay en ese momento
// Con FOLLOW se queda mandando las lineas nuevas hasta que termine
void h_logs(int fd, cmd_t* cmd, char* response, size_t response_size){
  
    int id = (int)strtol(cmd->args[0], NULL, 10);

    const char* job = NULL;
    bool FOLLOW = false;

    for(int i=1; i<cmd->argc; i++){
        if(strcmp(cmd->args[i], "FOLLOW") == 0){
            FOLLOW = true;
        }
        else{
            job = cmd->args[i];
        }
    }


    //si en ningun lado del input viene follow
    if(FOLLOW == false){
     int result = run_logs(id, job, response, response_size);
     if (result == -1) {
         snprintf(response, response_size, "ERROR id desconocido\n");
     }  else if (result == -2) {
         snprintf(response, response_size, "ERROR el trabajo '%s' no existe\n", job);
     }
     return;
    }

    //caso con follow: 
    int count = run_job_count(id);

    if(count <0){

        snprintf(response, response_size, "ERROR, id desconocido\n");
        return;
    }

    size_t* positions = calloc((size_t)count, sizeof(size_t));
    if (positions == NULL) {
        snprintf(response, response_size, "ERROR, sin memoria\n");
        return;
    }
    
    char new_lines[8192];

    while(1){
        bool finished = false;
        int result = run_logs_follow(id, job, positions, new_lines, sizeof new_lines, &finished);

        if(result == -2){
            snprintf(response, response_size, "ERROR el trabajo '%s' no existe\n", job);
            break;
        }

         if(result == -1){
            snprintf(response, response_size, "ERROR el id no existe\n");
            break;
        }

        if (new_lines[0] != '\0') {
            if (nu_write_all(fd, new_lines, strlen(new_lines)) < 0) {
                break;
            }
        }

        //termina de imprimir si todos los trabajos terminaron o si el servidor se apaga
        if (finished==true || is_closing()) {
            break;
        }


    }

    free(positions);

}

static void h_cancel(cmd_t* cmd, char* response, size_t response_size) {

    int id = (int)strtol(cmd->args[0], NULL, 10);
    if (run_cancel(id) < 0)
        snprintf(response, response_size, "ERROR id desconocido\n");
    else
        snprintf(response, response_size, "OK %d canceled\n", id);
}

static void h_list(cmd_t* cmd, char* response, size_t response_size) {
    (void)cmd;
    run_list(response, response_size);
}


static void *handle_connection(void *arg) {
    connection_t *conn = arg;
    char linea[512];
    char response[1024] = {0};

    if (nu_read_line(conn->file_descriptor, linea, sizeof linea) < 0){
        snprintf(response, sizeof response, "ERROR linea invalida\n");
    }else {
        cmd_t cmd;
        parse_command(linea, &cmd);
        switch (cmd.tipo) {
            case CMD_SUBMIT: h_submit(&cmd, response, sizeof response); break;
            case CMD_STATUS: h_status(&cmd, response, sizeof response); break;
            case CMD_SHUTDOWN: h_shutdown(response, sizeof response); break;
            case CMD_LOGS: h_logs(conn->file_descriptor, &cmd, response, sizeof response); break;
            case CMD_CANCEL: h_cancel(&cmd, response, sizeof response); break;
            case CMD_LIST: h_list(&cmd, response, sizeof response); break;
            default:
                snprintf(response,  sizeof response, "ERROR %s\n", cmd.error);
        }
    }

    nu_write_all(conn->file_descriptor, response, strlen(response));

    close(conn->file_descriptor);
    free(conn);
    handler_exit();
    return NULL;

}

//nueva funcion para validar los nuevos parametros
static int parse_parameters(int argc, char** argv){

   if (argc != 4) {
        fprintf(stderr, "uso: %s <port> <agents> <work_directory>\n", argv[0]);
        return -1;
    }  

  if(atoi(argv[2]) <= 0 || access(argv[3], F_OK) != 0 ){

     if (access(argv[3], F_OK) != 0) {
       fprintf(stderr, "Invalid work_directory\n");
     }

      if (atoi(argv[2]) <= 0){
      fprintf(stderr, "Invalid number of agents\n");
     }

    return -1; 
  }
  
  
  return 0;

}



static unsigned short parse_port(int argc, char **argv)
{
    if (argc < 2)
    {
        return DEFAULT_PORT;
    }

    char *end = NULL;
    errno = 0;
    long value = strtol(argv[1], &end, 10);

    if (errno != 0 || end == argv[1] || *end != '\0' ||
        value <= 0 || value > 65535) {
        fprintf(stderr, "invalid port '%s', using %d\n", argv[1], DEFAULT_PORT);
        return DEFAULT_PORT;
        }

    return (unsigned short)value;
}

int main(int argc, char** argv) {
    if (install_signal_handlers() < 0) {
        return EXIT_FAILURE;
    }

    if (parse_parameters(argc, argv) < 0) {
    return EXIT_FAILURE;
    }

    unsigned short port = parse_port(argc, argv);
    int agents = atoi(argv[2]);
    work_directory = argv[3];
    
    // semaforo que lleva control de los hilos manejadores
    sem_init(&handlers_zero, 0, 0);

    g_listen_fd = nu_listen(port, LISTEN_BACKLOG);

    if (g_listen_fd < 0) {
        return EXIT_FAILURE;
    }

    unsigned long accepted = 0;

    //inicializar el scheduler
    if(!scheduler_start(&scheduler, agents)){
        fprintf(stderr, "ERROR al iniciar el scheduler\n");
        return EXIT_FAILURE;
    }


    printf("listening on port %u, agents=%d, work directory=%s — Ctrl-C to stop\n", port, agents, work_directory);
    fflush(stdout);
    while (g_running && !is_closing()) {

        int client_file_descriptor = accept(g_listen_fd, NULL, NULL);
        if (client_file_descriptor < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        connection_t *conn = malloc(sizeof(connection_t));
        
        if (conn == NULL)
        {
            fprintf(stderr, "out of memory, dropping connection\n");
            close(client_file_descriptor);
            continue;
        }
        
        conn->file_descriptor = client_file_descriptor;
        conn->connection_id = ++accepted;
        handler_enter();
        
        pthread_t thread_id;
        
        int pthread_created = pthread_create(&thread_id, NULL, handle_connection, conn);

        if (pthread_created != 0) {
            fprintf(stderr, "pthread_create failed %s\n", strerror(pthread_created));
            close(client_file_descriptor);
            free(conn);
            --accepted;
            handler_exit();
            continue;
        }
        pthread_created = pthread_detach(thread_id);
        
        if (pthread_created != 0) {
            fprintf(stderr, "pthread_detach failed %s\n", strerror(pthread_created));
        }
    }
    request_shutdown();
    close(g_listen_fd);
    g_listen_fd = -1;


    wait_handlers();
    scheduler_shutdown(&scheduler);
    run_registry_destroy();
    sem_destroy(&handlers_zero);

    return EXIT_SUCCESS;
}