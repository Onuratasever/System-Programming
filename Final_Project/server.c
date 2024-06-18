#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>
#include <bits/sigaction.h>
#include <asm-generic/signal-defs.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <math.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PIDE_IN_OVEN 6
#define OVEN_ENTRANCES 2
#define MAX_LOG_LENGTH 512
int pide_x_coordinate, pide_y_coordinate;
int max_delivery_capacity;
int port;
int is_ip = 0;
typedef struct Order
{
    int client_socket;
    int client_pid;
    int x_coordinate, y_coordinate;
    int index;
    struct Order *next;
    pthread_mutex_t order_mutex;
} Order;

typedef struct
{
    Order *front;
    Order *rear;
    pthread_mutex_t mutex;
} Queue;

typedef struct
{
    sem_t oven_sem;
    sem_t entrance_sem;
    int pide_count;
    pthread_mutex_t oven_mutex;
} Oven;

typedef struct
{
    int id;
    pthread_t thread;
    int available;
    Oven *oven;
    Queue *order_queue;
    double cpu_time_used;
    pthread_mutex_t *manager_mutex;
    pthread_cond_t *order_cond;
} Chef;

typedef struct
{
    int id;
    pthread_t thread;
    int available;
    int delivery_count;
    double cpu_time_used;
    Queue delivery_order_queue;
    pthread_mutex_t *manager_mutex;
    pthread_mutex_t delivery_mutex;
    pthread_cond_t *delivery_cond;
} Delivery;

Chef *chefs;
Delivery *deliveries;
pthread_t manager_thread;
pthread_mutex_t manager_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;
sem_t ready_pide_sem;
Oven oven;
Queue order_queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
Queue delivery_order_queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};

int *client_pids;
int client_count = 0;
int n, m, running = 1;
int server_fd; // Server socket file descriptor
int index_of_order = 0;
int log_fd;
char *ip;

// Log operations
void write_to_log(const char *message)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", tm_info);

    char final_message[MAX_LOG_LENGTH];
    snprintf(final_message, MAX_LOG_LENGTH, "[%s] %s\n", timestamp, message); // Append timestamp to message

    if (write(log_fd, final_message, strlen(final_message)) == -1)
    {
        fprintf(stderr, "Error writing to file: %s\n",strerror(errno));
    }
}

// Queue operations
void enqueue(Queue *q, int client_socket, int client_pid, int x_coordinate, int y_coordinate, int index) // Add order to the end of the queue
{
    Order *new_order = (Order *)malloc(sizeof(Order));
    new_order->client_socket = client_socket;
    new_order->client_pid = client_pid;
    new_order->next = NULL;
    new_order->x_coordinate = x_coordinate;
    new_order->y_coordinate = y_coordinate;
    new_order->index = index;
    pthread_mutex_init(&new_order->order_mutex, NULL);
    pthread_mutex_lock(&q->mutex);
    if (q->rear == NULL)
    {
        q->front = q->rear = new_order;
    }
    else
    {
        q->rear->next = new_order;
        q->rear = new_order;
    }
    pthread_mutex_unlock(&q->mutex);
}

