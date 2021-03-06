/*
* Main source code file for lsh shell program
*
* You are free to add functions to this file.
* If you want to add functions in a separate file
* you will need to modify Makefile to compile
* your additional functions.
*
* Add appropriate comments in your code to make it
* easier for us while grading your assignment.
*
* Submit the entire lab1 folder as a tar archive (.tgz).
* Command to create submission archive:
    $> tar cvf lab1.tgz lab1/
*
* All the best
*/


//TODO ignore signal SIGINT
#include <stdio.h>

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/*
* Function declarations
*/

void test(Pgm *); //Remove
void sigintHandler(int);
void sigchldHandler(int);
int Run_pipe(Command *);
void EvaluateCommando(Pgm *, Command *);
char* Get_path(char *); //Remove?
void Redirections(Command *);
void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

//Make a list of foreground PIDs to kill when ctrl+c is pressed 

//pid of main process
pid_t main_pid;

/*
* Name: main
*
* Description: Gets the ball rolling...
*
*/

int main(void)
{
    Command cmd;
    int n;

    main_pid = getpid();

    signal(SIGINT, sigintHandler);

    while (!done) {
        //signal(SIGCHLD, sigchldHandler);
        char *line;
        line = readline("> ");

        if (!line) {
            /* Encountered EOF at top level */
            done = 1;
        }
        else {
            /*
            * Remove leading and trailing whitespace from the line
            * Then, if there is anything left, add it to the history list
            * and execute it.
            */
            stripwhite(line);

            if(*line) {
                add_history(line);
                n = parse(line, &cmd);
                Run_pipe(&cmd);
            }
        }
        if(line) {
            free(line);
        }
    }
    return 0;
}

void sigchldHandler(int signo) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { //Wait for any child without blocking
        //printf("PID of terminated child %d\n", pid);
    }
    return;
  
}

void sigintHandler(int signo) { //Change
    //kill foreground processes 
    if (getppid() == main_pid) {
       exit(0);
    }
    return;
}

void
EvaluateCommando(Pgm *current_pgm, Command *cmd)
{
    int nmb_args = 0;

    while (*((current_pgm->pgmlist)+(nmb_args + 1)) != NULL) {
        nmb_args++;
    }

    char *prog1[nmb_args+2];
    memset( prog1, 0, (nmb_args+2)*sizeof(char*) );
    char *one = *(current_pgm->pgmlist);
    *(one+strlen(one)) = '\0';
    prog1[0] = one;

    //loop to add the args
    for (int j = 1; j<=nmb_args; j++) {
        char *two = *((current_pgm->pgmlist)+j);
        *(two+strlen(two)) = '\0';
        prog1[j] = two;
    }
    prog1[nmb_args+1] = 0;
    execvp((prog1[0]), prog1);
        perror("execvp failed");
        exit(1);

}

