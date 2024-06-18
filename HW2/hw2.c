#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <bits/sigaction.h>
#include <asm-generic/signal-defs.h>

#define FIFO1_NAME "FIFO1"
#define FIFO2_NAME "FIFO2"

static int counter = 0; // Create process counter

void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        if (WIFEXITED(status)) 
        {
            printf("Child process %d terminated with exit status %d\n", pid, WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status)) 
        {
            printf("Child process %d terminated by signal %d\n", pid, WTERMSIG(status));
        }
		counter++;
    }
}

int main(int argc, char* argv[])
{
    int sizeOfArray, i, fd_fifo1, fd_fifo2, child1_fifo1_fd, child1_fifo2_fd, child2_fifo2_fd;
    int* randomNumbersArray;
    char command[] = "multiply";  /* Create command */
    int command_size = strlen(command);
    pid_t child1, child2;

    //Argumant check
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s \"integer value\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Set signal handler for SIGCHLD */
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
        
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    //Creation of size and memory allocation for array
    sizeOfArray = atoi(argv[1]);
    randomNumbersArray = (int *)malloc(sizeOfArray * sizeof(int));
    srand(time(NULL));

    //Random number are assigned
    for(i=0; i<sizeOfArray; i++)
    {
        randomNumbersArray[i] = rand()%sizeOfArray;
    }

    printf("Random numbers:\n");
    for(i=0; i<sizeOfArray; i++)
    {
        printf("%d ", randomNumbersArray[i]);
    }
    printf("\n");

    //fifo creation and error check
    if(mkfifo(FIFO1_NAME, 0666) == -1)
    {
        //printf("geliyomu\n");
        perror("error mkfifo1 creation");
        exit(EXIT_FAILURE);
    }
    if(mkfifo(FIFO2_NAME, 0666) == -1)
    {
        //printf("geliyomu\n");
        perror("error mkfifo2 creation");
        exit(EXIT_FAILURE);
    }

    //Tries to open fifo file as long as there is signal interrupt
    while(((fd_fifo1 = open(FIFO1_NAME, O_RDWR, 0666)) == -1) && (errno == EINTR));

    if(fd_fifo1 == -1)
    {
        fprintf(stderr, "[%ld]: failed to open named pipe %s for write: %s\n", (long)getpid(), FIFO1_NAME, strerror(errno));
        exit(EXIT_FAILURE);
    }
    // printf("fd: %d \n", fd_fifo1);
    //It prints size of the array to the file
    if (write(fd_fifo1, &sizeOfArray, sizeof(int)) == -1) {
        perror("FIFO1 write error.\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        //printf("Array size'ı başarılı bir şekilde yazıldı\n");
    }

    //It prints -the array to the file
    if (write(fd_fifo1, randomNumbersArray, sizeOfArray * sizeof(int)) == -1) {
        perror("FIFO1 write error.\n");
        exit(EXIT_FAILURE);
    }
    else
    {
       // printf("Array başarılı bir şekilde yazıldı\n");
    }

    //Tries to open fifo file as long as there is signal interrupt
    while(((fd_fifo2 = open(FIFO2_NAME, O_RDWR, 0666)) == -1) && (errno == EINTR));
    if(fd_fifo2 == -1)
    {
        fprintf(stderr, "[%ld]: failed to open named pipe %s for write: %s\n", (long)getpid(), FIFO2_NAME, strerror(errno));
        exit(EXIT_FAILURE);
    }
    //It prints size of command to the file
    if (write(fd_fifo2, &command_size, sizeof(int)) == -1) {
        perror("FIFO2 write error.\n");
        exit(EXIT_FAILURE);
    }
    //It writes the command to the file
    if (write(fd_fifo2, command, strlen(command)*sizeof(char)) == -1) {
        perror("FIFO2 write error.\n");
        exit(EXIT_FAILURE);
    }
    //It prints size of the array to the file
    if (write(fd_fifo2, &sizeOfArray, sizeof(int)) == -1) {
        perror("FIFO2 write error.\n");
        exit(EXIT_FAILURE);
    }
    //It prints -the array to the file
    if (write(fd_fifo2, randomNumbersArray, sizeOfArray * sizeof(int)) == -1) {
        perror("FIFO2 write error.\n");
        exit(EXIT_FAILURE);
    }

   /* Deallocate numbers array and prevent the dangling pointer*/
    free(randomNumbersArray);
    randomNumbersArray = NULL; 

    /*-----------------------------PROCESS CREATION WITH FORK---------------------------*/
    child1 = fork(); // creates a child process
    if (child1 == -1) 
    {
		perror("Error while creating first child process.\n");
        exit(EXIT_FAILURE);
	}
    else if(child1 == 0) // child process
    {
        sleep(10); // Sleep function
        // printf("child1 id: %d\n", getpid());
        int array_size = 0;
		int sum = 0;
        int* numbers;

        //Tries to open fifo file as long as there is signal interrupt
        while(((child1_fifo1_fd = open(FIFO1_NAME, O_RDONLY, 0666)) == -1) && (errno == EINTR));
    	//int child1_fifo1_fd = open(FIFO1_NAME, O_RDONLY, 0666);
        if(child1_fifo1_fd == -1)
        {
            fprintf(stderr, "[%ld]: failed to open named pipe %s for write: %s\n", (long)getpid(), FIFO1_NAME, strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        /* Read array size from FIFO1 */
		if (read(child1_fifo1_fd, &array_size, sizeof(int)) == -1)
		{
			perror("FIFO1 read error\n");
			exit(EXIT_FAILURE);
		}

        //Allocate memory for the numbers
        numbers = (int *)malloc(array_size * sizeof(int));

		/* Read numbers from FIFO1 */
		if (read(child1_fifo1_fd, numbers, array_size * sizeof(int)) == -1)
		{
			perror("FIFO1 read error\n");
			exit(EXIT_FAILURE);
		}

        for(int i = 0; i < array_size; i++) {
			sum += numbers[i];
            //printf("d: %d ", numbers[i]);
		}
        //printf("\n");

        //Tries to open fifo file as long as there is signal interrupt
        while(((child1_fifo2_fd = open(FIFO2_NAME, O_WRONLY, 0666)) == -1) && (errno == EINTR));
    	// child1_fifo2_fd = open(FIFO2_NAME, O_WRONLY, 0666);
        // printf("fifo2 fd: %d\n", child1_fifo2_fd);
        if(child1_fifo2_fd == -1)
        {
            fprintf(stderr, "[%ld]: failed to open named pipe %s for write: %s\n", (long)getpid(), FIFO2_NAME, strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* Write sum of numbers elements to FIFO2 */
		if (write(child1_fifo2_fd, &sum, sizeof(int)) == -1)
		{
			perror("FIFO2 wrdddite error\n");
			exit(EXIT_FAILURE);
		}

        close(child1_fifo1_fd);
        close(child1_fifo2_fd);
        //deallocate numbers array
        free(numbers);
        //prevents dangling pointer
        numbers = NULL;

        //printf("Result of sum operation in first child is: %d\n", sum);
		printf("End of the first child!\n");
        exit(EXIT_SUCCESS);
    }
    else // Parent process
    {
        //Creates a child process
        child2 = fork();

        if(child2 == -1)
        {
            perror("Error while creating first child process.\n");
            exit(EXIT_FAILURE);
        }
        else if (child2 == 0) // Child process
        {
            sleep(10); // Sleeps ten seconds

        // printf("child2 id: %d\n", getpid());

            int command_size2;
            int array_size;
            int multiplication = 1;
            int* numbers;
            int sum;
            char* command_from_file;
            //command size, command, size, array, sum

            while(((child2_fifo2_fd = open(FIFO2_NAME, O_RDONLY, 0666)) == -1) && (errno == EINTR));
            if(child2_fifo2_fd == -1)
            {
                fprintf(stderr, "[%ld]: failed to open named pipe %s for write: %s\n", (long)getpid(), FIFO2_NAME, strerror(errno));
                exit(EXIT_FAILURE);
            }

            // Read command size from FIFO2
            if (read(child2_fifo2_fd, &command_size2, sizeof(int)) == -1)
            {
                perror("FIFO1 read error\n");
                exit(EXIT_FAILURE);
            }

            //Allocate memeory for the command string
            command_from_file = (char *) malloc((command_size2+1)*sizeof(char));

            // Read command from FIFO2
            if (read(child2_fifo2_fd, command_from_file, command_size2* sizeof(char)) == -1)
            {
                perror("FIFO2 read error\n");
                exit(EXIT_FAILURE);
            }
            command_from_file[command_size2] = '\0';
            //printf("command: *%s*\n", command_from_file);

            // Read array size from FIFO2
            if (read(child2_fifo2_fd, &array_size, sizeof(int)) == -1)
            {
                perror("FIFO1 read error\n");
                exit(EXIT_FAILURE);
            }

            //Allocate memory for the number array
            numbers = (int*) malloc(array_size * sizeof(int));

            // Read numbers array from FIFO2
            if (read(child2_fifo2_fd, numbers, array_size* sizeof(int)) == -1)
            {
                perror("FIFO2 read error\n");
                exit(EXIT_FAILURE);
            }

            // Read sum from FIFO2
            if (read(child2_fifo2_fd, &sum, sizeof(int)) == -1)
            {
                perror("FIFO2 read error\n");
                exit(EXIT_FAILURE);
            }

            if(strcmp("multiply", command_from_file) == 0)
            {
				for(int i = 0; i < array_size; i++)
                {
					multiplication = multiplication* numbers[i];
                    //printf("sayi: %d ", numbers[i]);
				}
                //printf("\n");
			}
			else
            {
				perror("Invalid command\n");
				exit(EXIT_FAILURE);
			}

            close(child2_fifo2_fd);

            //Deallocate numbers array and prevent dangling pointer
            free(numbers);
    		numbers = NULL; 

            //Deallocate command array and prevent dangling pointer
			free(command_from_file);
    		command_from_file = NULL; 

            //printf("Result of multiplication operation in second child is: %d\n", multiplication);
			printf("\nSum of the results of first and second child processes: %d + %d = %d\n",sum, multiplication, sum + multiplication);
			printf("End of the second child!\n");
			exit(EXIT_SUCCESS);
        }

        while (1) 
        {
			printf("Proceeding...\n");
			sleep(2);
			// printf("counter = %d\n",counter);
			if (counter == 2) {
                 // Close FIFO1 
				close(fd_fifo1); 
				 // Close FIFO2 
                close(fd_fifo2);
				//unlink fifos
                unlink(FIFO1_NAME);
				unlink(FIFO2_NAME);
				
				return EXIT_SUCCESS;
			}
        }
    }
    return 0;
}