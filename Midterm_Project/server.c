#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <bits/waitflags.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <asm-generic/signal-defs.h>
#include <sys/wait.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>

#define FIFO_PATH "FIFO"
#define BUFFER_SIZE 128
#define MAX_WORDS 5
#define RESPONSE_SIZE 1024
#define MAX_LINE_LENGTH 100
#define MAX_TOKENS 50
#define NUM_OF_FILES 128
#define SHM_PATH "/shared_memory"
#define LOG_FILE_NAME "log.txt"


typedef struct {
    char file_name[50];
    pthread_mutex_t mutex;
} FileData;

FileData *files_data;

struct Node {
    int data;
    struct Node* next;
};

struct Queue {
    struct Node *front, *rear;
    int size;
};

struct Node* newNode(int data);

struct Queue* createQueue();

void enqueue(struct Queue* q, int data);

int dequeue(struct Queue* q);

int isEmpty(struct Queue* q);

void destroyQueue(struct Queue* q);

void list_command(char response[RESPONSE_SIZE], int write_fifo_fd, char* dir_name);

void read_all_file(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, char* dirname);

void read_line_file(char response[RESPONSE_SIZE], char* file_name, int line_number, int write_fifo_fd, char* dirname);

void write_to_spesific_file_line(char response[RESPONSE_SIZE], char* file_name, int line_number, char* string_message, int write_fifo_fd, char* dirname);

void write_to_end_of_file(char response[RESPONSE_SIZE], char* file_name, char* text, int write_fifo_fd, char* dirname);

void write_to_fifo(int write_fifo_fd, char response[RESPONSE_SIZE]);
void upload_from_client_to_server(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, pid_t client_pid, char* dir_name); //response, filename, fifo

void download_from_server_to_client(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, pid_t client_pid, char* dir_name, int arch); //response, filename, fifo

int copy_file(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, char* src, char* dest, int arch); //response, filename, fifo

int check_file_existence(const char *filepath);

void archive_server(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, char* dir_name); //response, filename, fifo

void remove_pid(pid_t pid, int max_clients);

int addClient(pid_t child_pid, pid_t client_pid);

void init_client_array();

void destroy_client_array();

void killServer();

char *concatenateStrings(char **strArray, int size, int limit);

int tokenize(char* words[MAX_WORDS], char* buffer);

int open_write_fifo(pid_t client_pid);

int open_read_fifo(pid_t client_pid);

void init_files_data();

void init_shared_mutex(pthread_mutex_t *mutex);

int is_file_in_shared_memory(char* file_name);

int add_file_to_shared_memory(char* file_name);

int enter_critical_region(char* file_name);

void exit_critical_region(char* file_name, int file_index);

void write_to_log_file(char log_buffer[BUFFER_SIZE], int log_fd);

void destroy_words(char *words[MAX_WORDS], int word_count);

void destroy_mutex();

int number_of_clients = 0, log_fd;
int* children_pids;
int* clients_pids;
int max_number_of_clients, shm_fd, shm_size;
char log_buffer[BUFFER_SIZE];
struct Queue* client_queue;

void sigchld_handler(int sig) 
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        number_of_clients--;
        remove_pid(pid, max_number_of_clients); //Removes dead child
        // if (WIFEXITED(status)) 
        // {
        //     //printf("Child process %d terminated with exit status %d\n", pid, WEXITSTATUS(status));
        // }
        // else if (WIFSIGNALED(status)) 
        // {
        //     printf("Child process %d terminated by signal %d\n", pid, WTERMSIG(status));
        // }
        // else
        //     printf("signal handler else'e geldi\n");
    }
}

//When command is killServer for child process, it sends a signal to terminate program
void signal_handler(int sig) 
{
    sprintf(log_buffer, "Kill server signal handler.\n");
    write_to_log_file(log_buffer, log_fd);
    close(log_fd);
    unlink(FIFO_PATH);
    killServer();
    destroy_mutex();
    close(shm_fd);
    munmap(files_data, shm_size);
    shm_unlink(SHM_PATH);
}

