
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/wait.h>


#define REPL_INPUT_LIMIT 100
#define REPL_MAX_TOKENS 15
#define REPL_ASYNC "&"
#define REPL_EXIT "exit"

bool TESTING_MODE = true;


/*
    Executes Read–eval–print loop
*/
void repl();

/*
    function takes char and Delimiter as input and returns a array of splitted char's
*/
int parseInput(char replInput[], char * arrayToPut[], char delimiter[]);

__pid_t replExec(char * replArrayInput[], bool async);

int main(void) {

    printf("The shell is starting\n");

    // Start REPL 
    repl();
    return 0;
}

void repl(void) {
    // Exit variable
    bool exitREPL = false;
    while (!exitREPL) {
        char text[] = "sh550 > ";
        char replInput[REPL_INPUT_LIMIT];
        bool async = false;
        // #Get input
        printf("%s", text);
        fgets(replInput, REPL_INPUT_LIMIT, stdin);

        if (strcmp(replInput, "\n")) {

            // Parse Input
            char * seperatedInput[REPL_MAX_TOKENS] = {
                NULL
            };
            int inputTokenLength = parseInput(replInput, seperatedInput, " \n");

            // Check special command

            if (strncmp(seperatedInput[0], REPL_EXIT, 4) == 0) {
                printf("Exiting the REPL! \n");
                exitREPL = true;
                exit(0);
            } else if (strncmp(seperatedInput[inputTokenLength - 1], REPL_ASYNC, 1) == 0) {
                async = true;
                seperatedInput[inputTokenLength - 1] = NULL;
                printf("Async Mode On \n");
            }

            // #Execute

            replExec(seperatedInput, async);
            // #Clean


            // flush the input buffer            
            while(getchar() != '\n');

        }

    }
}


int parseInput(char replInput[], char * arrayToPut[], char delimiter[]) {

    int lengthOfTokens = 0;
    arrayToPut[lengthOfTokens] = strtok(replInput, delimiter);

    while (arrayToPut[lengthOfTokens] != NULL) {
        arrayToPut[++lengthOfTokens] = strtok(NULL, delimiter);
    }

    arrayToPut[lengthOfTokens + 1] = (char * ) NULL;
    return lengthOfTokens;
}


__pid_t replExec(char * replParsedInput[], bool async) {

    __pid_t child_pid = fork();

    if (child_pid == -1) {
        // Error in creating the fork
        perror("P:Error in forking the process\n");
        exit(EXIT_FAILURE);
    }


    if (child_pid == 0) {
        // Child process working 
        // Use execv
        printf("C:Child working\n");

        char * execution[] = {"ls", "-l", (char * ) NULL};

        int exec_return_value = execvp(replParsedInput[0], replParsedInput);
        if (exec_return_value == -1) {
            perror("Error is executing command");
            exit(EXIT_FAILURE);
        } 

    }
    if (child_pid > 0) {
        printf("P:Child process pid is : %d \n", child_pid);

        if (!async) {
            printf("P:Async turned OFF waiting for child\n");

            int wait_status;
            __pid_t terminated_child_pid = waitpid( child_pid, &wait_status, 0);
            if (terminated_child_pid == -1) {
                perror("wait");
                exit(EXIT_FAILURE);
            }
        }
        // printf("P: my child %d terminates.\n", child_pid);
    }
    return child_pid;
}


// sh test.sh 5 1 &
// sh test.sh 5 2 &