#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads = 1;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;
typedef struct proxy_thread_args {
  int source_fd;
  int dest_fd;
  pthread_cond_t* cond;
  int is_done;
}  proxy_thread_args;
/*
*
*/
void http_send_not_found(int fd){
  http_start_response(fd,404);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);
}

/*
* return file size if exists
*/
int file_exists(char* filename){
  struct stat buffer;
  int exist = stat(filename, &buffer);
  if(!exist)
    return buffer.st_size;
  else return -1;
}
/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 * You can change these functions to anything you want.
 * 
 * ATTENTION: Be careful to optimize your code. Judge is
 *            sesnsitive to time-out errors.
 */
void serve_file(int fd, char *path, int size) {
  char* buf = malloc(1024);
  snprintf(buf, 1024, "%d", size);
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path)); 
  http_send_header(fd, "Content-Length", buf);
  http_end_headers(fd);


  int file_fd = open(path, O_RDONLY);
  if(file_fd < 0) {
    return;
  }
  int read_n, read_buf_size = 1024;
  buf = malloc(read_buf_size);
  
  while((read_n = read(file_fd, buf, read_buf_size)) > 0){
    http_send_data(fd, buf, read_n);
  }
  free(buf);
  close(file_fd);
}

void serve_directory(int fd, char *path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  // content length?
  http_end_headers(fd);
  
  int buf_size = 1024;
  char*buf = malloc(buf_size);

  DIR* dir = opendir(path);
  struct dirent *entry;
  
  while ((entry = readdir(dir)) != NULL) {
    
    char* ref = malloc(1024);
    // strcpy(ref, path);
    // strcat(ref, "/");
    // strcat(ref, entry->d_name);
    strcpy(ref, entry->d_name);
    snprintf(buf, buf_size, "<a href=\"%s\">%s</a>\n", ref, entry->d_name);
    http_send_string(fd, buf);
    free(ref);
  }
  free(buf);
  closedir(dir);
}


/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 * 
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

  struct http_request *request = http_request_parse(fd);

  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }
  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  // construct path
  /* Remove beginning `./` */
  char *path = malloc(strlen(server_files_directory) + strlen(request->path) + 1);
  // path[0] = '.';
  // path[1] = '/';
  strcpy(path, server_files_directory);
  strcat(path, request->path);
  
  struct stat sfile;

  //stat system call
  stat(path, &sfile);
  if(S_ISREG(sfile.st_mode)){
      serve_file(fd, path, sfile.st_size);
  } else if (S_ISDIR(sfile.st_mode)){
    // create index.html path
    char* index_name = "/index.html";
    char* index_path = malloc(strlen(path) + strlen(index_name) + 1);
    strcpy(index_path, path);
    strcat(index_path, index_name);
    int size;
    if((size = file_exists(index_path)) != -1){
      serve_file(fd, index_path, size);
    } else {
      serve_directory(fd, path);
    }
  } else {
    http_send_not_found(fd);
  }
  free(path);
  close(fd);
  return;
}

void send_from_src_to_dest(int src, int dest){
  int size, buf_size = 1024;
  char* buf = malloc(buf_size);
  while((size = read(src, buf, buf_size)) > 0){
    http_send_data(dest, buf, size);  
  }
  free(buf);
}

void* run_proxy_thread(void* args){
  proxy_thread_args* args_ = (proxy_thread_args*) args;
  send_from_src_to_dest(args_->source_fd, args_->dest_fd);
  args_->is_done = 1;
  pthread_cond_signal(args_->cond);
  return NULL;
}
/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */

  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(target_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    close(target_fd);
    close(fd);
    return;

  }


  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


  proxy_thread_args* to_client_args = malloc(sizeof(proxy_thread_args));
  to_client_args->source_fd = target_fd;
  to_client_args->dest_fd = fd;
  to_client_args->cond = &cond;
  to_client_args->is_done = 0;

  proxy_thread_args* to_server_args = malloc(sizeof(proxy_thread_args));
  to_server_args->source_fd = fd;
  to_server_args->dest_fd = target_fd;
  to_server_args->cond = &cond;
  to_server_args->is_done = 0;


  pthread_t to_server, to_client;
  pthread_create(&to_server, NULL, run_proxy_thread, to_server_args);
  pthread_create(&to_client, NULL, run_proxy_thread, to_client_args);

  while(!(to_server_args->is_done) && !(to_client_args->is_done)){
    pthread_cond_wait(&cond, &mutex);
  }

  pthread_cancel(to_server);
  pthread_cancel(to_client);

  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&mutex);

  free(to_client_args);
  free(to_server_args);

  close(fd);
  close(target_fd);

}


void *run_thread(void* args){
  void (*request_handler)(int) = (void (*)(int)) args;
  while(1){
    int fd = wq_pop(&work_queue);
    request_handler(fd);
  }
}
void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  wq_init(&work_queue);
  pthread_t threads[num_threads];
  for(int i = 0; i < num_threads; i++){
    pthread_create(&threads[i], NULL, run_thread, request_handler);
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    if(num_threads == 0) {  
      request_handler(client_socket_number);
      close(client_socket_number);
    } else 
      wq_push(&work_queue, client_socket_number);
    

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
