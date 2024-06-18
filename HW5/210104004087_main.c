#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <asm-generic/signal-defs.h>
#include <bits/sigaction.h>
#include <bits/pthreadtypes.h>


#define MAX_BUFFER_SIZE 1024
#define MAX_FILENAME_LENGTH 256

typedef struct
{
    char src[MAX_FILENAME_LENGTH];
    char dest[MAX_FILENAME_LENGTH];
} FilePair;

typedef struct
{
    FilePair *buffer;
    int buffer_size;
    int count;
    int head;
    int tail;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    int regular_files;
    int fifo_files;
    int directories;
    size_t total_bytes_copied;
} Buffer;

Buffer buffer;
int num_workers;
size_t buffer_size;
char *src_dir;
char *dest_dir;
int running = 1;
int static count = 0;


pthread_barrier_t barrier;
pthread_t manager_thread;
pthread_t *workers;

void *manager(void *arg);
void *worker(void *arg);
void initialize_buffer(Buffer *buffer, int size);
void destroy_buffer(Buffer *buffer);
void add_to_buffer(Buffer *buffer, const char *src, const char *dest);
FilePair remove_from_buffer(Buffer *buffer);
void copy_directory(const char *src, const char *dest);
void intHandler(int dummy);


int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <buffer_size> <num_workers> <src_dir> <dest_dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = intHandler;
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    buffer_size = atoi(argv[1]);
    num_workers = atoi(argv[2]);
    src_dir = argv[3];
    dest_dir = argv[4];

    workers = malloc(sizeof(pthread_t) * num_workers);
    initialize_buffer(&buffer, buffer_size);

    pthread_barrier_init(&barrier, NULL, num_workers);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    pthread_create(&manager_thread, NULL, manager, NULL);
    for (int i = 0; i < num_workers; i++)
    {
        pthread_create(&workers[i], NULL, worker, NULL);
    }

    pthread_join(manager_thread, NULL);
    for (int i = 0; i < num_workers; i++)
    {
        // printf("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAa\n");
        pthread_join(workers[i], NULL);
    }

    gettimeofday(&end, NULL);
    long seconds = (end.tv_sec - start.tv_sec);
    long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size:  %ld\n", num_workers, buffer_size);
    printf("Number of Regular File: %d\n", buffer.regular_files);
    printf("Number of FIFO File: %d\n", buffer.fifo_files);
    printf("Number of Directory: %d\n", buffer.directories);
    printf("TOTAL BYTES COPIED: %zu\n", buffer.total_bytes_copied);
    printf("Execution time: %ld seconds and %ld micros\n", seconds, micros);

    free(workers);
    destroy_buffer(&buffer);
    pthread_barrier_destroy(&barrier);
    return 0;
}

//****************Buffer functions****************

// Initialize the buffer with the given size and initialize the mutex and condition variables
void initialize_buffer(Buffer *buffer, int size)
{
    buffer->buffer = malloc(sizeof(FilePair) * size);
    buffer->buffer_size = size;
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->done = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    buffer->regular_files = 0;
    buffer->fifo_files = 0;
    buffer->directories = 0;
    buffer->total_bytes_copied = 0;
}

// Destroy the buffer and free the memory
void destroy_buffer(Buffer *buffer)
{
    free(buffer->buffer);
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_full);
    pthread_cond_destroy(&buffer->not_empty);
}

// Add a file pair to the buffer
void add_to_buffer(Buffer *buffer, const char *src, const char *dest)
{
    // printf("Locktan önce\n");
    pthread_mutex_lock(&buffer->mutex); // Lock the buffer
    // printf("Locktan sonra\n");
    while (buffer->count == buffer->buffer_size && running)
    {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex); // If the buffer is full, wait for it to be not full
        if (!running) // If the program is not running, unlock the mutex to prevent other mutexes wait and return
        {
            pthread_mutex_unlock(&buffer->mutex);
            return;
        }
        
    }
    // printf("While'dan sonra\n");
    strncpy(buffer->buffer[buffer->tail].src, src, MAX_FILENAME_LENGTH);
    strncpy(buffer->buffer[buffer->tail].dest, dest, MAX_FILENAME_LENGTH);
    buffer->tail = (buffer->tail + 1) % buffer->buffer_size; // Update the tail
    buffer->count++; // Increment the count
    pthread_cond_signal(&buffer->not_empty); // Signal that the buffer is not empty
    pthread_mutex_unlock(&buffer->mutex); // Unlock the buffer
}

