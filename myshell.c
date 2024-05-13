#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

// Function to parse user input into commands and tokens
char** parseInput(char* command, int* numTokens, char* commandType) {
    char** tokens = (char*)malloc(sizeof(char) * 256);
    int tokenIndex = 0;
    char cmdType = 'S';  // Default to single command
    int isValidInput = 1;

    if (command[0] == '&' || command[0] == '#' || command[0] == '>') {
        cmdType = 'I';  // Incorrect input
        isValidInput = 0;
    } else if (command[0] == '\0') {
        cmdType = 'E';  // Empty input
    }

    // Tokenize the command using spaces
    char* token = strtok(command, " ");
    while (token != NULL) {
        tokens[tokenIndex] = (char*)malloc(strlen(token) + 1);
        strcpy(tokens[tokenIndex], token);
        tokenIndex++;
        token = strtok(NULL, " ");
    }
    tokens[tokenIndex] = NULL;

    // Detect command type based on tokens
    for (int i = 0; i < tokenIndex; i++) {
        if (strcmp(tokens[i], "&&") == 0) {
            cmdType = 'P';  // Parallel execution
            isValidInput = 1;
            break;
        } else if (strcmp(tokens[i], "##") == 0) {
            cmdType = 'S';  // Sequential execution
            isValidInput = 1;
            break;
        } else if (strcmp(tokens[i], ">") == 0) {
            cmdType = 'O';  // Output redirection
            isValidInput = 1;
            break;
        }
    }

    if (!isValidInput) {
        cmdType = 'I';  // Incorrect input
    }

    *numTokens = tokenIndex;
    *commandType = cmdType;
    return tokens;
}

// Function to execute a single command
void executeCommand1(char** tokens) {
    if (execvp(tokens[0], tokens) == -1) {
        printf("Shell: Incorrect command");
        perror("Shell: Incorrect command");
        exit(EXIT_FAILURE);
    }
}

void executeCommand(char** tokens) {
    int childPID = fork();
    if (childPID < 0) {
        perror("Fork failed");
        exit(1);
    } else if (childPID == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        if (execvp(tokens[0], tokens) == -1) {
            printf("Shell: Incorrect Command\n");
            exit(1);
        }
    } else {
        int status;
        waitpid(childPID, &status, 0);
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 0) {
                printf("Shell: Command executed successfully\n");
            } else {
                printf("Shell: Command terminated with an error\n");
            }
        } else {
            printf("Shell: Command terminated abnormally\n");
        }
    }
}



// Function to execute multiple commands in parallel
void executeParallelCommands(char** tokens, int numTokens) {
    int subCommandStart = 0;

    for (int i = 0; i <= numTokens; i++) {
        if (i == numTokens || strcmp(tokens[i], "&&") == 0) {
            tokens[i] = NULL;

            if (strcmp(tokens[subCommandStart], "cd") == 0) {
                if (chdir(tokens[subCommandStart + 1]) == -1) {
                    printf("Shell: Incorrect command\n");
                }
                subCommandStart = i + 1;
                continue;
            }

            int childPID = fork();
            if (childPID < 0) {
                perror("Fork failed");
                exit(1);
            } else if (childPID == 0) {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                executeCommand1(&tokens[subCommandStart]);
                exit(0);
            }
            subCommandStart = i + 1;
        }
    }

    while (wait(NULL) > 0);
}

// Function to execute multiple commands sequentially
void executeSequentialCommands(char** tokens, int numTokens) {
    int subCommandStart = 0;

    for (int i = 0; i <= numTokens; i++) {
        if (i == numTokens || strcmp(tokens[i], "##") == 0) {
            tokens[i] = NULL;

            if (strcmp(tokens[subCommandStart], "cd") == 0) {
                if (chdir(tokens[subCommandStart + 1]) == -1) {
                    printf("Shell: Incorrect command\n");
                }
                subCommandStart = i + 1;
                continue;
            }

            int childPID = fork();
            if (childPID < 0) {
                perror("Fork failed");
                exit(1);
            } else if (childPID == 0) {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                executeCommand1(&tokens[subCommandStart]);
                exit(0);
            }
            waitpid(childPID, NULL, 0);
            subCommandStart = i + 1;
        }
    }
}

// Function to execute a command with output redirection
void executeCommandRedirection(char** tokens, int numTokens) {
    int outputFileIndex = -1;

    for (int i = 0; i < numTokens && outputFileIndex == -1; i++) {
        if (strcmp(tokens[i], ">") == 0) {
            outputFileIndex = i + 1;
        }
    }

    if (tokens[outputFileIndex] == NULL) {
        printf("Shell: Incorrect command\n");
    } else {
        tokens[outputFileIndex - 1] = NULL;

        int childPID = fork();
        if (childPID < 0) {
            perror("Fork failed");
            exit(1);
        } else if (childPID == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            close(STDOUT_FILENO);
            open(tokens[outputFileIndex], O_CREAT | O_WRONLY | O_APPEND, 0666);

            executeCommand1(tokens);
            exit(0);
        }
        waitpid(childPID, NULL, 0);
    }
}

void setupSignalHandlers() {
    signal(SIGINT, SIG_IGN);   // Ignore Ctrl+C signal
    signal(SIGTSTP, SIG_IGN);  // Ignore Ctrl+Z signal
}

int main() {
    setupSignalHandlers();
    size_t MAXSIZE = 256;
    char commandType;
    char* command = (char*)malloc(sizeof(char) * MAXSIZE);
    char** tokens;
    char pwd[MAXSIZE];
    int numTokens, subCommandIndex = 0;

    while (1) {
        getcwd(pwd, sizeof(pwd));
        printf("%s$", pwd);

        if (fgets(command, MAXSIZE, stdin) == NULL) {
            break;
        }
        command[strlen(command) - 1] = '\0';

        tokens = parseInput(command, &numTokens, &commandType);
        // Check for the 'exit' command
        if (strcmp(tokens[0], "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        if (commandType == 'P') {
            executeParallelCommands(tokens, numTokens);
        } else if (commandType == 'S') {
            executeSequentialCommands(tokens, numTokens);
        } else if (commandType == 'O') {
            executeCommandRedirection(tokens, numTokens);
        } else if (commandType == 'C') {
             executeCommand(tokens);
               
        }

        for (int i = 0; i < numTokens; i++) {
free(tokens[i]); // Free allocated memory for tokens
        }

        subCommandIndex = 0; // Reset subCommandIndex for the next iteration
    }

    free(tokens); // Free the tokens array
    free(command); // Free the command string

    return 0;
}
