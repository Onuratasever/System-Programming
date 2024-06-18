#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h> // Bu satırı ekleyin
#include <arpa/inet.h>
#include <signal.h>
#include <asm-generic/signal-defs.h>
#include <bits/sigaction.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <time.h>

#define BUFFER_SIZE 1024

int port;
int length, width;
typedef struct
{
    int id;
    int client_socket;
} Client;

pthread_t *client_threads;
Client *clients;
int client_count;
int is_ip = 0;
char *ip;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void signal_handler(int signum) // Signal handler for SIGINT
{
    printf("> ^C signal .. cancelling orders.. editing log..\n");

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].client_socket != -1)
        {
            close(clients[i].client_socket);
        }
    }

    free(client_threads);
    free(clients);
    exit(0);
}

// Client function to send order to server and receive response
void *client_function(void *arg)
{
    Client *client = (Client *)arg;
    struct sockaddr_in serv_addr;
    char order[50];
    char buffer[BUFFER_SIZE] = {0};
    char buffer2[BUFFER_SIZE] = {0};
    char buffer3[BUFFER_SIZE] = {0};
    char buffer4[BUFFER_SIZE] = {0};

    int sock = 0;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Client %d: Socket creation error\n", client->id);
        return NULL;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(sock, F_SETFL, flags);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    // serv_addr.sin_addr.s_addr = INADDR_ANY; // localhost (127.0.0.1)

    if (is_ip)
    {
        if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
        {
            perror("inet_pton");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Client %d: Connection Failed\n", client->id);
        return NULL;
    }
    srand(time(NULL));
    int x_coordinate = rand() % width;
    int y_coordinate = rand() % length;
    sprintf(order, "%d Order from PID: %d %d %d %d %d", client_count, getpid(), length, width, x_coordinate, y_coordinate);
    send(sock, order, strlen(order), 0);
    printf("> Customer %d: Order sent\n", client->id);

    int i = 0;
    while (i != 4) // It gets 4 response from server
    {
        int valread = read(sock, buffer, BUFFER_SIZE);
        // printf("valread: %d\n", valread);
        if (valread > 0)
        {
            printf("> Customer %d %s\n", client->id, buffer);
            memset(buffer, 0, BUFFER_SIZE);
            i++;
            // printf("i: %d\n", i);
        }
    }

    close(sock);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 5 && argc != 6)
    {
        fprintf(stderr, "Usage: %s [portnumber] [numberOfClients] [p] [q] or %s [ip] [portnumber] [numberOfClients] [p] [q]\n", argv[0], argv[0]);
        return 1;
    }

    if (argc == 6) // ip is given as argument
    {
        is_ip = 1;
        ip = (char *)malloc(strlen(argv[1]) * sizeof(char));
        strcpy(ip, argv[1]);
        port = atoi(argv[2]);
        client_count = atoi(argv[3]);
        length = atoi(argv[4]);
        width = atoi(argv[5]);
    }
    else if (argc == 5) // Only port number is given as argument
    {
        is_ip = 0;
        port = atoi(argv[1]);
        client_count = atoi(argv[2]);
        length = atoi(argv[3]);
        width = atoi(argv[4]);
    }

    client_threads = (pthread_t *)malloc(client_count * sizeof(pthread_t)); // Allocate memory for client threads
    clients = (Client *)malloc(client_count * sizeof(Client));

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    printf("> PID: %d\n", getpid());
    for (int i = 0; i < client_count; i++) // Create client threads
    {
        clients[i].id = i;
        pthread_create(&client_threads[i], NULL, client_function, &clients[i]);
    }

    for (int i = 0; i < client_count; i++) // Wait for client threads to finish
    {
        pthread_join(client_threads[i], NULL);
    }

    free(client_threads);
    free(clients);
    printf("> All orders are served\n");
    return 0;
}