Order *dequeue(Queue *q) // Remove order from the front of the queue
{
    pthread_mutex_lock(&q->mutex);
    if (q->front == NULL)
    {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
    Order *order = q->front;
    q->front = q->front->next;
    if (q->front == NULL)
    {
        q->rear = NULL;
    }
    pthread_mutex_unlock(&q->mutex);
    return order;
}

void send_status(Order *order, const char *status) // Send status message to client
{
    pthread_mutex_lock(&order->order_mutex);
    send(order->client_socket, status, strlen(status), 0);
    pthread_mutex_unlock(&order->order_mutex);
}

void *chef_function(void *arg)
{
    Chef *chef = (Chef *)arg;
    clock_t start, end;

    while (running == 1) // Main loop
    {
        start = clock();
        char log_buffer[MAX_LOG_LENGTH];
        pthread_mutex_lock(&manager_mutex);
        while (order_queue.front == NULL) // Wait for new order 
        {
            pthread_cond_wait(&order_cond, &manager_mutex);
        }

        Order *order = dequeue(&order_queue);
        chef->available = 0;
        pthread_mutex_unlock(&manager_mutex);

        if (order != NULL) // If there is an order 
        {
            send_status(order, "Order received");
            enqueue(&delivery_order_queue, order->client_socket, order->client_pid, order->x_coordinate, order->y_coordinate, order->index);
            // Simulate preparing pide
            memset(log_buffer, 0, sizeof(log_buffer));
            sprintf(log_buffer, "Chef %d is preparing order %d\n", chef->id, order->index);
            printf("> %s", log_buffer);
            write_to_log(log_buffer);
            sleep(2);

            // Put pide in the oven
            sem_wait(&oven.entrance_sem);
            pthread_mutex_lock(&oven.oven_mutex);

            while (oven.pide_count >= MAX_PIDE_IN_OVEN) // Wait for oven to be empty
            {
                pthread_mutex_unlock(&oven.oven_mutex);
                sem_post(&oven.entrance_sem);
                sleep(1);
                sem_wait(&oven.entrance_sem);
                pthread_mutex_lock(&oven.oven_mutex);
            }

            oven.pide_count++;

            memset(log_buffer, 0, sizeof(log_buffer));
            sprintf(log_buffer, "Chef %d put order %d to the oven. Total pides in the oven: %d\n", chef->id, order->index, oven.pide_count);
            printf("> %s", log_buffer);
            write_to_log(log_buffer);

            pthread_mutex_unlock(&oven.oven_mutex);
            sem_post(&oven.entrance_sem);

            // Simulate cooking time
            sleep(1);

            pthread_mutex_lock(&oven.oven_mutex);
            oven.pide_count--;

            memset(log_buffer, 0, sizeof(log_buffer));
            sprintf(log_buffer, "Chef %d took a order %d out of the oven. Total pides in the oven: %d\n", chef->id, order->index, oven.pide_count);
            printf("> %s", log_buffer);
            write_to_log(log_buffer);

            pthread_mutex_unlock(&oven.oven_mutex);
            sem_post(&ready_pide_sem);
            send_status(order, "Order ready for delivery");

            // Notify manager that pide is ready for delivery
            pthread_mutex_lock(&manager_mutex);
            chef->available = 1;
            pthread_cond_signal(&delivery_cond); // bunu kapattım düzelt
            pthread_mutex_unlock(&manager_mutex);
        }
        end = clock();
        chef->cpu_time_used += ((double)(end - start)) / CLOCKS_PER_SEC; // Calculate CPU time used
    }

    return NULL;
}

void find_best_employee()
{
    double min_time = 9999999;
    int min_index = -1;
    for (int i = 0; i < n; i++)
    {
        if (chefs[i].cpu_time_used < min_time)
        {
            min_time = chefs[i].cpu_time_used;
            min_index = i;
        }
    }
    printf("> Best employee is Chef %d, thank you sir!\n", min_index);

    // and for the deliveries
    min_time = 9999999;

    for (int i = 0; i < m; i++)
    {
        if (deliveries[i].cpu_time_used < min_time)
        {
            min_time = deliveries[i].cpu_time_used;
            min_index = i;
        }
    }
    printf("> Best employee is Delivery %d, thank you sir!\n", min_index);
}

void *delivery_function(void *arg)
{
    Delivery *delivery = (Delivery *)arg;
    clock_t start, end;
    while (running == 1)
    {
        char log_buffer[MAX_LOG_LENGTH];
        if (delivery->available == 0)
        {
            start = clock();
            // printf("+++++++++++++++Hiç geldi mi\n");
            pthread_mutex_lock(&delivery->delivery_mutex);
            while (delivery->delivery_count < max_delivery_capacity && running == 1)
            {
                if (client_count <= max_delivery_capacity && delivery->delivery_count != 0)
                {
                    // printf("breaka geldi\n");
                    break;
                }
                // printf("1-Delivery %d is waiting for pides count: %d, client_count: %d\n", delivery->id, delivery->delivery_count, client_count);
                pthread_cond_wait(&delivery_cond, &delivery->delivery_mutex);
                // printf("2-Delivery %d is waiting for pides count: %d, client_count: %d\n", delivery->id, delivery->delivery_count, client_count);
            }

            // Simulate delivery
            // printf("Delivery %d is delivering %d pides\n", delivery->id, delivery->delivery_count);
            // send_status(order->client_socket, "Order received\n");
            while (delivery->delivery_count != 0)
            {
                Order *order = dequeue(&delivery->delivery_order_queue);
                send_status(order, "Order is in the way");

                memset(log_buffer, 0, sizeof(log_buffer));
                sprintf(log_buffer, "Delivery %d is delivering order %d to adress (%d, %d)\n", delivery->id, order->index, order->x_coordinate, order->y_coordinate);
                printf("> %s", log_buffer);
                write_to_log(log_buffer);

                sleep(3);
                delivery->delivery_count--;
                client_count--;

                memset(log_buffer, 0, sizeof(log_buffer));
                sprintf(log_buffer, "Delivery %d delivered order %d to adress (%d, %d). Remaining pides to deliver: %d\n", delivery->id, order->index, order->x_coordinate, order->y_coordinate, delivery->delivery_count);
                printf("> %s", log_buffer);
                write_to_log(log_buffer);

                send_status(order, "Order is delivered");
                if (client_count == 0)
                {
                    memset(log_buffer, 0, sizeof(log_buffer));
                    sprintf(log_buffer, "done serving client %d\n", order->client_pid);
                    printf("> %s", log_buffer);
                    write_to_log(log_buffer);
                }
                close(order->client_socket);
                free(order);
            }
            delivery->available = 1;
            pthread_mutex_unlock(&delivery->delivery_mutex);
            end = clock();
            delivery->cpu_time_used += ((double)(end - start)) / CLOCKS_PER_SEC;
            if (client_count == 0)
            {
                find_best_employee();
            }
        }
    }

    return NULL;
}

void *manager_function(void *arg)
{
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Socket oluşturma
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // setsockopt() to allow reusing the address and port
    if(is_ip == 1)
    {
        int optval = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
            perror("setsockopt failed");
            exit(EXIT_FAILURE);
        }
    }


    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl get failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl set failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if(is_ip == 1) // If ip is given
    {
        address.sin_addr.s_addr = inet_addr(ip);
    }
    else // If ip is not given
        address.sin_addr.s_addr = INADDR_ANY;
    // Soketi bağlama
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Dinleme
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("> Manager is waiting for orders...\n");

    while (running == 1)
    {
        // printf("OOOOOOOOOOOOOOmanager new socket oluşturmadan önce\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                // No incoming connections, continue loop
                // Assign ready pides to a free delivery
                for (int i = 0; i < m; i++)
                {
                    if (deliveries[i].delivery_count < max_delivery_capacity && deliveries[i].available == 1)
                    {
                        pthread_mutex_lock(&deliveries[i].delivery_mutex);
                        if (sem_trywait(&ready_pide_sem) == 0) // If there is a ready pide assign it to the firrst available delivery
                        {
                            // printf("SEEEEEEMMMMMMMMMMMMMM\n");
                            Order *order1 = dequeue(&delivery_order_queue);
                            enqueue(&deliveries[i].delivery_order_queue, order1->client_socket, order1->client_pid, order1->x_coordinate, order1->y_coordinate, order1->index);
                            deliveries[i].delivery_count++;
                            if ((deliveries[i].delivery_count == max_delivery_capacity) || (deliveries[i].delivery_count < max_delivery_capacity && client_count <= max_delivery_capacity))
                            {
                                deliveries[i].available = 0;
                                // printf("Delivery %d is delivering %d pides\n", deliveries[i].id, deliveries[i].delivery_count);
                                pthread_cond_signal(&delivery_cond);// wake up delivery thread
                                pthread_cond_signal(&delivery_cond);
                            }
                            pthread_mutex_unlock(&deliveries[i].delivery_mutex); // Unlock delivery mutex
                            break;
                        }
                        else
                            pthread_mutex_unlock(&deliveries[i].delivery_mutex);
                    }
                }
                usleep(100000); // 100ms sleep to avoid busy-waiting
                
                continue;
            }
        }

        read(new_socket, buffer, BUFFER_SIZE);

        int client_pid;
        int num_of_client;
        int length, width;
        int customer_x_coordinate, customer_y_coordinate;
        sscanf(buffer, "%d Order from PID: %d %d %d %d %d", &num_of_client, &client_pid, &length, &width, &customer_x_coordinate, &customer_y_coordinate);
        pide_y_coordinate = length / 2;
        pide_x_coordinate = width / 2;
        // Client PID'sini kaydet
        pthread_mutex_lock(&manager_mutex);
        client_pids = realloc(client_pids, (client_count + 1) * sizeof(int));
        client_pids[client_count++] = client_pid;
        pthread_mutex_unlock(&manager_mutex);

        // printf("AAAAAAAAAAAAAAAAAAAAAAAAAA\n");
        pthread_mutex_lock(&manager_mutex);
        enqueue(&order_queue, new_socket, client_pid, customer_x_coordinate, customer_y_coordinate, index_of_order);
        index_of_order++;
        pthread_cond_signal(&order_cond); // Notify chefs about new order
        pthread_mutex_unlock(&manager_mutex);

        // Assign order to a free chef
        // printf("CCCCCCCCCCCCCCCCCCCCCC\n");
        pthread_mutex_lock(&manager_mutex);
        for (int i = 0; i < n; i++)
        {
            if (chefs[i].available)
            {
                chefs[i].available = 0;
                pthread_cond_signal(&order_cond);
                break;
            }
        }
        pthread_mutex_unlock(&manager_mutex);

        // printf("fordan önce***************\n");
        for (int i = 0; i < m; i++)
        {
            if (deliveries[i].delivery_count < max_delivery_capacity && deliveries[i].available == 1)
            {
                // printf("Buraya geliyomuuuuuuuuuuuuuuuuuuuuuuuaaaaaaaaaaaaaa\n");
                pthread_mutex_lock(&deliveries[i].delivery_mutex);
                // printf("Buraya geliyomuuuuuuuuuuuuuuuuuuuuuuu\n");
                if (sem_trywait(&ready_pide_sem) == 0) // If there is a ready pide assign it to the firrst available delivery
                {
                    Order *order1 = dequeue(&delivery_order_queue);
                    enqueue(&deliveries[i].delivery_order_queue, order1->client_socket, order1->client_pid, order1->x_coordinate, order1->y_coordinate, order1->index);
                    deliveries[i].delivery_count++;
                    if ((deliveries[i].delivery_count == max_delivery_capacity) || (deliveries[i].delivery_count < max_delivery_capacity && client_count <= max_delivery_capacity))
                    {
                        deliveries[i].available = 0;
                        // printf("Delivery %d is delivering %d pides\n", deliveries[i].id, deliveries[i].delivery_count);
                        pthread_cond_signal(&delivery_cond);
                        pthread_cond_signal(&delivery_cond);
                    }
                    pthread_mutex_unlock(&deliveries[i].delivery_mutex);
                    break;
                }
                else
                    pthread_mutex_unlock(&deliveries[i].delivery_mutex);
            }
        }
    }
    return NULL;
}

