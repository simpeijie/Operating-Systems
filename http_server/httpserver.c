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
#include <unistd.h>
#include <poll.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

char *get_content(char *path, size_t size) {
	FILE *fp = fopen(path, "rb");

	if (fp == NULL) {
		//http_start_response()
	}
	char *content;
	if (size == -1) {
		fseek(fp, 0, SEEK_END);
		long fsize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		content = malloc(fsize + 1);
		if (content) 
			fread(content, 1, fsize, fp);
		content[fsize] = 0;
	}
	else {
		content = malloc(size + 1);
		if (content)
			fread(content, 1, size, fp);
		content[size] = 0;
	}
	fclose(fp);

	return content;
}

void respond(int fd, int status_code, char *path, size_t size){
	char *content;
	char *type;
	char str[1024];
	type = http_get_mime_type(path);

	http_start_response(fd, status_code);
	http_send_header(fd, "Content-Type", type);	
	if (strcmp(type, "text/html") == 0) {
		content = get_content(path, -1);
		sprintf(str, "%zu", strlen(content));
		http_send_header(fd, "Content-Length", str);
		http_end_headers(fd);
		http_send_string(fd, content);
	}
	else {
		content = get_content(path, size);
		sprintf(str, "%zu", size);
		http_send_header(fd, "Content-Length", str);
		http_end_headers(fd);
		http_send_data(fd, content, size);
	}
}

/* pop client off work-queue and start handling request */
void thread_handler(void (*request_handler)(int)) {
	while (1) {
		int client_socket_fd = wq_pop(&work_queue);	
		request_handler(client_socket_fd);
		close(client_socket_fd);
	}
}

char *get_ip_from_hostname(char *ip) {
	struct hostent *he;
	struct in_addr **addr_list;

	if ((he = gethostbyname(server_proxy_hostname)) == NULL) {
		perror("gethostbyname");
		exit(1);
	}

	addr_list = (struct in_addr **) he->h_addr_list;
	for (int i = 0; addr_list[i] != NULL; i++) {
		strcpy(ip, inet_ntoa(*addr_list[i])); 
		return ip;
	}
	return NULL;
}
/*
void read_from_server(int server_fd, int client_fd) {
	pthread_mutex_lock(&mu);
	ssize_t size = 0;
	char buf[1024];
	while (1) {
		while ((size = read(server_fd, buf, sizeof(buf) - 1)) < 0) {
			pthread_cond_signal(&client_cv);
			pthread_cond_wait(&server_cv, &mu);
		}
		if (write(client_fd, buf, size) < 0)  
			break;
	}	
	pthread_mutex_unlock(&mu);
}

void read_from_client(int client_fd, int server_fd) {
	pthread_mutex_lock(&mu);
	ssize_t size = 0;
	char buf[1024];
	while (1) {
		while ((size = read(client_fd, buf, sizeof(buf) - 1)) < 0) {
			pthread_cond_signal(&server_cv);
			pthread_cond_wait(&client_cv, &mu);
		}
		if (write(server_fd, buf, size) < 0)
			break;
	}
	pthread_mutex_unlock(&mu);
}
*/
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
 */
void handle_files_request(int fd) {

  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */

  struct http_request *request = http_request_parse(fd);
	
	char current_path[1024];
	strcpy(current_path, server_files_directory);
	strcat(current_path, request->path);
	
	struct stat sb;
	stat(current_path, &sb);
	if (S_ISREG(sb.st_mode)) {
		respond(fd, 200, current_path, sb.st_size);
	}
	else if (S_ISDIR(sb.st_mode)) {
		// append / to the end of path if one isn't found
		if (current_path[strlen(current_path) - 1] != '/') {
			strcat(current_path, "/");
		}
	
		printf("%s\n", current_path);
		char temp[1024];
		strcpy(temp, current_path);
		strcat(temp, "/index.html");
		DIR *dir;
		struct dirent *ent;
		
		// if index.html does not exist in current dir
		if (access(temp, F_OK) == -1) {
			if ((dir = opendir(current_path)) != NULL) {
				char link[1024];
				while ((ent = readdir(dir)) != NULL) {
					strcat(link, "<a href=\"");
					strcat(link, ent->d_name);
					strcat(link, "\">");
					strcat(link, ent->d_name);
					strcat(link, "</a>");
				}
				http_start_response(fd, 200);
				http_send_header(fd, "Content-Type", "text/html");
				http_end_headers(fd);
				http_send_string(fd, link);	
				link[0] = 0;
			}
		}
		else {
			strcat(current_path, "/index.html");
  		respond(fd, 200, current_path, sb.st_size);	
		}
	}
	else {	
		http_start_response(fd, 404);
	}
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
   * TODO: Your solution for Task 3 goes here! Feel free to delete/modify *
   * any existing code.
   */
	char ip[100];
	if (get_ip_from_hostname(ip) == NULL) {
		perror("Error getting ip address");
		exit(errno);
	}

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(ip);
	server_address.sin_port = htons(server_proxy_port);


	int socket_number = socket(PF_INET, SOCK_STREAM, 0);	
	if (socket_number == -1) {
		perror("Failed to create a new socket");
		exit(errno);
	}

  int socket_option = 1;
  if (setsockopt(socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

	if (connect(socket_number, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
		perror("Failed to connect to server");
		exit(errno);
	}

//	int status = fcntl(socket_number, F_SETFL, fcntl(socket_number, F_GETFL, 0) | O_NONBLOCK);
	/*fd_set *readfds;
	fd_set *writefds;
	FD_SET(socket_number, readfds);
	FD_SET(fd, readfds);
	FD_SET(socket_number, writefds);
	FD_SET(fd, writefds);

	if (select(
	*/
	struct pollfd fds[2];
	fds[0].fd = socket_number;
	fds[1].fd = fd;
	fds[0].events = POLLIN;
	fds[1].events = POLLIN;

	int ret;
	char buf[1024];
	ssize_t size = 0;
	while (1) {
		ret = poll(fds, 2, 0);
		if (ret == -1) {
			perror("Error when polling");
			exit(1);
		}
		if (fds[0].revents & POLLIN) {
			size = read(fds[0].fd, buf, sizeof(buf) - 1);		
			write(fds[1].fd, buf, size);
		}
		if (fds[1].revents & POLLIN) {
			size = read(fds[1].fd, buf, sizeof(buf) - 1);		
			write(fds[0].fd, buf, size);
		}
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

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }
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

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;
	num_threads = 1;
	wq_init(&work_queue);

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

	pthread_t t;
	for (int i = 0; i < num_threads; i++) {
		pthread_create(&t, NULL, (void *) &thread_handler, request_handler);
	}

  serve_forever(&server_fd, request_handler);

	return EXIT_SUCCESS;
}
