#define MAX_CMD 2050
// #define _GNU_SOURCE
#define  _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h> 
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include "smallsh.h"
int processID;  // process ID of smallsh
char directory[MAX_CMD];
char exitStatus[2] = "0";  // holds exit value
int termSignal = 0;  // holds termination signal value
int nonBuiltIn = 0;  // tracks if command is non built in 
int idCount = 0;  // tracks number of background process ID's
char* ID[500];  // tracks background process ID's
char* foreID[500];  // tracks foreground process ID's
int foreIDCount = 0;  // tracks number of foreground process ID's
int stpCount = 0;  // tracks number of times SIGSTP has been caught
bool noBackground = false;  // tracks if SIGSTP has been caught to ignore & (no background processes)
bool terminateSignal = true;  // tracks if last command caught a termination signal

/* struct for input from user*/
struct input
{
    char* command;
    char* argc[513];  // all arguments
    char* firstSymbol; // first redirection
    char* secondSymbol; // second redirection
    char* file1;  // first file listed in input from user
    char* file2; // second file listed in input from user 
    char* background; // &
};

/* Parses the user input and puts each part of the user input in their correct place in struct input. If user input command is exit, status, or cd it performs those commands within the function*/
struct input* createInput(char* userCommand)
{
    struct input* currInput = malloc(sizeof(struct input));
    char* line = NULL;  // tracks if command is a cd, exit, or status
    int count = 0;  // tracks index in input->agrc array
    int endArgs = 0;  // tracks if it has reached past the arguments in the user input when parsing
    int symbCount = 0;  // tracks first or second redirection
    int fileCount = 0;  // tracks number of files in user input
    bool currArg = false;  // tracks if token is an argument
    bool currSymbol = false;  // tracks if token is a symbol
    bool currFile = false;  // tracks if token is a file
    char str[12];  // holds process ID 
    bool mkdir = false;  // tracks if command is mkdir to make sure file becomes an argument
    bool stripped = false;  // tracks if token has been stripped of "$$" to replace with process ID
    char* slash = "/\0";  // used to add to directory argument for cd 
    char* null = "\0";  // adds null byte
    nonBuiltIn = 0;  // resets nonBuiltIn counter
    // For use with strtok_r
    char* saveptr;
    // opens current directory
    DIR* currDirSave = opendir(".");
    // The first token is the commnad
    char* token = strtok_r(userCommand, " ", &saveptr);
    // if command is "cd" strips trailing new line
    if (strncmp(token, "cd", strlen("cd")) != 0) {
        strtok(token, "\n");
    }
    // saves command in command slot in input struct
    currInput->command = calloc(strlen(token), sizeof(char) + 1);
    strcpy(currInput->command, token);
    // user entered command exit and termnates the program
    if (strncmp(token, "exit", strlen("exit")) == 0) {
        exit(0);
    }
    // command entered is cd
    if (strncmp(token, "cd", strlen("cd")) == 0) {
        line = "None";  // tells function everything in input after command is an argument
        // user entered cd by itdself
        if (strlen(userCommand) == 3) {
            chdir(getenv("HOME"));  // sets current directory to HOME
            getcwd(directory, sizeof(directory));  // sets current wokring directory to variable directory
        }
        else {
            // next token is argument
            token = strtok_r(NULL, " ", &saveptr);
            // if token contatins "$$"
            if (strpbrk(token, "$$") != 0) {
                sprintf(str, "%d", processID);  // saves processID to str
                // copies token and processID to token3 and sets current directory to token3
                char* token2 = strtok(token, "$$");
                char* token3 = malloc(strlen(token2) + strlen(str) + 1);
                strcpy(token3, token);
                strcat(token3, str);
                strcat(token3, null);
                strtok(token3, "\n");
                chdir(token3);
                getcwd(directory, sizeof(directory));  // sets current wokring directory to variable directory
            }
            else {
                // adds current working directory + "/" + token to newDir and sets current directory to newDir
                char* newDir = malloc(strlen(directory) + strlen(token) + strlen(slash) + strlen(slash) + 1);
                strcpy(newDir, directory);
                strcat(newDir, slash);
                strcat(newDir, token);
                strcat(newDir, null);
                strtok(newDir, "\n");
                chdir(newDir);
                getcwd(directory, sizeof(directory));  // sets current wokring directory to variable directory
            }
        }
    }
    // command is status
    if (strncmp(token, "status", strlen("status")) == 0) {
        line = "none";  // tells function everything in input after command is an argument
        // if function hasn't parsed a non built in function and no termination signal has been captured,
        // prints exit value to terminal
        if (nonBuiltIn == 0 && terminateSignal == false) {
            printf("exit value %s\n", exitStatus);
            terminateSignal = false;
        }
        // caught termination signal
        else {
            printf("terminated by signal %d\n", termSignal);  // prints termination signal number to terminal
            terminateSignal = true;
        }
    }
    terminateSignal = false;
    // command is "echo"
    if (strncmp(token, "echo", strlen("echo")) == 0) {
        nonBuiltIn++;  // this is a non built in command
        // keeps parsing until reaching end of user input
        while (line == NULL) {
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL) {
                line = "none";  // tells function everything in input after command is an argument
                break;
            }
            // token contains "$$"
            if (strpbrk(token, "$$") != 0) {
                stripped = true;
                sprintf(str, "%d", processID);
                char* token2 = strtok(token, "$$"); // removes "$$" from token
                char* token3 = malloc(strlen(token2) + strlen(str) + 1);
                strcpy(token3, token);
                strcat(token3, str);
                strcat(token3, null); // token 3 contains token wit "$$" replaced with processID
                currInput->argc[count] = token3;  // saves token in argument array in input struct
                count++;  // increases index of argc array
                currArg = true;  // function know token is argument
            }
            else {
                strtok(token, "\n");  // strips trailing new line
                currInput->argc[count] = token;  // saves token in argument array in input struct
                count++;  // increases index of argc array
                currArg = true;  // function knows token is argument
            }
        }
    }
    // command is non built in
    if (line == NULL) {
        nonBuiltIn++;
    }
    // parse till end of user input
    while (line == NULL) {
        token = strtok_r(NULL, " ", &saveptr);
        // reached the end of user input
        if (token == NULL) {
            break;
        }
        // token is a redirection (< or >)
        if ((strncmp(token, ">", strlen(token)) == 0 && strncmp(token, ">", strlen(">")) == 0) || (strncmp(token, "<", strlen(token)) == 0 && strncmp(token, "<", strlen("<")) == 0)) {
            // this is the first redirection
            if (symbCount == 0) {
                // saves symbol in firstSymbol of input struct
                currInput->firstSymbol = calloc(strlen(token) + 1, sizeof(char));
                strcpy(currInput->firstSymbol, token);
                symbCount++;  // increase symbCount to show first redirection has already been recorded
                endArgs++;  // no more arguments after a redirection
                currArg = false;  // no more arguments after a redirection
                currSymbol = true;
            }
            // second redirection
            else {
                // saves redirection in secondSymbol of input struct
                currInput->secondSymbol = calloc(strlen(token) + 1, sizeof(char));
                strcpy(currInput->secondSymbol, token);
                symbCount++;  // increase symbCount to show second redirection has already been recorded
                endArgs++;  // no more arguments after a redirection
                currSymbol = true;
                currArg = false;  // no more arguments after a redirection
            }
        }
        else {
            // token is an argument
            if (endArgs == 0) {
                // token has "$$" in it
                if (strpbrk(token, "$$") != 0) {
                    // replaces "$$" in token with processID
                    stripped = true;
                    sprintf(str, "%d", processID);
                    char* token2 = strtok(token, "$$");
                    char* token3 = malloc(strlen(token2) + strlen(str) + 1);
                    strcpy(token3, token2);
                    strcat(token3, str);
                    strcat(token3, null);
                    currInput->argc[count] = token3;  // saves token with "$$" replaced with processID in argument array in input struct
                    count++;  // increase count for argc array
                    currArg = true;  // token is an argument
                }
                else {
                    strtok(token, "\n");  // remove trailing new line
                    currInput->argc[count] = token;  // saves token is agrc array
                    count++;  // increase count for argc array
                    currArg = true;  // token is an argument
                }
            }
            // token is an "&" (command runs in background) and commands are allowed to occur in the background
            if (strncmp(token, "&\n", strlen(token)) == 0 && noBackground == false) {
                // saves "&" as background in input struct
                currInput->background = calloc(strlen(token) + 1, sizeof(char));
                strcpy(currInput->background, token);
                endArgs++;  // no more arguments after "&"
                // function thought "&" was a argument
                if (currArg == true) {
                    currInput->argc[count - 1] = "\0";  // removes last argument placed in agrc array because it is "&"
                }
                currArg = false;  // no more arguments after "&"
                break; // no more input afer "&" so stops parsing
            }
            // first file
            else if (fileCount == 0) {
                DIR* currDir = opendir(".");  // opens current directory
                struct dirent* aDir;
                // goes through all files in current directory
                while ((aDir = readdir(currDir)) != NULL) {
                    // file in directory matches user input file name
                    if ((strncmp(token, aDir->d_name, strlen(token) - 1) == 0) || (mkdir == true) || (currArg == false)) {
                        // token contains "$$"
                        if (strpbrk(token, "$$") != 0) {
                            // replaces "$$" with processID
                            sprintf(str, "%d", processID);
                            char* token4 = token;
                            char* token2 = strtok(token, "$$");
                            char* token3 = malloc(strlen(token2) + strlen(str) + 1);
                            strcpy(token3, token);
                            strcat(token3, str);
                            currInput->file1 = calloc(strlen(token3) + 1, sizeof(char));
                            strcpy(currInput->file1, token3);  // saves token with "$$" replaced with processID in file1 in input struct
                            fileCount++; // increase count to show we have found file in user input
                            // function thought file1 was a argument
                            if (currArg == true) {
                                currInput->argc[count - 1] = NULL;  // removes last argument placed in agrc array because it is file1
                            }
                            endArgs++;  // no more arguments after a file
                            currArg = false;  // no more arguments after a file
                            currFile = true;  // file has been placed in input struct
                        }
                        else if (currFile == false) {
                            currInput->file1 = calloc(strlen(token) + 1, sizeof(char));
                            // token has "$$" stripped already
                            if (stripped == true) {
                                // adds process ID to token and copies to token 3
                                char* token3 = malloc(strlen(token) + strlen(str) + 1);
                                strcpy(token3, token);
                                strcat(token3, str);
                                strtok(token3, "\n");
                                strcpy(currInput->file1, token3);  // copies token3 as file1
                                fileCount++;  // increase count to show we have found file in user input
                                // function thought file1 was a argument
                                if (currArg == true) {
                                    currInput->argc[count - 1] = NULL;  // removes last argument placed in agrc array because it is file1
                                }
                                endArgs++;  // no more arguments after a file
                                currArg = false;  // no more arguments after a file
                                currFile = true;  // file has been placed in input struct
                            }
                            // token doesn't have "$$"
                            else {
                                strtok(token, "\n");  // removes trailing new line
                                strcpy(currInput->file1, token);  // copies token as file1 in input struct
                                fileCount++;  // increase count to show we have found file in user input
                                if (currArg == true) {
                                    currInput->argc[count - 1] = NULL;
                                }
                                endArgs++;  // no more arguments after a file
                                currArg = false;  // no more arguments after a file
                                currFile = true;  // file has been placed in input struct
                            }

                        }
                    }
                }
            }
            // token is file2
            else {
                currInput->file2 = calloc(strlen(token) + 1, sizeof(char));
                strtok(token, "\n");  // removes trailing new line
                strcpy(currInput->file2, token);  // copies token to file2 in input struct
                endArgs++;  // no more arguments after a file
                currArg = false;  // no more arguments after a file
            }
        }
    }
    // printf("%s\n", currInput->command);
    // printf("%s\n", currInput->argc[0]);
    // printf("%s\n", currInput->firstSymbol);
    // printf("%s\n", currInput->file1);
    // printf("%s\n", currInput->secondSymbol);
    // printf("%s\n", currInput->file2);
    // printf("%s\n", currInput->background);
    return currInput;
}