// Signal handler for SIGINT (^C)
// It closes the server and writes the log file
void signal_handler(int signum)
{
    printf("> ^C.. Upps quiting.. writing log file\n");
    char log_buffer[MAX_LOG_LENGTH];
    memset(log_buffer, 0, sizeof(log_buffer));
    sprintf(log_buffer, "Server is closed by ^C\n");
    write_to_log(log_buffer);
    pthread_mutex_lock(&manager_mutex);
    while (order_queue.front != NULL)
    {
        Order *order = dequeue(&order_queue);
        close(order->client_socket);
        kill(order->client_pid, SIGINT);
        free(order);
    }
    kill(client_pids[0], SIGINT);
    pthread_mutex_unlock(&manager_mutex);
    running = 0;

    // Chefs and deliveries termination
    for (int i = 0; i < n; i++)
    {
        pthread_cond_signal(&order_cond); // Wake up all chef threads
    }

    for (int i = 0; i < m; i++)
    {
        pthread_cond_signal(&delivery_cond); // Wake up all delivery threads
    }

    printf("All chefs and deliveries joined\n");
    pthread_mutex_unlock(&manager_mutex);
    // int res = pthread_join(manager_thread, NULL);
    // printf("Manager joined with result %d\n", res);
    close(server_fd);

    free(chefs);
    free(deliveries);
    free(client_pids);
    close(log_fd);
    exit(0);
}