void Redirections (Command *currentCmd) {
  if (currentCmd->rstdout!=NULL) { //check if we have to redirect in an file
    int fdOut=creat(currentCmd->rstdout, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
    dup2(fdOut, 1); //the "1" is the f. descriptor for the output stream of the program
    close(fdOut);
  }
  if (currentCmd->rstdin!=NULL) { //check if we have to take an input from a file
    int fdIn=open(currentCmd->rstdin, 0);
    dup2(fdIn, 0); // "0" is the f. descriptor for input stream
    close(fdIn);
  }
}

int Run_pipe(Command *cmd)
{
    int pid_chld; //pid of first child

    int nmb_pipes = 0;
    Pgm *current = cmd->pgm;

    //Check if special commands exit or cd
    char com_exit[5] = "exit";
    char *ce = &(com_exit[0]);
    if (strcmp(*(cmd->pgm->pgmlist), ce) == 0) {
        exit(0); //Do we need to think about something else here, like killing background processes?
    }
    //Check if cd command 
    char com_cd[5] = "cd";
    char *cc = &(com_cd[0]);
    if (strcmp(*(cmd->pgm->pgmlist), cc) == 0) {
        char *newPathRelative = *((current->pgmlist)+ 1); //get 
        if (chdir(newPathRelative) < 0) {
            printf("Could not change directory to %s", newPathRelative);
        }
        return 0;
    }

    //Check how many pipes necessary and if legal commands, if not return
    while (current->next != NULL){
        nmb_pipes++;
        current = current->next;
    }
    int fds[nmb_pipes+1][2]; //fd for read and write for all future processes, starting from the first process
    memset( fds, 0, (nmb_pipes+1)*2*sizeof(int) );

    if (cmd->bakground == 1) { //if in background
        signal(SIGCHLD, sigchldHandler);
    } else {
        //printf("Sighandler set to not in background\n");
        //signal(SIGCHLD, SIG_DFL); //Set to default and make parent wait instead, change SIGINT 
    }

    if (nmb_pipes == 0) {

        int pid = fork();

        if (pid < 0) { 
            perror("Fork failed");
            exit(1);
        }

        if (pid == 0) { //If child

            if (cmd->bakground == 1) { //if in background
                setpgid(0,0);
            } 
            Pgm *current_pgm = cmd->pgm;
            Redirections(cmd); //Change input and/or output source if specified
            EvaluateCommando(current_pgm, cmd); //Execute command
        } 
        else { //If parent
            if (!(cmd->bakground == 1)) { //if not in background
                wait(NULL); //Blocking wait for parent if process not set to background, or enough to put child in foreground?
            } 
        }
    }
    else { //Create pipes and execute processes
      
        fds[0][0] = STDIN_FILENO; //first one should read from standard input
        fds[nmb_pipes][1] = STDOUT_FILENO; //Last one should write to standard output

        for (int i = 0; i<nmb_pipes; i++) { //Create all the necessary pipes
            int fd[2];
            if (pipe(fd) == -1) {
                fprintf(stderr, "Pipe failed");
                return -1;
            }
            fds[i][1] = fd[1]; //write to this pipe
            fds[i+1][0] = fd[0]; //read from this pipe
        }

        for (int i = 0; i <= nmb_pipes; i++) { //Let main process fork a child for each command
            int pid = fork();
            if (pid == -1) {
                perror("Fork failed");
                exit(1);
            }
            
            if(pid == 0) {
                if (cmd->bakground == 1) { //if in background
                    setpgid(0,0); //Change
                } 
                
                Pgm *current_pgm = cmd->pgm;
                for (int j = nmb_pipes; (j-i)>0; j-- ) { //Make current_pgm the the command to be executed
                    current_pgm = current_pgm->next;
                }

                Redirections(cmd);

                if (fds[i][1] != STDOUT_FILENO ) {
                    close(STDOUT_FILENO);  //closing stdout
                    dup2(fds[i][1], STDOUT_FILENO); //replacing stdout with pipe write
                    close(fds[i][1]);
                }
                if (fds[i][0] != STDIN_FILENO) {
                    close(STDIN_FILENO);  //closing stdin
                    dup2(fds[i][0], STDIN_FILENO); //replacing stdin with pipe read
                    close(fds[i][0]);
                }

                //close pipes not used
                for (int k = 0; k<i; k++) { //for lower
                    if (fds[k][0] != STDIN_FILENO) {
                        close(fds[k][0]);
                    }
                    close(fds[k][1]);
                }
                for (int l = i+1; l<=nmb_pipes; l++) { //for higher
                    close(fds[l][0]);
                    if (fds[l][1] != STDOUT_FILENO) {
                        close(fds[l][1]);
                    }
                }
                EvaluateCommando(current_pgm, cmd);
            } else { //if parent

            }
        }
    }
    //Close parents pipes
    if (nmb_pipes>0) {

        for (int m = 1; m<nmb_pipes; m++) {
            close(fds[m][0]);
            close(fds[m][1]);
        }
        close(fds[0][1]); //close write of first pipe
        close(fds[nmb_pipes][0]); //close read from last pipe
    }

    //Wait for all children if in foreground
    if (!(cmd->bakground)) { //If not in background
        for (int i = 0; i<= nmb_pipes; i++) { //Wait for all children
            wait(NULL);
        }
    }
    return 0;
}

//Just to check if we're extracting the right things, only for debugging
void test(Pgm *pgm) {
    const Pgm *const current_pgm = pgm;
    printf("%s %s %lu %lu\n", *(current_pgm->pgmlist), *((current_pgm->pgmlist)+1), sizeof(*(current_pgm->pgmlist)+0), strlen(*(current_pgm->pgmlist)+0));

    char *one = *(current_pgm->pgmlist);
    *(one+strlen(one)) = '\0';
    char *two = *((current_pgm->pgmlist)+1);
    *(two+strlen(two)) = '\0';
    char *const prog1[3] = {one, two, 0};
}

/*
* Name: PrintCommand
*
* Description: Prints a Command structure as returned by parse on stdout.
*
*/

void
PrintCommand (int n, Command *cmd)
{
    printf("Parse returned %d:\n", n);
    printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );  //If stdin set
    printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" ); //If stdout set
    printf("   bg    : %s\n", cmd->bakground ? "yes" : "no"); //Ending command with &
    PrintPgm(cmd->pgm); //Commands entered
} //?: operator = condition true? -> evaluate 1st, otherwise evanuate 2nd


char* Get_path(char *com) {
    char path[6] = "/bin/";
    size_t total_len = strlen(com) + strlen(path) + 1; //strlen doesn't count the \0
    char search_path[total_len];
    char *start = &search_path[0];
    strncpy(start, path, total_len);
    strncat(start, com, total_len);
    return start;
}


/*
* Name: PrintPgm
*
* Description: Prints a list of Pgm:s
*
*/
void
PrintPgm (Pgm *p)
{
    if (p == NULL) {
        return;
    }
    else {
        char **pl = p->pgmlist;

        /* The list is in reversed order so print
        * it reversed to get right
        */
        PrintPgm(p->next);
        printf("    [");
        while (*pl) {
            printf("%s ", *pl++);
        }
        printf("]\n");
    }
}

/*
* Name: stripwhite
*
* Description: Strip whitespace from the start and end of STRING.
*/
void
stripwhite (char *string)
{
    register int i = 0;

    while (isspace( string[i] )) {
        i++;
    }

    if (i) {
        strcpy (string, string + i);
    }

    i = strlen( string ) - 1;
    while (i> 0 && isspace (string[i])) {
        i--;
    }

    string [++i] = '\0';
}
