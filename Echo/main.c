#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


typedef struct {
    char client_ip[INET6_ADDRSTRLEN];
    int client_fd;
} handle_client_arg_t;

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int select_info(struct addrinfo *res) {
    struct addrinfo *p;
    int listen_fd;
    int yes;

    yes = 1;
    listen_fd = -1;
    for(p = res; p != NULL; p = p->ai_next) {
        // Get a socket corresponding to the required address information
        if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        // Set socket option so the port can be reused (?)
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            continue;
        }
        // Bind the socket to a port
        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(listen_fd);
            perror("server: bind");
            continue;
        }
        break;
    }

    return listen_fd;
}

int setup_listen(char* port) {
    struct addrinfo hints, *res;
    int listen_fd;
    int status;

    // Set the hints struct to 0, and then set the desired option that we want `getaddrinfo` to look for
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // IPv4 of IPv6
    hints.ai_socktype = SOCK_STREAM; // Stream sockets, we want a connection socket
    hints.ai_flags = AI_PASSIVE;     // Actually I'm not sure what that flag do, but i need it to get local address based on the documentation
    // Get the local (server) address information
    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }
    // Get a socket descriptor
    listen_fd = select_info(res);
    freeaddrinfo(res);
    if(listen_fd == -1) {
        fprintf(stderr, "setup_listen: no valid server info\n");
        exit(1);
    }

    return listen_fd;
}

void *handle_client(void *arg) {
    handle_client_arg_t* args;
    int nb_bytes;
    char buffer[256];

    args = (handle_client_arg_t*)arg;
    while((nb_bytes = recv(args->client_fd, buffer, 255, 0)) > 0) {
        buffer[nb_bytes] = '\0';
        printf("[%s]: Recieved: %s\n", args->client_ip, buffer);
    }
    if(nb_bytes == 0) {
        printf("[%s] Connection closed\n", args->client_ip);
    }
    if(nb_bytes == -1) {
        perror("recv");
        exit(1);
    }
    free(arg);
    close(args->client_fd);
    pthread_exit(EXIT_SUCCESS);
}

int check_number(char* arg) {
    for(size_t i = 0; i < strlen(arg); i++) {
        if(arg[i] < '0' || arg[i] > '9') {
            return 0;
        }
    }

    return 1;
}

void check_arg(int argc, char* argv[]) {
    if(argc < 2) {
        fprintf(stderr, "ERROR: Please specify a port number.\n");
        exit(1);
    }
    if(!check_number(argv[1])) {
        fprintf(stderr, "ERROR: The port must be a number.\n");
        exit(1);
    }
    if(atoi(argv[1]) < 1024) {
        fprintf(stderr, "ERROR: Port 0 to 1023 are reserved, please select another port.\n");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    check_arg(argc, argv);

    int listen_fd, connect_fd;
    socklen_t connect_fd_len;
    struct sockaddr_storage in_addr;
    socklen_t addr_size;
    char client_ip[INET6_ADDRSTRLEN];
    char buffer[256];
    int nb_bytes;

    listen_fd = setup_listen(argv[1]);

    if(listen(listen_fd, 10) == -1) {
        perror("listen");
        exit(1);
    }
    printf("[MAIN] Listening from port: %s\n", argv[1]);

    while(1) {
        connect_fd_len = sizeof in_addr;
        connect_fd = accept(listen_fd, (struct sockaddr *)&in_addr, &connect_fd_len);
        if(connect_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(in_addr.ss_family,
                  get_in_addr((struct sockaddr *)&in_addr),
                  client_ip,
                  sizeof client_ip);
        printf("[MAIN] Revieved connection from %s\n", client_ip);

        // Handle client
        handle_client_arg_t* args = malloc(sizeof(handle_client_arg_t));
        if(!args) {
            perror("malloc");
            exit(1);
        }
        args->client_fd = connect_fd;
        snprintf(args->client_ip, sizeof(args->client_ip), "%s", client_ip);
        pthread_t thread;
        if(pthread_create(&thread, NULL, handle_client, args) != 0) {
            perror("pthread_create");
            close(connect_fd);
            free(args);
        }
        else {
            pthread_detach(thread);
        }
    }
    close(listen_fd);
}