void sigpipe_handler(int signo) {
    printf("QUEUE boşaltılıyor.\n");
    //queue yu yok edicem
    while (order_queue.front != NULL)
    {
        Order *order = dequeue(&order_queue);
        close(order->client_socket);
        free(order);
    }
}

int main(int argc, char const *argv[])
{
    if (argc != 5 && argc != 6) // Error handling
    {
        fprintf(stderr, "Usage: %s [portnumber] [CookthreadPoolSize] [DeliveryPoolSize] [k] or %s [ip] [portnumber] [CookthreadPoolSize] [DeliveryPoolSize] [k] \n", argv[0], argv[0]);
        return 1;
    }
    
    if(argc == 6)
    {
        is_ip = 1;
        ip = (char *)malloc(strlen(argv[1]) * sizeof(char));
        strcpy(ip, argv[1]);
        port = atoi(argv[2]);
        n = atoi(argv[3]);
        m = atoi(argv[4]);
        max_delivery_capacity = atoi(argv[5]);
    }
    else
    {
        is_ip = 0;
        port = atoi(argv[1]);
        n = atoi(argv[2]);
        m = atoi(argv[3]);
        max_delivery_capacity = atoi(argv[4]);
    }
    

    log_fd = open("log.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (log_fd == -1)
    {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    chefs = (Chef *)malloc(n * sizeof(Chef));
    deliveries = (Delivery *)malloc(m * sizeof(Delivery));
    client_pids = (int *)malloc(0);

    sem_init(&oven.oven_sem, 0, MAX_PIDE_IN_OVEN);
    sem_init(&oven.entrance_sem, 0, OVEN_ENTRANCES);
    sem_init(&ready_pide_sem, 0, 0);
    pthread_mutex_init(&oven.oven_mutex, NULL);

    // Aşçıları başlatma
    for (int i = 0; i < n; i++)
    {
        chefs[i].id = i;
        chefs[i].available = 1;
        chefs[i].oven = &oven;
        chefs[i].order_queue = &order_queue;
        chefs[i].manager_mutex = &manager_mutex;
        chefs[i].order_cond = &order_cond;
        chefs[i].cpu_time_used = 0.0;
        pthread_create(&chefs[i].thread, NULL, chef_function, &chefs[i]);
    }

    for (int i = 0; i < m; i++)
    {
        deliveries[i].id = i;
        deliveries[i].available = 1;
        deliveries[i].delivery_count = 0;
        deliveries[i].manager_mutex = &manager_mutex;
        deliveries[i].cpu_time_used = 0.0;
        if (pthread_mutex_init(&deliveries[i].delivery_mutex, NULL) != 0)
        {
            perror("Mutex initialization failed");
            exit(EXIT_FAILURE);
        }

        deliveries[i].delivery_cond = &delivery_cond;
        deliveries[i].delivery_order_queue.front = NULL;
        deliveries[i].delivery_order_queue.rear = NULL;
        if (pthread_mutex_init(&deliveries[i].delivery_order_queue.mutex, NULL) != 0)
        {
            perror("Mutex initialization failed");
            exit(EXIT_FAILURE);
        }
        pthread_create(&deliveries[i].thread, NULL, delivery_function, &deliveries[i]);
    }

    // Signal handler ayarla
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa1;
    sa1.sa_handler = sigpipe_handler;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa1, NULL) < 0) {
        perror("Sigaction failed");
        exit(EXIT_FAILURE);
    }
    // Manager thread
    pthread_create(&manager_thread, NULL, manager_function, NULL);

    // Bekleme
    pthread_join(manager_thread, NULL);
     for (int i = 0; i < n; i++) // Wait for all chef threads to finish
    {
        int res = pthread_join(chefs[i].thread, NULL); // Wait for all chef threads to finish
        // printf("Chef %d joined with result %d\n", i, res);
    }

    for (int i = 0; i < m; i++) // Wait for all delivery threads to finish
    {
        // printf("Delivery %d joined with result %d\n", i, pthread_join(deliveries[i].thread, NULL));
        int res = pthread_join(deliveries[i].thread, NULL); // Wait for all delivery threads to finish
        // printf("Delivery %d joined with result %d\n", i, res);
    }

    

    free(chefs);
    free(deliveries);
    free(client_pids);
    close(log_fd);
    printf("Server is closed\n");
    return 0;
}
