
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/wait.h>

#define REPL_HISTORY_LIMIT 100

#define REPL_INPUT_LIMIT 100
#define REPL_MAX_TOKENS 15
#define REPL_ASYNC "&"
#define REPL_EXIT "exit"
#define REPL_LIST_JOBS "listjobs"
#define REPL_FOREGROUND "fg"


bool TESTING_MODE = true;


struct processMetaData
{
   __pid_t processID;
   char command[REPL_INPUT_LIMIT];
   int execId;
   char * execStatus;
};

struct processMetaData execHistory[REPL_HISTORY_LIMIT];
int execHistoryLength = 0;

int prevASync = -1;



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



char * getStatus(struct processMetaData meta) {
    int processStatus;
    // in finished state waitpid will return error as there will be no process in execution. Thus only check when process is running.
    if( strcmp(meta.execStatus,"RUNNING") == 0){
        __pid_t return_pid = waitpid(meta.processID, &processStatus, WNOHANG); // Only checks status of pid
        if( return_pid == -1) {
            perror("error in getting process");
            return "ERROR";
        } else if ( return_pid == 0) {   
            return "RUNNING";
        } else if ( return_pid == meta.processID) {
            return "FINISHED";
        }
    } else {
        return meta.execStatus;
    }

}

void childProcess(char * replParsedInput[]){
        // Use execv
        // printf("C:Child working\n");

        int exec_return_value = execvp(replParsedInput[0], replParsedInput);
        if (exec_return_value == -1) {
            perror("Error is executing command");
            exit(EXIT_FAILURE);
        } 
}

void parentProcess(char * replParsedInput[], __pid_t child_pid, bool async) {
    pid_t rc_pid;
    int execID;
    char * status;
    // printf("P:Child process pid is : %d \n", child_pid);
    

    if (!async) {
        // printf("P:Async turned OFF waiting for child\n");
        int wait_status;
        __pid_t terminated_child_pid = waitpid( child_pid, &wait_status, 0);
        status  = "FINISHED";
        if (terminated_child_pid == -1) {
            status  = "Error";
            perror("wait");
            exit(EXIT_FAILURE);
        }
        execID = rand()% 900+ 100;
        prevASync = -1;
    } else {
        // Assign same execution id to Async process
        if (prevASync < 0) {
            execID = rand()% 900+ 100;
            prevASync = execID;
        } else {
            execID = prevASync;
        }
        status  = "RUNNING";
    }

    char commandStringCopy[REPL_INPUT_LIMIT] = "";

    for(int j = 0; j < REPL_INPUT_LIMIT && replParsedInput[j] != NULL; j++) {
        strcat(commandStringCopy, replParsedInput[j]);
        strcat(commandStringCopy, " ");
    }

    struct processMetaData processMeta = {
        .processID = child_pid,
        .execId = execID,
        .execStatus = status
    };
    
    // copy the data and not the pointer
    strcpy(processMeta.command, commandStringCopy);

    execHistory[execHistoryLength++] = processMeta;

}

/*
    Executes the command provided by replArrayInput by forking a child and replacing it with exec. Also async flag decides wether REPL waits for child process to finish or not. 
*/
__pid_t replExec(char * replParsedInput[], bool async) {

    __pid_t child_pid = fork();

    if (child_pid == -1) {
        // Error in creating the fork
        // perror("P:Error in forking the process\n");
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


void listExecHistory (bool allFlag) { 
    printf("\nProcess history\n");
    for(int i =0; i< execHistoryLength; i++) {
        execHistory[i].execStatus = getStatus( execHistory[i]);
        if( allFlag || strcmp(execHistory[i].execStatus, "FINISHED") != 0) {
            
            printf("Command '%s' with PID %d and execID: %d status: %s\n",execHistory[i].command, execHistory[i].processID, execHistory[i].execId, execHistory[i].execStatus);
        }
    }
    // printf("Process history complete\n");
}

int searchHistoryByPid( __pid_t pid) {
    int result = -1;
    for(int i =0; i< execHistoryLength; i++) {
        if( pid == execHistory[i].processID) {
            result = i;
        }
    }
    return result;
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
            char * seperatedInput[REPL_MAX_TOKENS] = { NULL };
            int inputTokenLength = parseInput(replInput, seperatedInput, " \n");

            if (strncmp(seperatedInput[inputTokenLength - 1], REPL_ASYNC, 1) == 0) {
                async = true;
                seperatedInput[inputTokenLength - 1] = NULL;
                // printf("Async Mode On \n");
            }

            // Check special command

            if (strncmp(seperatedInput[0], REPL_EXIT, 4) == 0) {
                printf("Exiting the REPL! \n");
                exitREPL = true;
                exit(0);
            } else if (strncmp(seperatedInput[0], REPL_LIST_JOBS, 7) == 0) {
                listExecHistory(true);
            } else if (strncmp(seperatedInput[0], REPL_FOREGROUND, 2) == 0) {
                int wait_status;
                int pid = atoi(seperatedInput[1]);
                printf("Bringing process '%d' to forground", pid);
                __pid_t terminated_child_pid = waitpid( (__pid_t) pid, &wait_status, 0);
                if (terminated_child_pid == -1) {
                    perror("foreground");
                    exit(EXIT_FAILURE);
                }

                // Update history
                int metaDataIndex = searchHistoryByPid(pid);
                if( metaDataIndex != -1) {
                    execHistory[metaDataIndex].execStatus = "FINISHED";
                }

            } else {
                // #Execute
                replExec(seperatedInput, async);
            }

            // #Clean

            // listExecHistory(true);
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
// sh test.sh 10 2 &