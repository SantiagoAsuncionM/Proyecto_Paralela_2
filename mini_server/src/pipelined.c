//
// Created by Sleyter Angulo on 9/17/26.
// Actualizado por Fabricio Arias y Santiago Asuncion
//
#define _POSIX_C_SOURCE 200809L

#include "../includes/net_util.h"
#include "../includes/protocol.h"
#include "../includes/run_registry.h"
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
#define DEFAULT_PORT 8080
#define LISTEN_BACKLOG 64

static volatile sig_atomic_t g_running = 1;


static int g_listen_fd = -1;
static int g_closing = 0;
static pthread_mutex_t g_closing_mutex = PTHREAD_MUTEX_INITIALIZER;

static int active_handlers = 0;
static pthread_mutex_t handlers_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t handlers_zero;

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

    char* file_name = strdup(cmd->args[0]); //yamls/file.yaml
    if (file_name == NULL) {
        snprintf(response, response_size, "ERROR sin memoria\n");
        return;
    }

    int id = run_add(file_name);
    if(id<0){
        free(file_name);
        snprintf(response, response_size, "ERROR registro lleno\n");
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

void h_logs(cmd_t* cmd, char* response, size_t response_size){
  (void)cmd;
  snprintf(response, response_size, "ERROR no implementado\n");
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
            case CMD_LOGS: h_logs(&cmd, response, sizeof response); break;
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
    //int agents = atoi(argv[2]);
    //const char* work_directory = argv[3];
    
    // semaforo que lleva control de los hilos manejadores
    sem_init(&handlers_zero, 0, 0);

    g_listen_fd = nu_listen(port, LISTEN_BACKLOG);

    if (g_listen_fd < 0) {
        return EXIT_FAILURE;
    }

    unsigned long accepted = 0;


    printf("listening on port %u — Ctrl-C to stop\n", port);
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
    run_registry_destroy();
    sem_destroy(&handlers_zero);

    return EXIT_SUCCESS;
}