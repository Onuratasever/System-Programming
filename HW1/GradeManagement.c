#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h> // for tolower function
#include <fcntl.h>


#define MAX_NAME_LENGTH 50
#define MAX_GRADE_LENGTH 3
#define MAX_LINE_LENGTH 100
#define MAX_TOKENS 100
#define MAX_LINES 1000

// Function declarations
void createFile(char *filename);
void addStudentGrade(char* commandTokens[], int count);
void searchStudent(char* commandTokens[], int count);
void sortAll(char* commandTokens[], int count);
void showAll(char* commandTokens[], int count);
void listGrades(char* commandTokens[], int count);
void listSome(char* commandTokens[], int count);
void logTaskCompletion(char *task);
void listCommands();
void gtuStudentGrades(char* commandTokens[], int count);

int main(int argc, char *argv[]) 
{
    FILE *logFilePointer;
    char* commandWord;
    char* fileWords;
    char* commandTokens[MAX_TOKENS];
    char* fileTokens[MAX_TOKENS];
    char command[100];
    char line[MAX_LINE_LENGTH];
    char logging[200];
    int count;
    int logFileDescriptor = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (logFileDescriptor != -1) {
        if (close(logFileDescriptor) == -1) {
            perror("Error closing file");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Error creating file!\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Enter command: ");
        fgets(command, sizeof(command), stdin); //Get command input from user
        command[strcspn(command, "\n")] = '\0';  // Remove newline character
        count = 0;
        commandWord = strtok(command, " ");

        while(commandWord != NULL && count < MAX_TOKENS) // tokenize 
        {   
            commandTokens[count] = commandWord;
            count++;
            commandWord = strtok(NULL, " ");
        }

        pid_t pid = fork(); // creates new process

        if (pid == -1) {
            perror("fork");
            exit(1);
        }
        else if(pid == 0)
        {
             // Child process
            // printf("This is the child process. PID: %d\n", getpid());
            // printf("Parent PID: %d\n", getppid()); // Get parent process ID
            if(strcmp(commandTokens[0], "gtuStudentGrades") == 0 && count == 1) //gtuStudentGrades lists all available commands
            {
                listCommands();
            }
            else if(strcmp(commandTokens[0], "gtuStudentGrades") == 0 && count == 2) //gtuStudentGrades opens specified file
            {
                gtuStudentGrades(commandTokens, count);
            }
            else if(strcmp(commandTokens[0], "gtuStudentGrades") == 0 && count > 2) //gtuStudentGrades too many arguments error
            {
                fprintf(stderr, "Too many arguments!\n");
                sprintf(logging, "%s: too many arguments!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "addStudentGrade") == 0 && count > 4) //add name surname and grade to the file 
            {
                addStudentGrade(commandTokens, count);
            }
            else if(strcmp(commandTokens[0], "addStudentGrade") == 0 && count <= 4) //Missing arguments
            {
                fprintf(stderr, "Missing arguments!\n");
                sprintf(logging, "%s: Missing arguments!\n", command);
                logTaskCompletion(logging);

            }
            else if(strcmp(commandTokens[0], "searchStudent") == 0 && count >= 4) // search for a student's grade by enterng the student's name. return student name surname and grade.
            {
                searchStudent(commandTokens, count);
            }
            else if(strcmp(commandTokens[0], "searchStudent") == 0 && count < 4) //Missing arguments
            {
                fprintf(stderr, "Missing arguments!\n");
                sprintf(logging, "%s: Missing arguments!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "sortAll") == 0 && count == 2) // Sort
            {
                sortAll(commandTokens, count);
            }
            else if(strcmp(commandTokens[0], "sortAll") == 0 && count < 2) // Missing Arguments
            {
                fprintf(stderr, "Missing arguments!\n");
                sprintf(logging, "%s: Missing arguments!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "sortAll") == 0 && count > 2) // Too many Arguments
            {
                fprintf(stderr, "Too many arguments!\n");
                sprintf(logging, "%s: too many arguments!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "showAll") == 0 && count == 2) //Everything in the file will be printed
            {
                showAll(commandTokens, count);
            }
            else if(strcmp(commandTokens[0], "showAll") == 0 && count == 1) //Error file name is neceassary!
            {
                fprintf(stderr, "File Name is necessary!\n");
                sprintf(logging, "%s: Missing arguments, File name is required!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "showAll") == 0 && count > 2) //Error too many arguments!
            {
                fprintf(stderr, "Too many arguments!\n");
                sprintf(logging, "%s: too many arguments!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "lstGrades") == 0 && count == 2) //Show first 5 entries
            {
                listGrades(commandTokens, count);
            }
            else if(strcmp(commandTokens[0], "lstGrades") == 0 && count == 1) //Error file name is necessary!
            {
                fprintf(stderr, "File Name is necessary!\n");
                sprintf(logging, "%s: Missing arguments, File name is required!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "lstGrades") == 0 && count > 2) //Error too many arguments!
            {
                fprintf(stderr, "Too many arguments!\n");
                sprintf(logging, "%s: too many arguments!\n", command);
                logTaskCompletion(logging);
            }
            else if(strcmp(commandTokens[0], "lstSome") == 0 && count == 4) //lstSome 5 2 grades.txt command wll l st entres between 5th and 10th.
            {
                listSome(commandTokens, count);
            }
            else if(strcmp(commandTokens[0], "lstSome") == 0 && count != 4) //Error wrong input number.
            {
                fprintf(stderr, "Please try like: \"lstSome 'numofEntries' 'pageNumber' 'grades.txt'\" \n");
                sprintf(logging, "%s: Wrong argument number!\n", command);
                logTaskCompletion(logging);
            }
            else
            {
                fprintf(stderr, "Command not found!\n");
                sprintf(logging, "%s command not found!\n", command);
                logTaskCompletion(logging);
            }
            exit(0);
        }
        else
        {
            // Parent process
            // printf("This is the parent process. PID: %d\n", getpid());
            // printf("Child PID: %d\n", pid); // Get child process ID
            wait(NULL);
        }
    }
    return 0;
}

//lists all the commands and informations
void listCommands()
{
    printf(
        "\ngtuStudentGrades\n(1) \n-Without arguments it displays all commands.\nExample Usage: gtuStudentGrades\n(2) \n-With file name parameter if the file is not exist, it creates file.\nExample Usage: gtuStudentGrades \"grades.txt\"\n\naddStudentGrade\n-It adds students name, surname and grade to the file.\nExample Usage: addStudentGrade \"Name Surname\" \"AA\" \"grades.txt\"\n"
    );
    printf("\nsearchStudent\n-By entering student name, student and grade can be shown, if it is exist.\nExample Usage: searchStudent \"Name Surname\" \"grades.txt\"\n");
    printf("\nsortAll\n-It sorts  by student name or grade in ascendng or descendng order. \nExample Usage: sortAll \"gradest.txt\"\n");
    printf("\nshowAll\n-It display all the student with grades.\nExample Usage: showAll \"grades.txt\"\n");
    printf("\nlstGrades\n-It displays first 5 students with grades.\nExample Usage: lstGrades \"grades.txt\"\n");
    printf("\nlstSome\n-It displays the students withs grades which are between arguments.\nExample Usage: lstSome 5 2 \"grades.txt\"\nExample: lstSome 5 2 \"grades.txt\" command will list entries between 5th and 10th.\n");

    logTaskCompletion("By using gtuStudentGrades, commands were listed.\n");
}

void FileOpenSuccessMessage(char* commandTokens[], int count)
{
    char logging[100];
    sprintf(logging, "%s named file is opened!\n", commandTokens[count-1]);
    logTaskCompletion(logging);
}

void FileCloseSuccessMessage(char* commandTokens[], int count)
{
    char logging[100];
    sprintf(logging, "%s named file is closed!\n", commandTokens[count-1]);
    logTaskCompletion(logging);
}

void NullFileMessage(char* commandTokens[], int count)
{
    char logging[100];
    sprintf(logging, "%s: Error opening file, possibly null returned!\n", commandTokens[count-1]);
    logTaskCompletion(logging);
}

void ErrorReadingFileMessage(char* commandTokens[], int count)
{
    char logging[100];
    sprintf(logging, "%s: Error reading file!\n", commandTokens[count-1]);
    logTaskCompletion(logging);
}

//add student full name and grade to the file. It add space between each word and to the end of the line it adds '\n'
void addStudentGrade(char* commandTokens[], int count) 
{
    int fileDescriptor = open(commandTokens[count - 1], O_WRONLY | O_APPEND, 0666); //opens file if it is exist

    int i;
    char logging[100];
    ssize_t bytes_written;
    if (fileDescriptor != -1)
    {
        FileOpenSuccessMessage(commandTokens, count);
        for(i=1; i<=count-3; i++) // It will write full name with spaces
        {
            bytes_written = write(fileDescriptor, commandTokens[i], strlen(commandTokens[i]));
            if (bytes_written == -1) {
                perror("Error writing to log file");
                close(fileDescriptor);
                exit(EXIT_FAILURE);
            }

            if (i <= count - 3) {
                bytes_written = write(fileDescriptor, " ", 1);
                if (bytes_written == -1) {
                    perror("Error writing to file");
                    close(fileDescriptor);
                    exit(EXIT_FAILURE);
                }
            }
        }

        bytes_written = write(fileDescriptor, commandTokens[i], strlen(commandTokens[i]));
        if (bytes_written == -1) {
            perror("Error writing to log file");
            close(fileDescriptor);
            exit(EXIT_FAILURE);
        }

        bytes_written = write(fileDescriptor, "\n", 1);
        if (bytes_written == -1) {
            perror("Error writing to file");
            close(fileDescriptor);
            exit(EXIT_FAILURE);
        }

        //logging
        logTaskCompletion("Student and grade is added to the file.\n");
        printf("Student and grade is added to the file.\n");
        FileCloseSuccessMessage(commandTokens, count);
        close(fileDescriptor);
    }
    else
    {
        NullFileMessage(commandTokens, count);
        printf("Error is occured while file is opening\n");
        //logging
    }
}

//It searchs file line by line and tries to catch a match
void searchStudent(char* commandTokens[], int count) 
{
    int fd = open(commandTokens[count - 1], O_RDONLY); // opens file to read
    char line[MAX_LINE_LENGTH];
    char* fileWords;
    char* fileTokens[MAX_TOKENS];
    char logging[100];
    int isEqual = 1;
    int isExist = 0;
    int bytesRead;
    char currentChar;
    int lineIndex = 0;

    if (fd != -1)
    {
        FileOpenSuccessMessage(commandTokens, count);
        int countFileWords;
        while ((bytesRead = read(fd, &currentChar, 1)) > 0) 
        {
            if(currentChar == '\n' || lineIndex == MAX_LINE_LENGTH - 1) //bir line'ın bittiğini anlar ve line üstünde işlem yapar
            {
                line[lineIndex] = '\0';
                lineIndex = 0;
                //line üstünde işlemler

                isEqual = 1;
                line[strcspn(line, "\n")] = '\0';  // Remove newline character
                countFileWords = 0;
                fileWords = strtok(line, " ");

                while(fileWords != NULL && count < MAX_TOKENS)
                {   
                    fileTokens[countFileWords] = fileWords;
                    countFileWords++;
                    fileWords = strtok(NULL, " ");
                }
                for (int i = 0; i < countFileWords - 1 && isEqual == 1; i++) // compares tokens
                {
                    if((count-2 != countFileWords - 1))
                    {
                        isEqual = 0;
                    }
                    else if( strcmp(commandTokens[i+1], fileTokens[i]) != 0)
                    {
                        isEqual = 0;
                    }
                    
                }
                if(isEqual == 1) // Match condition
                {
                    for (int i = 0; i < countFileWords; i++) 
                    {
                        printf("%s ", fileTokens[i]);
                    }
                    printf("\n");
                    isExist = 1;
                }
            }
            else //line'ı oluşturmaya devam eder.
            {
                line[lineIndex] = currentChar;
                lineIndex++;
            }
            
        }
        if(isExist == 0)// No match
        {
            printf ("Student was not found!\n");
        }
        if (bytesRead == 0) //End of the file, search is done
        {
            sprintf(logging, "%s: Search operation completed successfully!\n", commandTokens[count-1]);
            logTaskCompletion(logging);
        }
        else if (bytesRead == -1) // error while reading file
        {
            ErrorReadingFileMessage(commandTokens, count);
        }
        
        //logging
        close(fd);
        FileCloseSuccessMessage(commandTokens, count);
    }
    else
    {
        NullFileMessage(commandTokens, count);
        printf("Error is occured while file is opening\n");
        //loglama
    }
}

int compareStringsForName(const void *a, const void *b) {
    char *const *pp1 = a;
    char *const *pp2 = b;
    char str1_lower[100], str2_lower[100];
    
    strcpy(str1_lower, *pp1);
    strcpy(str2_lower, *pp2);
    
    for (int i = 0; str1_lower[i]; i++) {
        str1_lower[i] = tolower(str1_lower[i]);
    }
    for (int i = 0; str2_lower[i]; i++) {
        str2_lower[i] = tolower(str2_lower[i]);
    }
    
    return strcmp(str1_lower, str2_lower);
}

int compareStringsForGrade(const void *a, const void *b) {
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    if (len1 < 2 || len2 < 2) {
        // If either string is too short, consider it greater
        return len1 - len2;
    }
    // Compare the last two characters
    return strcmp(str1 + len1 - 2, str2 + len2 - 2);
}

void reverseArray(char *arr[], int size) 
{
    int start = 0;
    int end = size - 1;

    while (start < end) {
        // Swap elements at start and end indices
        char *temp = arr[start];
        arr[start] = arr[end];
        arr[end] = temp;

        // Move start index forward and end index backward
        start++;
        end--;
    }
}

void sortAll(char* commandTokens[], int count) 
{
    int fd = open(commandTokens[count - 1], O_RDONLY);
    
    ssize_t bytes_Read;
    int numLines = 0;
    
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    char *sortedLines[MAX_LINES];
    char logging[100];
	char option[10];

    int bytesRead;
    char currentChar;
    int lineIndex = 0;

    
    if (fd != -1)
    {
        printf("\n------------------- Sort Options -------------------\n");
        printf("1. Sort according to student full name in ascending order\n");
        printf("2. Sort according to student full name in descending order\n");
        printf("3. Sort according to student grade in ascending order\n");
        printf("4. Sort according to student grade in descending order\n");
        printf("\nTo select, enter the option number!\n");

        bytes_Read = read(STDIN_FILENO, option, sizeof(option));
        if (bytes_Read > 0) {
            // Null-terminate the string
            option[bytes_Read - 1] = '\0'; // -1 to remove the newline character
        }

        FileOpenSuccessMessage(commandTokens, count);
         while ((bytesRead = read(fd, &currentChar, 1)) > 0) 
        {

            if(currentChar == '\n' || lineIndex == MAX_LINE_LENGTH - 1) //bir line'ın bittiğini anlar ve line üstünde işlem yapar
            {
                lines[numLines][lineIndex] = '\0';
                lineIndex = 0;
                //line üstünde işlemler
                lines[numLines][strcspn(lines[numLines], "\n")] = 0;
                sortedLines[numLines] = strdup(lines[numLines]);; // Store pointer to each line
                numLines++;
            }
            else //line'ı oluşturmaya devam eder.
            {
                lines[numLines][lineIndex] = currentChar;
                lineIndex++;
            }
           
        }
        if (bytesRead == 0) //end of the file 
        {
            //printf("End of file reached.\n");
        } 
        else if (bytesRead == -1) // error while reading file
        {
            ErrorReadingFileMessage(commandTokens, count);
        }
        // It checks option according to input and it sorts.
        if(strcmp(option, "1") == 0 || strcmp(option, "2") == 0)
        {
            qsort(sortedLines, numLines, sizeof(*sortedLines), compareStringsForName);
            if(strcmp(option, "2") == 0)
            {
                reverseArray(sortedLines, numLines);
                sprintf(logging, "%s: People sorted by student name in descending order\n", commandTokens[count-1]);
                logTaskCompletion(logging);
            }
            else
            {
                sprintf(logging, "%s: People sorted by student name in ascending order\n", commandTokens[count-1]);
                logTaskCompletion(logging);
            }
        }
        else if(strcmp(option, "3") == 0 || strcmp(option, "4") == 0)
        {
            qsort(sortedLines, numLines, sizeof(*sortedLines), compareStringsForGrade);
            if(strcmp(option, "3") == 0)
            {
                reverseArray(sortedLines, numLines);
                sprintf(logging, "%s: People sorted by student grades in ascending order\n", commandTokens[count-1]);
                logTaskCompletion(logging);
            }
            else
            {
                sprintf(logging, "%s: People sorted by student grades in descending order\n", commandTokens[count-1]);
                logTaskCompletion(logging);
            }
        }
        else
        {
            printf("Invalid option!\n");
        }

        for (int i = 0; i < numLines; i++)
        {
            printf("%s\n", sortedLines[i]);
        }
        close(fd);

        //It makes free allocated memory by strdup
        for (int i = 0; i < numLines; i++) 
        {
            free(sortedLines[i]);
        }
        //loglama
        FileCloseSuccessMessage(commandTokens, count);
    }
    else
    {
        NullFileMessage(commandTokens, count);
        printf("Error is occured while file is opening\n");
        //loglama
    }
}

void showAll(char* commandTokens[], int count) 
{
    int fd = open(commandTokens[count - 1], O_RDONLY);//It opens file to read

    char line[MAX_LINE_LENGTH];
    char logging[100];
    

    int bytesRead;
    char currentChar;
    int lineIndex = 0;

    if (fd != -1)
    {
        FileOpenSuccessMessage(commandTokens, count);
        while ((bytesRead = read(fd, &currentChar, 1)) > 0) 
        {
            if(currentChar == '\n' || lineIndex == MAX_LINE_LENGTH - 1) //bir line'ın bittiğini anlar ve line üstünde işlem yapar
            {
                line[lineIndex] = '\0';
                lineIndex = 0;
                //line üstünde işlemler
                printf("%s\n", line);
            }
            else //line'ı oluşturmaya devam eder.
            {
                line[lineIndex] = currentChar;
                lineIndex++;
            }
        }
        if (bytesRead == 0) //End of the file
        {
            sprintf(logging, "%s: All students were showed.\n", commandTokens[count-1]);    
            logTaskCompletion(logging);
        } 
        else if (bytesRead == -1) //Error while reading the file 
        {
            ErrorReadingFileMessage(commandTokens, count);
        }
        //loglama
        close(fd);
        FileCloseSuccessMessage(commandTokens, count);
    }
    else
    {
        printf("Error is occured while file is opening\n");
        NullFileMessage(commandTokens, count);
        //loglama
    }
}

void listGrades(char* commandTokens[], int count) 
{
    int fd = open(commandTokens[count - 1], O_RDONLY); //It opens file to read

    char line[MAX_LINE_LENGTH];
    char logging[100];
    int localCounter = 0;

    int bytesRead;
    char currentChar;
    int lineIndex = 0;

    if (fd != -1)
    {
        
        FileOpenSuccessMessage(commandTokens, count);
        while ((bytesRead = read(fd, &currentChar, 1)) > 0 && localCounter < 5) //gets first five students
        {
            if(currentChar == '\n' || lineIndex == MAX_LINE_LENGTH - 1) //bir line'ın bittiğini anlar ve line üstünde işlem yapar
            {
                line[lineIndex] = '\0';
                lineIndex = 0;
                //line üstünde işlemler
                printf("%s\n", line);
                localCounter++;
            }
            else //line'ı oluşturmaya devam eder.
            {
                line[lineIndex] = currentChar;
                lineIndex++;
            }
        }
        if(localCounter == 5) //First five students are showed
        {
            sprintf(logging, "%s: First 5 students were showed.\n", commandTokens[count-1]);
            logTaskCompletion(logging);
        }
        if (bytesRead == -1) // error is occured while reading the file 
        {
            ErrorReadingFileMessage(commandTokens, count);
        }
        //loglama
        close(fd);
        FileCloseSuccessMessage(commandTokens, count);
    }
    else
    {
        printf("Error is occured while file is opening\n");
        NullFileMessage(commandTokens, count);
        //logging
    }
}

void listSome(char* commandTokens[], int count) 
{
    int fd = open(commandTokens[count - 1], O_RDONLY); //It opens file to read
    
    char line[MAX_LINE_LENGTH];
    char logging[100];
    char *endptr;

    int localCounter = 1;
    int isValid = 1;
    
    long numOfEntries = strtol(commandTokens[1], &endptr, 10); // 10 specifies base 10 (decimal)
    
     int bytesRead;
    char currentChar;
    int lineIndex = 0;
    // Checks inputs validity
    if (*endptr != '\0') {
        printf("Invalid input: %s\n", commandTokens[1]);
        isValid = 0;
    }

    long pageNum = strtol(commandTokens[2], &endptr, 10); // 10 specifies base 10 (decimal)
    if (*endptr != '\0') {
        printf("Invalid input: %s\n", commandTokens[2]);
        isValid = 0;
    }

    if (fd != -1 && isValid == 1)
    {
        FileOpenSuccessMessage(commandTokens, count);
        while ((bytesRead = read(fd, &currentChar, 1)) > 0 && localCounter <= pageNum*numOfEntries) 
        {
            if(currentChar == '\n' || lineIndex == MAX_LINE_LENGTH - 1) //bir line'ın bittiğini anlar ve line üstünde işlem yapar
            {
                line[lineIndex] = '\0';
                lineIndex = 0;
                //line üstünde işlemler
                if((pageNum-1)*numOfEntries<localCounter)//prints according to specified inputs
                {
                    printf("%s\n", line);
                }
                localCounter++;
            }
            else //line'ı oluşturmaya devam eder.
            {
                line[lineIndex] = currentChar;
                lineIndex++;
            }
        }
        if (bytesRead == 0 || localCounter >= pageNum*numOfEntries) //reaches end of the file or reaches the number of entries wanted to be printed
        {
            sprintf(logging, "%s: %sth page first %s entries were showed.\n", commandTokens[count-1], commandTokens[2], commandTokens[1]);
            logTaskCompletion(logging); 
        }
        else if (bytesRead == -1) // error occured while reading the file
        {
            ErrorReadingFileMessage(commandTokens, count);
        }
        //loglama
        close(fd);
        FileCloseSuccessMessage(commandTokens, count);
    }
    else if(isValid == 1)
    {
        printf("Error is occured while file is opening\n");
        NullFileMessage(commandTokens, count);
        //loglama
    }
}

void gtuStudentGrades(char* commandTokens[], int count)
{
    int fileDescriptor = open(commandTokens[1], O_RDWR | O_CREAT | O_APPEND, 0666); //a+ mode reads writes if exist. If it is not exist It creates specified file.
    char logging[100];
    if (fileDescriptor != -1)
    {
        FileOpenSuccessMessage(commandTokens, count);
        printf("File is opened successfully!\n");

        //loglama
        close(fileDescriptor);
        FileCloseSuccessMessage(commandTokens, count);
    }
    else
    {
        printf("Error is occured while file is opening\n");
        NullFileMessage(commandTokens, count);
        //loglama
    }
}

void logTaskCompletion(char *task) 
{
    pid_t pid = fork();
    int fileDescriptor = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    
    //For log operation another process is created and new process performs writing to the file operation.
    if (pid == -1) 
    {
        perror("fork");
        exit(1);
    }
    else if(pid == 0)
    {
        //printf("LOG This is the child process. PID: %d\n", getpid());
        //printf("LOG Parent PID: %d\n", getppid()); // Get parent process ID
        if (fileDescriptor != -1)
        {
            ssize_t bytes_written = write(fileDescriptor, task, strlen(task));
            //fprintf(filePointer, "%s", task);
            if (bytes_written == -1) {
                perror("Error writing to log file");
                close(fileDescriptor);
                exit(EXIT_FAILURE);
            }
            close(fileDescriptor);
        }
        else
        {
            fprintf(stderr, "Error creating log file!\n");
        }
        exit(0); // terminates child process
    }
    else
    {
        // Parent process
        //printf("lOG This is the parent process. PID: %d\n", getpid());
        //printf("lOG Child PID: %d\n", pid); // Get child process ID
        wait(NULL);
    }
}
