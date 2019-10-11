
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/wait.h>

#include<fcntl.h> 
#include <sys/stat.h>

#include <signal.h>

#define REPL_HISTORY_LIMIT 100

#define REPL_INPUT_LIMIT 100
#define REPL_MAX_TOKENS 15

#define REPL_ASYNC "&"
#define REPL_INPUT "<"
#define REPL_OUTPUT ">"
#define REPL_PIPE "|"

#define REPL_INPUT_ID 1
#define REPL_OUTPUT_ID 2
#define REPL_PIPE_ID 3

#define REPL_EXIT "exit"
#define REPL_LIST_JOBS "listjobs"
#define REPL_FOREGROUND "fg"
#define REPL_KILL "kill"

#define REPL_STDIN 0
#define REPL_STDOUT 1




bool TESTING_MODE = true;

int saved_stdout;
int saved_stdin;

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

int genExecId() {
    return  rand()% 900+ 100;
}
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

void childProcess(char * replParsedInput[], int *inputDescriptor, int inputDescriptorLength, int *outputDescriptor, int outputDescriptorLength){
    // Use execv

    fprintf(stdout, "child Process\n");
    // dup2(saved_stdout, 1);
    if ( inputDescriptorLength > 0) {
        /*
            changing Input to inputDescriptor
        */
        fprintf(stderr, "Input Descriptor is provided\n");
        
        close(0);   /* close normal stdin (fd = 0)*/
        if ( dup2(inputDescriptor[0],REPL_STDIN) < 0) {   /* make stdin same as inputDescriptor[0] */
            perror("P:Error in duping Input Descriptor(stdin) to provided descriptor\n");
            exit(EXIT_FAILURE);
        }
        // if(inputDescriptor[1]) {
            close(inputDescriptor[1]); /* we don't need the write end -- inputDescriptor[1]*/  
        // }
    } else {
        fprintf(stderr, "Input Descriptor to stdin\n");
        // dup2(saved_stdin, 0);
    }
    if ( outputDescriptorLength > 0 ) {
        /*
            changing Output to outputDescriptor
        */
        fprintf(stderr, "Output Descriptor is provided\n");
        
        close(1);  /* close normal stdout (fd = 1) */
        
        if ( dup2(outputDescriptor[1],REPL_STDOUT) < 0) {  /* make stdout same as outputDescriptor[1] */
            perror("P:Error in duping Output Descriptor (stdout) to provided descriptor\n");
            exit(EXIT_FAILURE);
        }
        // if(outputDescriptor[0]) {
            close(outputDescriptor[0]); /* we don't need the read end -- outputDescriptor[0] */

        // }
       
    } else {
        fprintf(stderr, "Output Descriptor to stdout\n");
        // dup2(saved_stdout, 1);
    }
    // dup2(saved_stdout, 1);
    // printf("child stdout check\n");
    // fflush(stdout);

    // for(int i = 0; i < 5; i++ ) {
    //     fprintf(stderr, "child executing %s \n",replParsedInput[i] );
    // }
    int exec_return_value = execvp(replParsedInput[0], replParsedInput);
    if (exec_return_value == -1) {
        perror("Error is executing command");
        exit(EXIT_FAILURE);
    } 
}