//For ctrl c
void sigint_handler(int sig) 
{
    sprintf(log_buffer, "Signal Interrupt.\n");
    write_to_log_file(log_buffer, log_fd);
    close(log_fd);
    killServer();
    unlink(FIFO_PATH);
    destroy_mutex();
    close(shm_fd);
    munmap(files_data, shm_size);
    shm_unlink(SHM_PATH);
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
    int fifo_fd, dummy_fd, word_count = 0, status;
    int client_index = 0;
    int is_client_connected = 0;
    pid_t child, parent;
    char buffer[BUFFER_SIZE];
    
    char *command_tokens[MAX_WORDS];
    char* dir_name;

    //Argumant check
    if(argc != 3)
    {
        fprintf(stderr, "Usage:  neHosServer <dirname> <max. #ofClients> \n");
        exit(EXIT_FAILURE);
    }
    max_number_of_clients = atoi(argv[2]);
    dir_name = argv[1];
    client_queue  = createQueue();

    struct stat st;
    if (stat(dir_name, &st) == -1) {
        if (mkdir(dir_name, 0777) == -1) 
        {
            perror("Klasör zaten var.");
        }
    }

    printf(">> Server Started PID: %d\n>> Waiting for clients...\n", getpid());
    log_fd = open(LOG_FILE_NAME, O_WRONLY | O_CREAT | O_APPEND, 0666);
    //Initializes client array with -1
    init_client_array();
    
    //Shared memory file opening
    if ((shm_fd = shm_open(SHM_PATH, O_CREAT | O_RDWR, 0666)) == -1)
    {
        perror("Error creating shared memory segment");
        exit(EXIT_FAILURE);
    }

    shm_size = sizeof(FileData) * NUM_OF_FILES; //shared memory size

    if (ftruncate(shm_fd, shm_size) == -1)
    {
        perror("Error setting size of shared memory segment");
        exit(EXIT_FAILURE);
    }
    //assigns allocated shared memory to the variable
    if ((files_data = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
    {
        perror("Error mapping shared memory segment");
        exit(EXIT_FAILURE);
    }    

    //İnitializes shared file array.
    init_files_data();
    sprintf(log_buffer, "Server is created. Server pid: %d\n*", getpid());
    strcpy(buffer, log_buffer);
    write_to_log_file(buffer, log_fd);

    //Signal adjustments
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sigact;
    sigact.sa_handler = signal_handler;
    sigact.sa_flags = 0;
    sigemptyset(&sigact.sa_mask);

    if (sigaction(SIGUSR1, &sigact, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sigact_int;
    sigact_int.sa_handler = sigint_handler;
    sigemptyset(&sigact_int.sa_mask);
    sigact_int.sa_flags = 0;

    // SIGINT (Ctrl+C)
    if (sigaction(SIGINT, &sigact_int, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    //fifo creation and error check
    if (mkfifo(FIFO_PATH, 0666) == -1) 
    {
        if (errno == EEXIST) 
        {
            perror("fifo is exist");
            exit(EXIT_FAILURE);
        }
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    //Tries to open fifo file as long as there is signal interrupt
    while(((fifo_fd = open(FIFO_PATH, O_RDWR | O_NONBLOCK, 0666)) == -1) && (errno == EINTR));
    if (fifo_fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    if ((dummy_fd = open(FIFO_PATH, O_WRONLY)) == -1)
        exit(EXIT_FAILURE);

    sprintf(log_buffer, "Main Server FIFO is created\n*");
    write_to_log_file(log_buffer, log_fd);

    int i = 0;
    while(1)
    {
        int write_fifo_fd, read_fifo_fd;
        char *words[MAX_WORDS];
        pid_t client_pid;
        char client_fifo_write[50];
        char client_fifo_read[50];
        char response[RESPONSE_SIZE];
        
        if(is_client_connected == 0)
        {
            ssize_t bytes_read;
            
            bytes_read = read(fifo_fd, buffer, BUFFER_SIZE);

            if (bytes_read == -1) //nothing from fifo
            {

                if(isEmpty(client_queue) == 1) // queue is empty so continue
                {
                    continue;
                }
                else //there are clients in queue
                {
                    // printf("number ***of clients: %d\n", number_of_clients);

                    if(number_of_clients < max_number_of_clients)
                    {
                        pid_t next_client = dequeue(client_queue);
                        memset(buffer, 0, sizeof(buffer));
                        sprintf(buffer, "connect %d", next_client);
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            else //Data from fifo
            {
                // printf("number of clients: %d\n", number_of_clients);
                if(number_of_clients < max_number_of_clients && isEmpty(client_queue) != 1) //queue is not empty
                {// ama burda queue ya koyulcak yeni clientin connecti de lazım

                    word_count = tokenize(words, buffer);
                    // for(int i=0; i<word_count; i++)
                    //     printf("words array token %d: %s\n", i+1, words[i]);

                    pid_t next_client = dequeue(client_queue);
                    memset(buffer, 0, sizeof(buffer));
                    sprintf(buffer, "connect %d", next_client);
                    if(strcmp(words[0], "connect") == 0) //yeni geleni queue ya ekledi
                    {
                        pid_t id = atoi(words[1]);
                        enqueue(client_queue, id);
                        // printf("ENQUEEEEEEE******\n");
                        write_fifo_fd = open_write_fifo(id);
                        memset(response, 0, RESPONSE_SIZE);
                        strcpy(response, "full");
                        write_to_fifo(write_fifo_fd, response);
                        close(write_fifo_fd);
                    }
                }
                else if(number_of_clients >= max_number_of_clients)
                {
                    word_count = tokenize(words, buffer);
                    // for(int i=0; i<word_count; i++)
                    //     printf("words array token %d: %s\n", i+1, words[i]);

                    if(strcmp(words[0], "connect") == 0) //yeni geleni queue ya ekledi
                    {
                        pid_t id = atoi(words[1]);
                        // printf("ENQUEEEEEEE***+++***\n");

                        enqueue(client_queue, id);
                        sprintf(log_buffer, "Server is full. Client %d added to queue.\n*", id);
                        write_to_log_file(log_buffer, log_fd);
                        write_fifo_fd = open_write_fifo(id);
                        memset(response, 0, RESPONSE_SIZE);
                        strcpy(response, "full");
                        write_to_fifo(write_fifo_fd, response);
                        close(write_fifo_fd);
                        continue;
                    }
                    else //tryConnect case
                    {
                        printf(">>Connection request PID %d… Que FULL. Client leaving... \n",atoi(words[1]));
                        sprintf(log_buffer, "Server is full. Client %d left.\n*", atoi(words[1]));
                        write_to_log_file(log_buffer, log_fd);
                        write_fifo_fd = open_write_fifo(atoi(words[1]));
                        memset(response, 0, RESPONSE_SIZE);
                        strcpy(response, "full");
                        write_to_fifo(write_fifo_fd, response);
                        close(write_fifo_fd);
                        memset(response, 0, RESPONSE_SIZE);
                        continue;
                    }
                }
            }
            destroy_words(words, word_count);
            word_count = tokenize(words, buffer);
            client_pid = atoi(words[1]);
        }

        write_fifo_fd = open_write_fifo(client_pid);

        if(((strcmp(words[0], "tryConnect") == 0 || strcmp(words[0], "connect") == 0) && (word_count) == 2) || is_client_connected == 1) // Connection between server and client
        {
            if(is_client_connected == 0)
            {
                word_count = 0;
                if(number_of_clients < max_number_of_clients) // connect
                {
                    client_index = addClient(client_pid, client_pid);//add client and forked process to the arrays.
                }
                parent = getpid();
                child = fork();

                if(child != 0)
                {
                    children_pids[client_index] = child; // normalde çocuğun olması gereken yere onunla ilişkili forklanmış yer koyuldu
                    clients_pids[client_index] = client_pid;
                    sprintf(log_buffer, "Child process %d is created.\n*", child);
                    write_to_log_file(log_buffer, log_fd);
                }
            }
            if (child == -1) 
            {
                perror("Error while creating child process.\n");
                sprintf(log_buffer, "Error while forking child.\n*");
                write_to_log_file(log_buffer, log_fd);
                exit(EXIT_FAILURE);
            }
            else if(child == 0)
            {
                if(is_client_connected == 0) // It is not connected yet.
                {
                    memset(response, 0, RESPONSE_SIZE);
                    strcpy(response, "connected");
                    write_to_fifo(write_fifo_fd, response);
                    is_client_connected = 1;
                    number_of_clients ++;
                    sprintf(log_buffer, ">>Client PID:'%d' connected as 'client%d'\n*", client_pid, client_index);
                    write_to_log_file(log_buffer, log_fd);
                    printf("%s", log_buffer);
                    
                    children_pids[client_index] = getpid(); // Artık çocuğun kendisinde de güncellendi. Çünkü çocuk sadece forktan öncekini biliyordu
            
                    read_fifo_fd = open_read_fifo(client_pid);     
                }

                memset(buffer, 0, sizeof(buffer));

                if (read(read_fifo_fd, buffer, BUFFER_SIZE) == -1) // reads command from fifo
                {
                    perror("read");
                    close(fifo_fd);
                    exit(EXIT_FAILURE);
                }
                int command_token_count = tokenize(command_tokens, buffer);
                memset(response, 0, RESPONSE_SIZE);

                if((strcmp(command_tokens[0], "list") == 0) && command_token_count == 1)
                {
                    sprintf(log_buffer, "list command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    list_command(response, write_fifo_fd, argv[1]);
                }
                else if((strcmp(command_tokens[0], "readF") == 0) && (command_token_count == 3 || command_token_count == 2))
                {
                    sprintf(log_buffer, "readF command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    //It reads all the file
                    if(command_token_count == 2)
                    {
                        read_all_file(response, command_tokens[1], write_fifo_fd, argv[1]);
                    }
                    else if(command_token_count == 3) // reads spesific line
                    {
                        read_line_file(response, command_tokens[1], atoi(command_tokens[2]), write_fifo_fd, argv[1]);
                    }
                }
                else if((strcmp(command_tokens[0], "writeT") == 0) && command_token_count >= 3)
                {
                    char *endptr;
                    char *combinedText;
                    long linuNum = strtol(command_tokens[2], &endptr, 10); // 10 specifies base 10 (decimal)
                    sprintf(log_buffer, "writeT command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    if (*endptr != '\0')// sayı değil
                    {
                        combinedText = concatenateStrings(command_tokens, command_token_count, 1);
                        write_to_end_of_file(response, command_tokens[1], combinedText, write_fifo_fd, argv[1]);
                    }
                    else // sayı
                    {
                        combinedText = concatenateStrings(command_tokens, command_token_count, 2);
                        write_to_spesific_file_line(response, command_tokens[1], atoi(command_tokens[2]), combinedText, write_fifo_fd, argv[1]);
                    }
                    free(combinedText);
                    combinedText = NULL;
                }
                else if((strcmp(command_tokens[0], "upload") == 0) && command_token_count == 2)
                {
                    sprintf(log_buffer, "upload command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    upload_from_client_to_server(response, command_tokens[1], write_fifo_fd, client_pid, dir_name); //response, filename, fifo
                }
                else if((strcmp(command_tokens[0], "download") == 0) && command_token_count == 2)
                {
                    sprintf(log_buffer, "download command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    download_from_server_to_client(response, command_tokens[1], write_fifo_fd, client_pid, dir_name, 0); //response, filename, fifo
                }
                else if((strcmp(command_tokens[0], "archServer") == 0) && command_token_count == 2)
                {
                    sprintf(log_buffer, "archServer command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    archive_server(response, command_tokens[1], write_fifo_fd, dir_name); //response, filename, fifo
                    download_from_server_to_client(response, command_tokens[1], write_fifo_fd, client_pid, dir_name, 1); //response, filename, fifo
                }
                else if((strcmp(command_tokens[0], "quit") == 0) && command_token_count == 1)
                {
                    sprintf(log_buffer, "quit command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    strcpy(response, "quit successful");
                    write_to_fifo(write_fifo_fd, response);
                    printf("Client%d disconnected.", client_index);
                    exit(EXIT_SUCCESS);
                }
                else if((strcmp(command_tokens[0], "killServer") == 0) && command_token_count == 1)
                {
                    sprintf(log_buffer, "killServer command from Client%d.\n*", client_index);
                    write_to_log_file(log_buffer, log_fd);
                    strcpy(response, "killServer successful");
                    write_to_fifo(write_fifo_fd, response);
                    printf("kill signal from client%d. Terminating...", client_index);
                    kill(parent, SIGUSR1);
                    exit(EXIT_SUCCESS); 
                }
            }
            else
            {
                if(number_of_clients < max_number_of_clients) //It increases client number for parent process.
                    number_of_clients ++;
            }
        }
        destroy_words(words, word_count); //It deallocates
    }

    unlink(FIFO_PATH);
    free(children_pids);
    close(log_fd);
    return 0;
}

void list_command(char response[RESPONSE_SIZE], int write_fifo_fd, char* dir_name)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    // Open the directory
    dir = opendir(dir_name);
    if (dir == NULL) 
    {
        perror("opendir");
        sprintf(log_buffer, "Error opening directory.\n*");
        write_to_log_file(log_buffer, log_fd);
        return;
    }
    else
    {
        strcpy(response, "");  // Clear the response
        while ((entry = readdir(dir)) != NULL) 
        {
            // Ignore special entries "." and ".."
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
            {
                continue;
            }
            strcat(response, entry->d_name);
            strcat(response, "\n");
            // Print the name of the file
           // printf("%s\n", entry->d_name);
        }
    }         
    // Close the directory
    write_to_fifo(write_fifo_fd, response);
    closedir(dir);
}

void read_line_file(char response[RESPONSE_SIZE], char* file_name, int line_number, int write_fifo_fd, char* dirname)
{
    int fd, bytesRead, file_index;
    int lineIndex = 0;
    int line_count=0;
    char currentChar;
    int isLineDone = 0;
    char line[MAX_LINE_LENGTH];

    file_index = enter_critical_region(file_name);
    char server_file_path[50];

    sprintf(server_file_path, "./%s/%s", dirname, file_name);

    fd = open(server_file_path, O_RDONLY); // opens file

    if(fd != -1)
    {
        while ((bytesRead = read(fd, &currentChar, 1)) > 0) 
        {
            if(currentChar == '\n' || lineIndex == MAX_LINE_LENGTH - 1) //bir line'ın bittiğini anlar ve line üstünde işlem yapar
            {
                line[lineIndex] = '\0';
                lineIndex = 0;
                line_count++;
                //line üstünde işlemler
                line[strcspn(line, "\n")] = '\0';  // Remove newline character
                
                isLineDone = 1;
                if(line_count == line_number)
                {
                    strcpy(response, line);
                    // printf("response: %s\n", response);
                    break;
                }
            }
            else //line'ı oluşturmaya devam eder.
            {
                line[lineIndex] = currentChar;
                lineIndex++;
                isLineDone = 0;
            } 
        }
        if (bytesRead == 0) //End of the file, search is done
        {
            if(isLineDone == 0)
            {
                line_count++;
                if(line_count == line_number)
                {
                    line[lineIndex] = '\0';
                    strcpy(response, line);
                }
                else
                {
                    sprintf(log_buffer, "Line could not found.\n*");
                    write_to_log_file(log_buffer, log_fd);
                    //printf("Tüm file okundu end of file gelindi ve istenilen line bulunamadı\n"); // son satırı dene
                    strcpy(response, "line could not found");
                }
            }
            else
            {
                sprintf(log_buffer, "Line could not found.\n*");
                write_to_log_file(log_buffer, log_fd);
                //printf("Tüm file okundu end of file gelindi ve istenilen line bulunamadı\n"); // son satırı dene
            }
        }
        else if (bytesRead == -1) // error while reading file
        {
            sprintf(log_buffer, "Read command is failled.\n*");
            write_to_log_file(log_buffer, log_fd);
            perror("reading file");
            close(fd);
            strcpy(response, "File could not read.\n");
        }
    }
    else
    {
        // perror("open file");
        strcpy(response, "Reading file is failled.");
        sprintf(log_buffer, "Read command is failled.\n*");
        write_to_log_file(log_buffer, log_fd);
    }

    write_to_fifo(write_fifo_fd, response);
    close(fd);
    exit_critical_region(file_name, file_index);
}

void read_all_file(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, char* dirname)
{
    int fd, bytesRead, file_index;
    int lineIndex = 0;
    char currentChar;

    file_index = enter_critical_region(file_name);

    char server_file_path[50];

    sprintf(server_file_path, "./%s/%s", dirname, file_name);

    fd = open(server_file_path, O_RDONLY); // opens file

    if(fd != -1)
    {
        while ((bytesRead = read(fd, &currentChar, 1)) > 0) 
        {
            response[lineIndex] = currentChar;
            lineIndex++;
        }
        if (bytesRead == 0) //End of the file, search is done
        {
           // printf("Tüm file okundu end of file gelindi\n"); // son satırı dene 
        }
        else if (bytesRead == -1) // error while reading file
        {
            perror("reading file");
            close(fd);
        }
    }
    else
    {
        perror("open file");
        strcpy(response, "Failled");
        close(fd);
    }
    write_to_fifo(write_fifo_fd, response);
    close(fd);
    exit_critical_region(file_name, file_index);
}

void write_to_spesific_file_line(char response[RESPONSE_SIZE], char* file_name, int line_number, char* text, int write_fifo_fd, char* dirname)
{
    int temp_fd, fd;
    int bytesRead;
    int lineIndex = 0;
    int line_count=0;
    char currentChar;
    int isSuccess = 0;
    char line[MAX_LINE_LENGTH];

    int file_index;

    file_index = enter_critical_region(file_name);

    char server_file_path[50];

    sprintf(server_file_path, "./%s/%s", dirname, file_name);


    fd = open(server_file_path, O_RDWR);
    if(fd != -1)
    {
        char temp_file[50]; // Geçici dosya yolunu belirt
        sprintf(temp_file, "./%s/tempfile.txt", dirname);

        // Geçici dosyayı oluştur ve openla aç
        temp_fd = open(temp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        if (temp_fd == -1)
        {
            perror("open");
        }
        while ((bytesRead = read(fd, &currentChar, 1)) > 0) 
        {
            if(line_count + 1 != line_number)
            {
                if(currentChar == '\n' || lineIndex == MAX_LINE_LENGTH - 1) //bir line'ın bittiğini anlar ve line üstünde işlem yapar
                {
                    lineIndex = 0;
                    line_count++;
                }
                else //line'ı oluşturmaya devam eder.
                {
                    lineIndex++;
                }
                if(write(temp_fd, &currentChar, 1) == -1)
                {
                    perror("write error");
                }
            }
            else
            {
                //kendi şeyini yaz
                if(write(temp_fd, text, strlen(text)) == -1)
                {
                    perror("write error");
                }
                if(write(temp_fd, "\n", 1) == -1)
                {
                    perror("write error");
                }
                if(write(temp_fd, &currentChar, 1) == -1)
                {
                    perror("write error");
                }
                isSuccess = 1;
                line_count++;
            } 
        }
        if(isSuccess == 1)
        {
            strcpy(response, "Successful");
            sprintf(log_buffer, "Writing spesific line is successful.\n*");
            write_to_log_file(log_buffer, log_fd);
            if (remove(server_file_path) == 0) 
            {
                // printf("Dosya başarıyla silindi.\n");
            } 
            else
            {
                // perror("Dosya silinirken bir hata oluştu");
            }
            if (rename(temp_file, server_file_path) != 0) 
            {
                // perror("Dosya adı değiştirilirken bir hata oluştu");
            } 
            else 
            {
                // printf("Dosya adı başarıyla değiştirildi.\n");
            }
            
        }
        else
        {
            strcpy(response, "Failled");
            sprintf(log_buffer, "Writing spesific line is failled.\n*");
            write_to_log_file(log_buffer, log_fd);
        }
        if (bytesRead == 0) //End of the file, search is done
        {
            sprintf(log_buffer, "Line could not found.\n*");
            write_to_log_file(log_buffer, log_fd);
        }
        else if (bytesRead == -1) // error while reading file
        {
            perror("reading file");
            sprintf(log_buffer, "Writing file is failled.\n*");
            write_to_log_file(log_buffer, log_fd);
        }
    }
    else
    {
        perror("open file");
        strcpy(response, "Writing file is failled.");
        sprintf(log_buffer, "Writing file is failled.\n*");
        write_to_log_file(log_buffer, log_fd);
    }
    
    // Dosyayı kapat
    close(fd);
    close(temp_fd);
    write_to_fifo(write_fifo_fd, response);
    exit_critical_region(file_name, file_index);
}

void write_to_end_of_file(char response[RESPONSE_SIZE], char* file_name, char* text, int write_fifo_fd, char* dirname)
{
    int file_index;
    file_index = enter_critical_region(file_name);

    char server_file_path[50];

    sprintf(server_file_path, "./%s/%s", dirname, file_name);

    int fd = open(server_file_path, O_WRONLY | O_APPEND | O_CREAT);

    if (fd == -1)
    {
        perror("open");
        strcpy(response, "Failled");
        return;
    }
    if (access(server_file_path, F_OK) != -1)
    {
        if(write(fd, "\n", 1) == -1)
        {
            perror("write error");
            strcpy(response, "Failled");
        }
    }
    if(write(fd, text, strlen(text)) == -1)
    {
        perror("write error");
        strcpy(response, "Failled");
    }
    else
    {
        strcpy(response, "Successful");
    }
    // Dosyayı kapat
    close(fd);
    write_to_fifo(write_fifo_fd, response);
    // pthread_mutex_unlock(&(files_data[file_index].mutex));
    exit_critical_region(file_name, file_index);

}

void upload_from_client_to_server(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, pid_t client_pid, char* dir_name) //response, filename, fifo
{
    char client_file_path[50];
    char server_file_path[50];

    sprintf(client_file_path, "./CLIENT_%d/%s", client_pid, file_name);
    sprintf(server_file_path, "./%s/%s", dir_name, file_name);

    copy_file(response, file_name, write_fifo_fd, client_file_path, server_file_path, 0);
}

void download_from_server_to_client(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, pid_t client_pid, char* dir_name, int arch) //response, filename, fifo
{
    char client_file_path[50];
    char server_file_path[50];

    sprintf(client_file_path, "./CLIENT_%d/%s", client_pid, file_name);
    if(arch == 0)
    {
        sprintf(server_file_path, "./%s/%s", dir_name, file_name);
        copy_file(response, file_name, write_fifo_fd, server_file_path, client_file_path, 0);
    }
    if(arch == 1)
    {
        copy_file(response, file_name, write_fifo_fd, file_name, client_file_path, 1);
    }
}

int copy_file(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, char* src, char* dest, int arch) //response, filename, fifo
{
    if(check_file_existence(dest))
    {
        strcpy(response, "File name is already exist.");
        sprintf(log_buffer, "File name is already exist.\n*");
        write_to_log_file(log_buffer, log_fd);
    }
    else if (check_file_existence(src))
    {
        int client_file_fd, server_file_fd, bytesRead, count = 0;
        char currentChar;

        client_file_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        server_file_fd = open(src, O_RDONLY);

        int client_file_index = enter_critical_region(dest);
        int server_file_index = enter_critical_region(src);
        
        if(server_file_fd == -1)
        {
            perror("opening server");
            strcpy(response, "Error openning server file.");
        }
        if(client_file_fd == -1)
        {
            perror("opening client");
            strcpy(response, "Error openning client file.");
        }
        else if(client_file_fd != -1 && server_file_fd != -1)
        {
            while ((bytesRead = read(server_file_fd, &currentChar, 1)) > 0) 
            {
                if(write(client_file_fd, &currentChar, 1) == -1)
                {
                    perror("write error");
                    strcpy(response, "Failled while uploading");
                    remove(src);
                }
                count++;
                //printf("count: %d\n", count);
            }
            if(bytesRead == -1)
            {
                perror("write error while writing from client to server");
                strcpy(response, "Failled while uploading");
            }
            else if(bytesRead == 0)
            {
                sprintf(response, "File operation successfully done, %d bytes are transferred.", count);
            }
        }
        close(client_file_fd);
        close(server_file_fd);

        exit_critical_region(dest, client_file_index);

        exit_critical_region(src, server_file_index);

    }
    else
    {
        strcpy(response, "File name is not exist");
    }
    strcpy(log_buffer, response);
    strcat(log_buffer, "*");

    write_to_log_file(log_buffer, log_fd);;
    if(arch == 0)
        write_to_fifo(write_fifo_fd, response);
    if(arch == 1)
        remove(file_name);
}

int check_file_existence(const char *filepath) 
{
    struct stat buffer;
    return (stat(filepath, &buffer) == 0);
}

void archive_server(char response[RESPONSE_SIZE], char* file_name, int write_fifo_fd, char* dir_name)
{
    pid_t child_pid;
    int status;
    char server_file_path[50];

    sprintf(server_file_path, "/%s/%s", dir_name,file_name);
    child_pid = fork();

    if (child_pid == 0) 
    { // Child process
        // Execute tar command to compress files
        //execl("/bin/tar", "tar", "-C", server_file_path, "-cvf", file_name, dir_name, NULL);
        execlp("tar", "tar", "-cvf", file_name, dir_name, NULL);
        // execlp("tar", "tar", "-cvf", "/path/to/target_folder/dosyalar.tar", "klasor/", NULL);
        //execl("/bin/tar", "tar", "-C", "/path/to/your/directory", "-cvf", "files.tar", "folder_name", NULL);
        //execl("/bin/tar", "tar", "-cvf", "/path/to/your/directory/files.tar", "-C", "/path/to/your/directory/folder_name", ".", NULL);
        // execl fonksiyonu sadece belirtilen komutu çalıştırabilir, başka bir işlemi devam ettiremez.
        // Dolayısıyla, buraya sadece hata durumunda çalışacak kodlar konabilir.
        strcpy(response, "Error, archive operation failled.");
        perror("execl failed");
        exit(EXIT_FAILURE);
    } 
    else if (child_pid < 0) 
    { // Fork failed
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    else 
    { // Parent process
        // Wait for child process to finish
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status)) 
        {
            printf("Child process exited with status %d\n", WEXITSTATUS(status));
            strcpy(response, "Archive operation is successful. ");
        } 
        else 
        {
            printf("Child process exited abnormally\n");
            memset(response, 0, RESPONSE_SIZE);
            strcpy(response, "Error, archive operation failled.");
        }
        //download_from_server_to_client(respo)
        write_to_fifo(write_fifo_fd, response);   
    }
}

void remove_pid(pid_t pid, int max_clients)
{
    int i;
        //printf("signal içinde number_of_clients: %d\n", number_of_clients);

    for(i=0; i<max_clients; i++)
    {
        //printf("remove pid %d, client_pid %d Remaining number of clients is %d\n", pid, children_pids[i] ,number_of_clients);
        if(children_pids[i] == pid)
        {
            children_pids[i] = -1;
            clients_pids[i] = -1;
            //printf("pid %d is removed\nRemaining number of clients is %d\n", pid, number_of_clients);
            break;
        }
    }
}

int addClient(pid_t child_pid, pid_t client_pid)
{
    int i;

    for(i=0; i<max_number_of_clients; i++)
    {
        if(children_pids[i] == -1)
        {
            children_pids[i] = child_pid;
            clients_pids[i] = client_pid;
            return i;
        }
    }
}
//It writes response to the given fifo df
void write_to_fifo(int write_fifo_fd, char response[RESPONSE_SIZE])
{
    // printf("fifodan önce response: %s\n", response);
    if(write(write_fifo_fd, response, RESPONSE_SIZE) == -1)
    {
        perror("write list to the client fifo");
        exit(EXIT_FAILURE);
    }
    // printf("fifodan sonra response: %s\n", response);

}

void killServer()
{
    int i;
    destroyQueue(client_queue);
    for (i = 0; i < max_number_of_clients; i++) 
    {
        //printf("**id%d: %d\n", i+1, children_pids[i]);
        if ((children_pids[i] != -1)) 
        { // Geçerli bir PID varsa
            //printf("öldürülen id: %d\n", children_pids[i]);
            //printf("öldürülen client id: %d\n", clients_pids[i]);
            kill(children_pids[i], SIGTERM); // Çocuk işlemi öldür ama client değil fork
        }
        if ((clients_pids[i] != -1)) 
        {
            kill(clients_pids[i], SIGTERM);
        }
    }
    //destroy_client_array();
}

void init_client_array()
{
    int i;
    children_pids = (int *)malloc(max_number_of_clients*sizeof(int));
    clients_pids = (int *)malloc(max_number_of_clients*sizeof(int));
    
    for(i=0; i<max_number_of_clients; i++)
    {
        children_pids[i] = -1;
        clients_pids[i] = -1;  
    }
}

void destroy_client_array()
{
    free(children_pids);
    free(clients_pids);
    children_pids = NULL;
    clients_pids = NULL;
}

char *concatenateStrings(char **strArray, int size, int limit)
{
    int totalLength = 0, i;
    for (i = limit + 1; i < size; i++) 
    {
        totalLength += strlen(strArray[i]);
    }

    char *combinedString = (char *)malloc(totalLength + size-limit); // Ek boşluklar için numStrings - 1 ekleyin
    combinedString[0] = '\0';

     for (int i = limit+1; i < size; i++) 
     {
        strcat(combinedString, strArray[i]);
        if (i < size - 1) 
        {
            strcat(combinedString, " "); // Son elemandan sonra boşluk eklemeyin
        }
    }
    return combinedString;
}

struct Node* newNode(int data)
{
    struct Node* temp = (struct Node*)malloc(sizeof(struct Node));
    temp->data = data;
    temp->next = NULL;
    return temp;
}

// Creates new queue
struct Queue* createQueue()
{
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

// Adding element (enqueue)
void enqueue(struct Queue* q, int data)
{
    struct Node* temp = newNode(data);

    q->size = q->size + 1;
    if (q->rear == NULL) {
        q->front = q->rear = temp;
        return;
    }
    q->rear->next = temp;
    q->rear = temp;
}

// Removing (dequeue)
int dequeue(struct Queue* q)
{
    if (q->front == NULL)
        return -1; // Kuyruk boşsa -1 döndür

    struct Node* temp = q->front;
    int data = temp->data;

    q->front = q->front->next;

    if (q->front == NULL)
        q->rear = NULL;

    q->size = q->size -1;

    free(temp);
    return data;
}

int isEmpty(struct Queue* q) {
    return (q->front == NULL);
}

// Kuyruğu yok etme (destroyQueue)
void destroyQueue(struct Queue* q) 
{
    struct Node* current = q->front;
    while (current != NULL) 
    {
        struct Node* next = current->next;
        kill(current->data, SIGTERM); // Çocuk işlemi öldür ama client değil fork
        free(current);
        current = next;
    }
    q->front = q->rear = NULL; // Kuyruğun başını ve sonunu NULL olarak ayarla
    free(q); // Kuyruk yapısını serbest bırak
}

int tokenize(char *words[MAX_WORDS], char* buffer)
{
    char *token = strtok(buffer, " \t\n");
    int word_count = 0;
    while (token != NULL) 
    {
        // Store the word in the words array
        words[word_count] = strdup(token); //BUNU SONRA FREELER
        if (words[word_count] == NULL) 
        {
            perror("Error allocating memory");
            exit(EXIT_FAILURE);
        }
        (word_count)++;
        // Get the next token
        token = strtok(NULL, " \t\n");
    }
    return word_count;
}

void destroy_words(char *words[MAX_WORDS], int word_count)
{
    int i;
    for(i=0; i<word_count; i++)
    {
        free(words[i]);
    }
}
//It opens write fifo and return the value
int open_write_fifo(pid_t client_pid)
{
    int write_fifo_fd;
    char client_fifo_write[50];
    
    sprintf(client_fifo_write, "CLIENT_FIFO_WRITE_%d", client_pid);
    sprintf(log_buffer, "%s FIFO is opened\n*", client_fifo_write);
    //write_to_log_file(log_buffer, log_fd);
    while(((write_fifo_fd = open(client_fifo_write, O_RDWR, 0666)) == -1) && ((errno == EINTR) || (errno == ENOENT)));
    if (write_fifo_fd == -1) 
    {
        sprintf(log_buffer, "Error while opening FIFO %s\n*", client_fifo_write);
        write_to_log_file(log_buffer, log_fd);
        if(errno == ENOENT)
        {
            perror("open serverda yazma");

        }
        perror("open serverda yazma");
        exit(EXIT_FAILURE);
    }
    return write_fifo_fd;
}
//It opens read fifo and return the value
int open_read_fifo(pid_t client_pid)
{
    int read_fifo_fd;
    char client_fifo_read[50];

    sprintf(client_fifo_read, "CLIENT_FIFO_READ_%d", client_pid);
    sprintf(log_buffer, "%s FIFO is opened\n*", client_fifo_read);
    write_to_log_file(log_buffer, log_fd);
    while(((read_fifo_fd = open(client_fifo_read, O_RDWR, 0666)) == -1) && ((errno == EINTR) || (errno == ENOENT)));
    if (read_fifo_fd == -1) 
    {
        sprintf(log_buffer, "Error while opening FIFO %s.\n*", client_fifo_read);
        write_to_log_file(log_buffer, log_fd);
        if(errno == ENOENT)
        {
            perror("open");

        }
        perror("open");
        exit(EXIT_FAILURE);
    }
    return read_fifo_fd;
}
//Initializes shared array elements with dummy values
void init_files_data()
{
    int i;
    FileData temp;
    strcpy(temp.file_name, "");
    init_shared_mutex(&(temp.mutex));
    for(i=1; i<NUM_OF_FILES; i++)
    {
        files_data[i] = temp;
    }
    strcpy(temp.file_name, LOG_FILE_NAME);
    files_data[0] = temp;
}
//It destroys mutexes
void destroy_mutex()
{
    int i;
    for(i=0; i< NUM_OF_FILES; i++)
    {
        pthread_mutex_destroy(&files_data[i].mutex);
    }
}
//It initializes mutexes
void init_shared_mutex(pthread_mutex_t *mutex) 
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(mutex, &attr);
}
//It checks if the giben filename is in the array
int is_file_in_shared_memory(char* file_name)
{
    int i;
    for(i=0; i<NUM_OF_FILES; i++)
    {
        if(strcmp(files_data[i].file_name, "") != 0 && strcmp(files_data[i].file_name, file_name) == 0)
        {
            return i;
        }
    }
    return -1;
}
//It adds a file to the shared array
int add_file_to_shared_memory(char* file_name)
{
    int i;
    for(i=0; i<NUM_OF_FILES; i++)
    {
        // printf("Bööööö\n");
        if(strcmp(files_data[i].file_name, "") == 0)
        {
            strcpy(files_data[i].file_name, file_name);
            return i;
        }
    }
    return -1;
}
//It checks shared array, if the file is not in the array it adds and locks
int enter_critical_region(char* file_name)
{
    int file_index;
    
    if((file_index = is_file_in_shared_memory(file_name)) == -1) // yoksa ekle
    {
        file_index = add_file_to_shared_memory(file_name);
    }
    
    pthread_mutex_lock(&(files_data[file_index].mutex));
    return file_index;
}
//It unlock mutex
void exit_critical_region(char* file_name, int file_index)
{
    pthread_mutex_unlock(&files_data[file_index].mutex);
}
//It writes to the log file according to critical regions.
void write_to_log_file(char log_buffer[BUFFER_SIZE], int log_fd)
{
    int file_index, i=0;
    char currentChar;
    file_index = enter_critical_region(LOG_FILE_NAME);
    while((currentChar = log_buffer[i]) != '*')
    {
        if(write(log_fd, &currentChar, 1) == -1)
        {
            perror("write error");
        }
        i++;
    }
    exit_critical_region(LOG_FILE_NAME, file_index);;
}