/* performs non built in commands using fork(), exec(), and waitpid() */
void  otherCommands(struct input* input) {
    char str[12];  // used to hold exit value of child
    int loop = 0;  // agrc index
    // counts number of arguments in argc array
    while (input->argc[loop] != NULL) {
        loop++;
    }
    char* argArray[loop + 1];  // holds an an array of the command and all arguments 
    loop = 0;  // resets index to 0
    // loops through each argument in argc array
    while (input->argc[loop] != NULL) {
        if (loop == 0) {
            argArray[loop] = input->command;  // saves command as first element in argArray
            loop++;
        }
        // saves all arguments after command in argArray
        else {
            argArray[loop] = input->argc[loop - 1];
            loop++;
        }
    }
    argArray[loop] = input->argc[loop - 1];  // adds last element of argc array to argArray
    argArray[loop + 1] = NULL;  // adds null to argArray
    int childStatus;
    char* arr[3];  // holds array of command and file1
    arr[0] = input->command;
    arr[1] = input->file1;
    arr[2] = NULL;
    char* arr2[2];  // hold array of just command
    arr2[0] = input->command;
    arr2[1] = NULL;
    bool openFile = false;  // holds whether there is a file for redirection
    bool openFile2 = false;  // holds whether there is a second file for redirection
    int saved_stdout;
    saved_stdout = dup(1);  // saves current stdout for later use
    fflush(stdout);
    pid_t childBackground;
    pid_t spawnPid = fork();  // fork for children processes
    char childID[20];  // holds child process ID's
    switch (spawnPid) {
    case -1:
        perror("fork()\n");
        exit(1);
        break;
        // child process
    case 0:
        // command is supposed to run in background, prints background pid of child
        if (input->background != NULL) {
            printf("background pid is %d\n", getpid());
            fflush(stdout);
        }
        // there is a first redirection in input struct
        if (input->firstSymbol != NULL) {
            openFile = true;  // this is file1
            // there is a first output redirection
            if ((strncmp(input->firstSymbol, ">", strlen(input->firstSymbol)) == 0)) {
                int out;
                // open file1 for output
                out = open(input->file1, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                dup2(out, 1);  // created copy of out for output
                close(out);  // close file1
                // there is a second redirection
                if (input->secondSymbol != NULL) {
                    openFile2 = true;  // this is file 2
                    // this is a output redirection
                    if ((strncmp(input->secondSymbol, ">", strlen(input->secondSymbol)) == 0)) {
                        int file2;
                        // open file2 for output
                        file2 = open(input->file2, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                        dup2(file2, 1);  // created copy of file2 for output
                        close(file2);  // close file2
                    }
                    // this is an input redireciton
                    if ((strncmp(input->secondSymbol, "<", strlen(input->secondSymbol)) == 0)) {
                        int file2;
                        file2 = open(input->file2, O_RDONLY);  // opens file2 for input
                        // file2 cannot be opened for input
                        if (file2 < 0) {
                            dup2(saved_stdout, 1); // restores stdout
                            strtok(input->file2, "\n");  // removes trailing new line
                            printf("cannot open %s for input\n", input->file2);
                            fflush(stdout);
                            exit(1);
                        }
                        // file 2 opened correctly
                        else {
                            dup2(file2, 0);  // create copy of file2 for input
                            close(file2);
                        }
                    }
                }
            }
            // first redirection is input redirection
            if ((strncmp(input->firstSymbol, "<", strlen(input->firstSymbol)) == 0)) {
                int file;
                file = open(input->file1, O_RDONLY);  // opens file for input
                // cannot open file for input
                if (file < 0) {
                    strtok(input->file1, "\n");  // remove trailing new line
                    printf("cannot open %s for input\n", input->file1);
                    exit(1);
                }
                dup2(file, 0);  // create copy of file for input
                close(file);
                // there is a second redirection
                if (input->secondSymbol != NULL) {
                    // second redirection is a output redirection
                    if ((strncmp(input->secondSymbol, ">", strlen(input->secondSymbol)) == 0)) {
                        int file2;
                        // file2 is opened for output
                        file2 = open(input->file2, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                        dup2(file2, 1);  // copies file2 for output
                        close(file2);
                    }
                    // second redirection is a input redirection
                    if ((strncmp(input->secondSymbol, "<", strlen(input->secondSymbol)) == 0)) {
                        int file2;
                        file2 = open(input->file2, O_RDONLY);  // opens file2 for input
                        // cannot open file2
                        if (file2 < 0) {
                            dup2(saved_stdout, 1);  // restore stdout
                            strtok(input->file2, "\n");  // removes trailing new line
                            printf("cannot open %s for input\n", input->file2);
                            exit(1);
                        }
                        dup2(file2, 0);  // copies file2 for input
                        close(file2);
                    }
                }
            }
        }
        // the user input contains a first redirection
        if (openFile == true) {
            execvp(input->command, arr2);   // creates child process with arr2 (just array of input command)
            if (errno == 2) {
                perror(input->command);  // command is not recognized/process failed
            }
        }
        // no arguments and no files in user input
        if (input->argc[0] == NULL && openFile == false) {
            execvp(input->command, arr); // creates child process with arr (array of input command and file1)
            if (errno == 2) {
                perror(input->command);  // process failed
            }
        }
        else {
            // input contains "&"
            if (input->background != NULL) {
                argArray[loop] = NULL;
            }
            // creates child process with argArray (array of command + arguments)
            execvp(input->command, argArray);
            if (errno == 2) {
                exit(1);  // process failed
            }
        }
        exit(2);
        break;
    default:
        // if command is supposed to be run in background, then doesn't wait for child process to terminate
        if (input->background != NULL) {
            childBackground = waitpid(spawnPid, &childStatus, WNOHANG);
            break;
        }
        // if command runs in foreground, wait for child process to terminat
        else {
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            break;
        }
    }
    // command runs in background
    if (input->background != NULL) {
        sprintf(childID, "%d", spawnPid);  // saves child process ID in childID
        char* cID = malloc(strlen(childID) + 1);
        strcpy(cID, childID);
        ID[idCount] = cID;  // saves child process ID in ID array
        idCount++;  // increase for index
    }
    // command runs in foreground
    if (input->background == NULL) {
        sprintf(childID, "%d", spawnPid);  // saves child process ID in childID
        char* cID = malloc(strlen(childID) + 1);
        strcpy(cID, childID);
        foreID[foreIDCount] = cID;  // saves child process ID in foreID array
        foreIDCount++;  // increase for index
    }
    // exit value is 1
    if (childStatus == 256) {
        sprintf(str, "%d", 1);
        strcpy(exitStatus, str);  // copies 1 into exitStatus
        if (input->background != NULL) {
            sleep(1);
        }
    }
    else {
        sprintf(str, "%d", 0);
        strcpy(exitStatus, str);  // copies 0 into exitStatus
        if (input->background != NULL) {
            sleep(1);
        }
    }
}


/* enters and exits out of foreground-only mode when catching SIGST*/
void sigstpHandler(int sig_num)
{
    // Reset handler to catch SIGINT next time
    signal(SIGTSTP, sigstpHandler);
    // SIGSTP is intercepted to enter foreground-only mode
    if (stpCount % 2 == 0) {
        printf("Entering foreground-only mode (& is now ignored)\n");
        noBackground = true;  // commands are not allowed to run in background now
    }
    // SIGSTP is intercepted to exit foreground-only mode
    if (stpCount % 2 != 0) {
        printf("Exiting foreground-only mode\n");
        noBackground = false;  // commands are now allowed to run in background
    }
    fflush(stdout);
    stpCount++; // increase count to recognize whether program is going into or out of foreground-only mode
}


/* any child running as foreground process is terminated when catching SIGINT*/
void sigintHandler(int sig_num)
{
    // Reset handler to catch SIGINT next time
    signal(SIGINT, sigintHandler);
    // there are foreground processes running
    if (foreIDCount != 0) {
        int j = 0;
        int status;
        // loops through processes in foreground
        while (j < foreIDCount) {
            if (foreID[j] != NULL) {
                int pid = atoi(foreID[j]);  // convert process ID in foreID[j] to int
                kill(pid, SIGKILL);  // kills process with process ID of pid
                printf("terminated by signal %d\n", sig_num);  // prints what signal terminated it
                fflush(stdout);
                terminateSignal = true;  // termination signal was caught
                termSignal = sig_num;  // sets the value of termSignal to current termination signal
            }
            j++;
        }
    }
}


/* prints if any process ID's running in the background terminated and how they terminated, catches SIGINT & SIGSTP, and takes input from the user*/
void cmd() {
    char* end;
    char* userCommand = NULL;  // user input
    size_t len = 0;
    int lineSize = 0;
    int count, i, flag = 0;
    // there are processes running in the background
    if (idCount != 0) {
        int j = 0;
        // loops through all background process ID's
        while (j < idCount) {
            if (ID[j] != NULL) {
                int status;
                long int child = strtol(ID[j], &end, 10);
                pid_t ch = waitpid(child, &status, WNOHANG);
                // if background process terminated with termination signal
                if (WIFSIGNALED(status)) {
                    printf("background pid %zu is done: terminated by signal %d\n", child, WTERMSIG(status));
                    terminateSignal = true;
                    termSignal = WTERMSIG(status);  // termSignal is current termination signal
                    fflush(stdout);
                    ID[j] = NULL;  // removes process ID from array
                    status = 0;  // resests status
                }
                // background process terminated normally
                else if (waitpid(child, &status, WNOHANG) != 0) {
                    printf("background pid %zu is done: exit value 0\n", child);
                    terminateSignal = false;
                    fflush(stdout);
                    ID[j] = NULL;  // removes process ID from array
                }
            }
            j++;
        }
    }
    printf(": ");
    fflush(stdout);
    signal(SIGTSTP, sigstpHandler);  // intercepts SIGSTP
    signal(SIGINT, sigintHandler); // intercepts SIGINT
    while (userCommand == NULL) {
        lineSize = getline(&userCommand, &len, stdin);  // saves user input as userCommand
    }
    // if user entered a blank line or a comment line
    if (userCommand != NULL && (userCommand[0] == '\n' || userCommand[0] == '#')) {
        flag = 1;  // tells function that it needs to skip evaluating the input
    }
    // not a comment or blank line
    if (flag != 1) {
        struct input* input = createInput(userCommand);  // creates input struct off userCommand
        // the command is a non built in command
        if (nonBuiltIn != 0) {
            otherCommands(input);  // calls otherCommands to run user input
        }
    }
    free(userCommand);
}


/* sets current working directory to current directory, saves processID, contionously call cmd().
compile the program as follows: "gcc --std=c99 -o smallsh smallsh.c"*/
int main(void) {
    getcwd(directory, sizeof(directory));  // set current working directory to directory
    processID = getpid();  // sets processID
    // printf("%d\n", processID);
    // continous call to cmd()
    while (7) {
        cmd();
    }
    return EXIT_SUCCESS;
}