// Remove a file pair from the buffer 
FilePair remove_from_buffer(Buffer *buffer)
{
    pthread_mutex_lock(&buffer->mutex); // Lock the buffer
    while (buffer->count == 0 && !buffer->done && running) // If the buffer is empty, wait for it to be not empty
    {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
        if (!running) // If the program is not running, unlock the mutex to prevent other mutexes wait and return
        {
            // printf("Running 0, return edilecek\n");
            pthread_mutex_unlock(&buffer->mutex);
            return buffer->buffer[buffer->head];
        }
        
    }
    FilePair file_pair;
    if (buffer->count > 0) // If the buffer is not empty, remove a file pair from the buffer
    {
        file_pair = buffer->buffer[buffer->head];
        buffer->head = (buffer->head + 1) % buffer->buffer_size;
        buffer->count--;

        // Update statistics
        struct stat statbuf;
        if (stat(file_pair.src, &statbuf) == 0)
        {
            if (S_ISREG(statbuf.st_mode)) // If it's a regular file, update the statistics
            {
                buffer->regular_files++;
                buffer->total_bytes_copied += statbuf.st_size;
            }
            else if (S_ISFIFO(statbuf.st_mode)) // If it's a FIFO file, update the statistics
            {
                buffer->fifo_files++;
            }
            else if (S_ISDIR(statbuf.st_mode)) // If it's a directory, update the statistics
            {
                buffer->directories++;
            }
        }
        else
        {
            perror("stat");
        }
        pthread_cond_signal(&buffer->not_full); // Signal that the buffer is not full
    }
    else
    {
        file_pair.src[0] = '\0';
        file_pair.dest[0] = '\0';
    }
    pthread_mutex_unlock(&buffer->mutex); // Unlock the buffer
    return file_pair;
}

//****************Manager Function****************
// Manager function to copy the source directory to the destination directory
void *manager(void *arg)
{
    copy_directory(src_dir, dest_dir); // Copy the source directory to the destination directory

    // printf("Manager done copying files\n");
    pthread_mutex_lock(&buffer.mutex); // Lock the buffer
    buffer.done = 1;
    pthread_cond_broadcast(&buffer.not_empty); // Wake up all workers
    pthread_mutex_unlock(&buffer.mutex);

    return NULL;
}

// Copy the files from the source directory to the destination directory
void copy_directory(const char *src, const char *dest)
{
    struct dirent *entry;
    DIR *dp = opendir(src);

    if (dp == NULL)
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Ensure the destination directory exists
    mkdir(dest, 0755);

    // Copy files and directories
    while ((entry = readdir(dp)) && running)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char src_path[MAX_FILENAME_LENGTH];
        char dest_path[MAX_FILENAME_LENGTH];
        // Check if src_path will overflow
        if (snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name) >= sizeof(src_path)) {
            fprintf(stderr, "Path too long: %s/%s\n", src, entry->d_name);
            continue;
        }

        // Check if dest_path will overflow
        if (snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name) >= sizeof(dest_path)) {
            fprintf(stderr, "Path too long: %s/%s\n", dest, entry->d_name);
            continue;
        }

        struct stat statbuf;
        if (stat(src_path, &statbuf) == 0) // Get file status and check if it exists and is accessible
        {
            if (S_ISREG(statbuf.st_mode)) // If it's a regular file, add it to the buffer
            {
                add_to_buffer(&buffer, src_path, dest_path);
            }
            else if (S_ISDIR(statbuf.st_mode)) // If it's a directory, recursively copy it
            {
                buffer.directories++;
                copy_directory(src_path, dest_path);
            }
        }
        else
        {
            perror("stat");
        }
    }

    // printf("Closing directory %s\n", src);
    closedir(dp);
}

//****************Worker Function****************
// Worker function to copy files it gets from the buffer and write them to the destination directory
void *worker(void *arg)
{
    pthread_barrier_wait(&barrier); // Synchronize threads at this point to start copying files
    while (running)
    {
        FilePair file_pair = remove_from_buffer(&buffer); // Get a file pair from the buffer to copy
        
        if (file_pair.src[0] == '\0' && file_pair.dest[0] == '\0') // If the buffer is empty and done, exit
        {
            break;
        }

        int src_fd = open(file_pair.src, O_RDONLY); // Open the source file
        if (src_fd < 0)
        {
            perror("open src");
            continue;
        }

        int dest_fd = open(file_pair.dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dest_fd < 0)
        {
            perror("open dest");
            close(src_fd);
            continue;
        }

        char buf[MAX_BUFFER_SIZE];
        ssize_t bytes_read, bytes_written;
        while ((bytes_read = read(src_fd, buf, buffer_size)) > 0)
        {
            bytes_written = write(dest_fd, buf, bytes_read);
            if (bytes_written != bytes_read)
            {
                perror("write");
                break;
            }
        }

        if (bytes_read < 0)
        {
            perror("read");
        }

        close(src_fd);
        close(dest_fd);

        //printf("debug\n");
    }
    pthread_barrier_wait(&barrier); // Synchronize threads at this point to finish copying files
    //printf("Worker exiting...\n");
    return NULL;
}

// All the workers are not ending because of the signal handler. I tried to use pthread_cancel but it didn't work. 
void intHandler(int dummy)
{
    printf("\nCTRL-C caught. Exiting...\n");
    running = 0; // Set running to 0 to signal all threads to exit from while loop
    pthread_cond_signal(&buffer.not_full);
    pthread_cond_signal(&buffer.not_empty);
    buffer.done = 1; 
    pthread_mutex_unlock(&buffer.mutex);
    pthread_cond_broadcast(&buffer.not_empty); // Wake up all workers
    pthread_cond_broadcast(&buffer.not_full); // Wake up all workers
}