
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
#define REPL_LIST_JOBS "listjobs"
#define REPL_FOREGROUNG "fg"


bool TESTING_MODE = true;


struct processMetaData
{
   __pid_t processID;
   int execId;
   char * execStatus;
};

struct processMetaData execHistory[100];
int execHistoryLength = 0;

int prevASync = NULL;



/*
    function takes char and Delimiter as input and returns a array of splitted char's
*/
int parseInput(char replInput[], char * arrayToPut[], char delimiter[]) {

    int lengthOfTokens = 0;
    arrayToPut[lengthOfTokens] = strtok(replInput, delimiter);

    while (arrayToPut[lengthOfTokens] != NULL) {
        arrayToPut[++lengthOfTokens] = strtok(NULL, delimiter);
    }

    arrayToPut[lengthOfTokens + 1] = (char * ) NULL;
    return lengthOfTokens;
}



char * getStatus( __pid_t pid) {
    int processStatus;
    __pid_t return_pid = waitpid(pid, &processStatus, WNOHANG); // Only checks status of pid
    if( return_pid == -1) {
        perror("get Status of pid");
        return "e";
    } else if ( return_pid == 0) {   
        return "r";
    } else if ( return_pid == pid) {
        return "f";
    }

}

void childProcess(char * replParsedInput[]){
        // Use execv
        printf("C:Child working\n");

        int exec_return_value = execvp(replParsedInput[0], replParsedInput);
        if (exec_return_value == -1) {
            perror("Error is executing command");
            exit(EXIT_FAILURE);
        } 
}

void parentProcess(char * replParsedInput[], __pid_t child_pid, bool async) {
    int   chld_state;
    pid_t rc_pid;
    int execID;
    printf("P:Child process pid is : %d \n", child_pid);
    

    if (!async) {
        printf("P:Async turned OFF waiting for child\n");
        int wait_status;
        __pid_t terminated_child_pid = waitpid( child_pid, &wait_status, 0);
        if (terminated_child_pid == -1) {
            perror("wait");
            exit(EXIT_FAILURE);
        }
        execID = rand()% 900+ 100;
    } else {
        // rc_pid = waitpid( child_pid, &chld_state, WNOHANG);
        // printf("wnohang:%d, %s\n",rc_pid, getStatus(child_pid));

                // Assign same execution id to Async process
        if (prevASync == NULL) {
            execID = rand()% 900+ 100;
        } else {
            execID = prevASync;
        }
    }
    struct processMetaData processMeta = {
        .processID = child_pid,
        .execId = execID,
        .execStatus = getStatus(child_pid)
    };
        
    execHistory[execHistoryLength++] = processMeta;

}

/*
    Executes the command provided by replArrayInput by forking a child and replacing it with exec. Also async flag decides wether REPL waits for child process to finish or not. 
*/
__pid_t replExec(char * replParsedInput[], bool async) {

    __pid_t child_pid = fork();

    if (child_pid == -1) {
        // Error in creating the fork
        perror("P:Error in forking the process\n");
        exit(EXIT_FAILURE);
    }


    if (child_pid == 0) {
        // Child process working 

        childProcess(replParsedInput);
        exit(EXIT_SUCCESS);
    }
    if (child_pid > 0) {
        
        parentProcess( replParsedInput, child_pid, async);
            
    }
    return child_pid;
}


void listExecHistory (){ 
    printf("\nProcess history\n");
    for(int i =0; i< execHistoryLength; i++) {
        execHistory[i].execStatus = getStatus( execHistory[i].processID);
        printf("Command %d with PID %d and execID: %d status: %s\n", i, execHistory[i].processID, execHistory[i].execId, execHistory[i].execStatus);
    }
    printf("\n");
}



/*
    Executes Read–eval–print loop
*/
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

            if (strncmp(seperatedInput[inputTokenLength - 1], REPL_ASYNC, 1) == 0) {
                async = true;
                seperatedInput[inputTokenLength - 1] = NULL;
                printf("Async Mode On \n");
            }

            // Check special command

            if (strncmp(seperatedInput[0], REPL_EXIT, 4) == 0) {
                printf("Exiting the REPL! \n");
                exitREPL = true;
                exit(0);
            } else if (strncmp(seperatedInput[0], REPL_LIST_JOBS, 7) == 0) {
                listExecHistory();
            } else if (strncmp(seperatedInput[0], REPL_FOREGROUNG, 2) == 0) {
                int wait_status;
                __pid_t terminated_child_pid = waitpid( (__pid_t) * seperatedInput[1], &wait_status, 0);
                if (terminated_child_pid == -1) {
                    perror("wait");
                    exit(EXIT_FAILURE);
                }
            } else {
                // #Execute
                replExec(seperatedInput, async);
            }

            // #Clean

            listExecHistory();
            // flush the input buffer            
            // while(getchar() != '\n');

        }

    }
}


int main(void) {

    printf("The shell is starting\n");

    // Start REPL 
    repl();
    return 0;
}

// sh test.sh 5 1 &
// sh test.sh 5 2 &