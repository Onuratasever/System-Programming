#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define FIFO_PATH "FIFO"
#define BUFFER_SIZE 128
#define REQUEST_SIZE 128
#define RESPONSE_SIZE 1024
#define MAX_WORDS 5
#define CONNECTION_RESPONSE 20

char client_fifo_write[50];
char client_fifo_read[50];
char *command_tokens[MAX_WORDS];
int command_token_count=0;

void free_command_tokens(char *command_tokens[], int count) 
{
    if (command_tokens == NULL) 
    {
        return; // Nothing to free, invalid pointer array
    }
    for (int i = 0; i < count; i++) 
    {
        if (command_tokens[i] != NULL) {
            free(command_tokens[i]); // Free the allocated memory
            command_tokens[i] = NULL; // Set the pointer to NULL to prevent double free
        }
    }
}

void communication(int write_fifo_fd, int read_fifo_fd, char command_temp[BUFFER_SIZE], char response[RESPONSE_SIZE])
{
    if(write(write_fifo_fd, command_temp, REQUEST_SIZE) == -1)
    {
        perror("Failed to send list request to server");
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        exit(1);
    }
    memset(response, 0, RESPONSE_SIZE);
    if(read(read_fifo_fd, response, RESPONSE_SIZE) == -1)
    {
        perror("Failed to get list response from server");
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        exit(1);
    }
    //free_command_tokens(command_tokens, command_token_count);
}