void parentProcess(char * replParsedInput[], __pid_t child_pid, bool async) {
    pid_t rc_pid;
    int execID;
    char * status = "RUNNING";
    // printf("P:Child process pid is : %d \n", child_pid);
    

    if (!async) {
        // printf("P:Async turned OFF waiting for child\n");
        int wait_status;
        __pid_t terminated_child_pid = waitpid( child_pid, &wait_status, 0);
        if (terminated_child_pid == -1) {
            status  = "Error";
            perror("wait");
            exit(EXIT_FAILURE);
        } else {
            status  = "FINISHED";
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
__pid_t replExec(char * replParsedInput[], bool async, int *inputDescriptor, int inputDescriptorLength, int *outputDescriptor, int outputDescriptorLength) {

    __pid_t child_pid = fork();

    if (child_pid == -1) {
        // Error in creating the fork
        perror("P:Error in forking the process\n");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        // Child process working 

        childProcess(replParsedInput, inputDescriptor, inputDescriptorLength, outputDescriptor, outputDescriptorLength);
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

int getIONo( char * inputChar ) {
    int returnStatus = -1;
    if ( strcmp(inputChar, REPL_INPUT) == 0) {
        returnStatus = REPL_INPUT_ID;
    } else if (strcmp(inputChar, REPL_OUTPUT) == 0 ) {
        returnStatus = REPL_OUTPUT_ID;
    } else if (strcmp(inputChar, REPL_PIPE) == 0) {
        returnStatus = REPL_PIPE_ID;
    }
    return returnStatus;
}

int checkIO( char ** inputArr, int length, int ** replInputIOOutputBuff) {
    int buffLength = 0;
    int prevIndex = -1;
    for ( int i = 0; i < length; i++ ) {
        int IONumber = getIONo(inputArr[i]);
        if ( IONumber != -1 ) {
            replInputIOOutputBuff[buffLength] = (int *) malloc( 3*sizeof(int));
            replInputIOOutputBuff[buffLength][0] = prevIndex;
            replInputIOOutputBuff[buffLength][1] = i;
            replInputIOOutputBuff[buffLength][2] = IONumber;
            prevIndex = i;
            buffLength++;
        }
    }
    // Adding End replIO
    replInputIOOutputBuff[buffLength] = (int *)malloc( 3*sizeof(int));
    replInputIOOutputBuff[buffLength][0] = prevIndex;
    replInputIOOutputBuff[buffLength][1] = length;
    replInputIOOutputBuff[buffLength][2] = -1;
    buffLength++;


    return buffLength;
}
void killProcess(__pid_t pid) {
    kill(pid, SIGKILL);
}
static volatile bool signal_ctrl_c = false;
void intHandler(int dummy) {
    printf("handler called");
    fflush(stdout);
    signal_ctrl_c = true;
}
/*
    Executes Read–eval–print loop
*/
void repl(void) {
    // Exit variable
    bool exitREPL = false;
    saved_stdout = dup(1);
    saved_stdin = dup(0);
    while (!exitREPL) {
        char text[] = "sh550 > ";
        char replInput[REPL_INPUT_LIMIT]= { '\0' } ;
        int * replInputIO[REPL_INPUT_LIMIT] = { NULL };
        // #Get input
        printf("%s", text);
        fgets(replInput, REPL_INPUT_LIMIT, stdin);

        if (strcmp(replInput, "\n")) {
        bool async = false;

            // Parse Input
            char * seperatedInput[REPL_MAX_TOKENS] = { NULL };
            int inputTokenLength = parseInput(replInput, seperatedInput, " \n");
            int replIOLength = checkIO(seperatedInput, inputTokenLength, replInputIO);
            if (strncmp(seperatedInput[inputTokenLength - 1], REPL_ASYNC, 1) == 0) {
                async = true;
                seperatedInput[inputTokenLength - 1] = NULL;
            }

            // Check special command

            if (strncmp(seperatedInput[0], REPL_EXIT, 4) == 0) {
                printf("Exiting the REPL! \n");
                exitREPL = true;
                exit(0);
            } else if (strncmp(seperatedInput[0], REPL_KILL, 3) == 0) {
                killProcess((__pid_t)atoi(seperatedInput[1]));
            } else if (strncmp(seperatedInput[0], REPL_LIST_JOBS, 7) == 0) {
                listExecHistory(true);
            } else if (strncmp(seperatedInput[0], REPL_FOREGROUND, 2) == 0) {
                int wait_status;
                int pid = atoi(seperatedInput[1]);
                printf("Bringing process '%d' to forground\n", pid);
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

            } else if ( replIOLength > 1) {
                // USed as Input
                int prevDescriptor[2];
                pipe(prevDescriptor);
                int prevDescriptorLength = 0;
                
                // To clear at the end , all the pipes used
                int * pipeArrayHistoy[2] = {prevDescriptor};
                int pipeArrayLength = 1;
                
                    int nextDescriptor[2];

                
                for( int i = 0; i< replIOLength && signal_ctrl_c == false; i++) {
                    // Used as Output
                    int nextDescriptorLength = 0;
                    char **tempReplEXec = (char**) malloc(REPL_INPUT_LIMIT * sizeof(char));
                    int prevIndexWithIO = replInputIO[i][0];
                    int nextIndexWithIO = replInputIO[i][1];
                    for( int j = 0; j < nextIndexWithIO - prevIndexWithIO -1; j++) {
                        tempReplEXec[j] = seperatedInput[j + prevIndexWithIO + 1]; 
                    }
                    nextDescriptorLength = 0;

                    /*
                        Creating a file descriptor to transfer data
                    */
                    if( replInputIO[i][2] == REPL_PIPE_ID) {
                        if ( i < replIOLength - 1 ) {
                            if (pipe(nextDescriptor) == -1) {
                                fprintf(stderr, "error with pipe %d", i);
                                perror("");
                                // exit(1);
                                break;
                            }
                            nextDescriptorLength = 2;
                            pipeArrayHistoy[pipeArrayLength++] =  nextDescriptor;

                        }
                    } else if( replInputIO[i][2] == REPL_INPUT_ID ){
                        prevDescriptor[0] = open( seperatedInput[nextIndexWithIO + 1], O_RDONLY);
                        if( prevDescriptor[0] < 0 ) {
                            fprintf(stderr, "error with read file descriptor for file %s and token :%d :",seperatedInput[nextIndexWithIO + 1], i);
                            perror("");
                            break;
                        }
                        prevDescriptorLength = 1;
                        pipeArrayHistoy[pipeArrayLength++] =  prevDescriptor;
                        i++;
                    } else if( replInputIO[i][2] == REPL_OUTPUT_ID ){
                        nextDescriptor[1] = open( seperatedInput[nextIndexWithIO + 1], O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR); // write only, if not create file, truncate previous data
                        nextDescriptor[0] = open( seperatedInput[nextIndexWithIO + 1], O_RDONLY);
                        if( nextDescriptor[1] < 0 ) {
                            fprintf(stderr, "error with write file descriptor for file %s and token :%d",seperatedInput[nextIndexWithIO + 1], i);
                            perror("");
                            break;
                        }
                        if( nextDescriptor[0] < 0 ) {
                            fprintf(stderr, "error with read file descriptor for file %s and token :%d :",seperatedInput[nextIndexWithIO + 1], i);
                            perror("");
                            break;
                        }
                        nextDescriptorLength = 1;
                        pipeArrayHistoy[pipeArrayLength++] =  nextDescriptor;
                        i++;
                    }
                             
                    replExec(tempReplEXec, false, prevDescriptor, prevDescriptorLength, nextDescriptor, nextDescriptorLength);

                    if ( dup2(nextDescriptor[0], prevDescriptor[0]) < 0)
                    {
                        fprintf(stderr, "error with nextDescriptor read %d", i);
                        perror(""); 
                        break;
                    }
                    close(nextDescriptor[1]); // closing write descriptor in parent as it is not required and race condition possible
                    prevDescriptorLength = nextDescriptorLength;
                    free(tempReplEXec);
                }
                // ls -la | grep c > o.txt | less
                // close unused pipes
                for(int i = 0; i < pipeArrayLength; i++ ) {
                    if( pipeArrayHistoy[i][0] > 0) {
                        close(pipeArrayHistoy[i][0]);
                    }
                    if(pipeArrayHistoy[i][1] > 0) {
                        close(pipeArrayHistoy[i][1]);
                    }
                }
            }
            else {
            // #Execute
                int intputDesc[2];
                int outputDesc[2];
                replExec(seperatedInput, async, intputDesc, 0,outputDesc, 0);
            }

            /*
                # Cleaning
            */
            
            // freeing replIO memory
            for( int i = 0; i < replIOLength; i++) {
                free(replInputIO[i]);           
            }

            // listExecHistory(true);
            // flush the input buffer            
            // while(getchar() != '\n');

        }
        if(signal_ctrl_c) {
            signal_ctrl_c = false;
        }

    }
    printf("Exiting REPL!");
}


int main(void) {
    signal(SIGINT, intHandler);

    printf("The shell is starting\n");

    // Start REPL 
    repl();
    return 0;
}

// sh test.sh 5 1 &
// sh test.sh 5 2 &
// sh test.sh 10 2 &