int main(int argc, char *argv[])
{
    char log_buffer[BUFFER_SIZE];
    char connection_request[256];
    char connection_response[CONNECTION_RESPONSE];
    char client_directory_name[50];
    char response[RESPONSE_SIZE];
    int server_fd, write_fifo_fd, read_fifo_fd;
    

    if (argc != 3) 
    {
        printf("Usage: %s <Connect/tryConnect> <ServerPID>\n", argv[0]);
        exit(1);
    }

    pid_t server_pid = atoi(argv[2]);
    //Checks if desired server exist or not

    sprintf(client_fifo_write, "CLIENT_FIFO_WRITE_%d", getpid());
    sprintf(client_fifo_read, "CLIENT_FIFO_READ_%d", getpid());
    if(mkfifo(client_fifo_read, 0666) == -1)
    {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if(mkfifo(client_fifo_write, 0666) == -1)
    {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if(kill(server_pid, 0) == 0)
    {
        // Process exists
        //printf("Process with PID %d exists.\n", server_pid);
    } 
    else 
    {
        // Process does not exist or permission denied
        printf("Process with PID %d does not exist or permission denied.\n", server_pid);
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        exit(EXIT_FAILURE);
    }

    while(((server_fd = open(FIFO_PATH, O_WRONLY, 0666)) == -1) && (errno == EINTR));
    if (server_fd == -1) 
    {
        perror("open server");
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        exit(EXIT_FAILURE);
    }
    sprintf(connection_request, "%s %d", argv[1], getpid());

    //Sends connection request
    if(write(server_fd, connection_request, BUFFER_SIZE) == -1)
    {
        perror("Failed to send connection request to server");
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        exit(1);
    }

    while(((write_fifo_fd = open(client_fifo_read, O_RDWR, 0666)) == -1) && ((errno == EINTR) || (errno == ENOENT)))
    {
        if(errno == ENOENT)
        {
            // printf("no ent: %s\n", client_fifo_read);
        }
        else
        {
            // printf("other: %d\n", errno);
        }
        fflush(stdout);

    }
    if (write_fifo_fd == -1) 
    {
        if(errno == ENOENT)
        {
            perror("open");

        }
        perror("open server writing");
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        exit(EXIT_FAILURE);
    }
    //It tries to open file as long as it is interrupted or it is not exist (it waits fifo creation)
    while(((read_fifo_fd = open(client_fifo_write, O_RDWR, 0666)) == -1) && ((errno == EINTR) || (errno == ENOENT)));

    if (read_fifo_fd == -1) 
    {
        if(errno == ENOENT)
        {
            perror("open serverdan okuma");

        }
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        exit(EXIT_FAILURE);
    }
    sprintf(client_directory_name, "CLIENT_%d", getpid());
    
    struct stat st;
    if (stat(client_directory_name, &st) == -1) 
    {
        if (mkdir(client_directory_name, 0777) == -1) 
        {
            perror("Directory is exist.");
        }
    }

    do
    {
        if(read(read_fifo_fd, response, RESPONSE_SIZE) == -1)
        {
            perror("Failed to get list response from server");
            exit(1);
        }
        printf("serverdan cevap: %s ve bağlanma tipi: %s\n", response, argv[1]);
    } while ((strcmp(response, "full") == 0) && strcmp(argv[1], "connect") == 0);

    if((strcmp(response, "full") == 0) && strcmp(argv[1], "tryConnect") == 0)
    {
        unlink(client_fifo_read);
        unlink(client_fifo_write);
        printf("Server is full. Leaving..\n");
        exit(EXIT_SUCCESS);
    }
    printf("Waiting for Que.. Connection established\n");
    //commands from user
    while(1)
    {
        char command[BUFFER_SIZE];
        char command_temp[BUFFER_SIZE];
        command_token_count = 0;
        printf(">> Enter command: ");
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) 
        {
            // Error handling for input failure
            perror("fgets");
            unlink(client_fifo_read);
            unlink(client_fifo_write);
            exit(EXIT_FAILURE);
        }
        command[strcspn(command, "\n")] = '\0';
        strcpy(command_temp, command);
        char *token = strtok(command, " \t\n");
        while (token != NULL) 
        {
            // Store the word in the words array
            command_tokens[command_token_count] = strdup(token); 
            if (command_tokens[command_token_count] == NULL) 
            {
                perror("Error allocating memory");
                unlink(client_fifo_read);
                unlink(client_fifo_write);
                free_command_tokens(command_tokens, command_token_count);
                exit(EXIT_FAILURE);
            }
            (command_token_count)++;
            // Get the next token
            token = strtok(NULL, " \t\n");
        }

        if((strcmp(command_tokens[0], "help") == 0) )
        {
            if(command_token_count == 1)
                printf("Client Commands:\n-help\n-list\n-readF\n-writeT\n-upload\n-download\n-archServer\n-killServer\n-quit\n");
            else if((strcmp(command_tokens[1], "readF") == 0) && command_token_count == 2)
                printf("readF <file> <line #>\nrequests to display the # line of the <file>, if no line number is given the whole contents of the file is requested \n");
            else if((strcmp(command_tokens[1], "writeT") == 0) && command_token_count == 2)
                printf("writeT <file> <line #> <string>\nrequest to write the  content of “string” to the  #th  line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time\n");
            else if((strcmp(command_tokens[1], "list") == 0) && command_token_count == 2)
                printf("sends a request to display the list of files in Servers directory\n");
            else if((strcmp(command_tokens[1], "upload") == 0) && command_token_count == 2)
                printf(" upload <file>\n uploads the file from the current working directory of client to the Servers directory\n");
            else if((strcmp(command_tokens[1], "download") == 0) && command_token_count == 2)
                printf(" download <file>\nrequest to receive <file> from Servers directory to client side\n");
            else if((strcmp(command_tokens[1], "archServer") == 0) && command_token_count == 2)
                printf(" archServer <fileName>.tar\ncollect all the files currently available on the the  Server side and store them in the <filename>.tar archive\n");
            else if((strcmp(command_tokens[1], "killServer") == 0) && command_token_count == 2)
                printf("Sends a kill request to the Server");
            else if((strcmp(command_tokens[1], "quit") == 0) && command_token_count == 2)
                printf("Sends a request to disconnect");

        }
        else if((strcmp(command, "quit") == 0) && command_token_count == 1)
        {
            printf(" Sending write request to server log file\nwaiting for logfile ...\nlogfile write request granted\nbye..\n");
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);
            printf("%s\n", response);
            break;
        }
        else if((strcmp(command_tokens[0], "list") == 0) && command_token_count == 1)
        {
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);

            printf("Files in server directory:\n%s", response);
        }
        else if((strcmp(command_tokens[0], "readF") == 0) && (command_token_count == 3 || command_token_count == 2))
        {
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);

            printf("%s\n", response);
        }
        else if((strcmp(command_tokens[0], "writeT") == 0))
        {
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);

            printf("%s\n", response);
        }
        else if((strcmp(command_tokens[0], "upload") == 0) && (command_token_count == 2))
        {
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);

            printf("%s\n", response);
        }
        else if((strcmp(command_tokens[0], "download") == 0) && (command_token_count == 2))
        {
            printf("file transfer request received. Beginning file transfer:\n");
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);

            printf("%s\n", response);
        }
        else if((strcmp(command_tokens[0], "archServer") == 0) && (command_token_count == 2))
        {
            printf(" Archiving the current contents of the server...");
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);

            printf("%s\n", response);
        }
        else if((strcmp(command_tokens[0], "killServer") == 0) && (command_token_count == 1))
        {
            communication(write_fifo_fd, read_fifo_fd, command_temp, response);

            printf("%s\n", response);
            break;
        }
    }

    close(server_fd);
    unlink(client_fifo_read);
    unlink(client_fifo_write);
    free_command_tokens(command_tokens, command_token_count);
    return 0;
}