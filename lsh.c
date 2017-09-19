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


#include <stdio.h>

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <stdbool.h>
#include <errno.h>

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

void test(Pgm *);
int Run_pipe2(Command *);
void EvaluateCommando(Pgm *, Command *);
//bool Execute_if_exist(char *, char *);
void Execute_if_exist(char *const prog[], Command *);
char* Get_path(char *);
int Run_pipe(Pgm *);
int Create_pipes_and_run(int, Pgm *);
void Redirections(Command *);
void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

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

    while (!done) {

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
                //PrintCommand(n, &cmd);

                Run_pipe2(&cmd);



            }
        }

        if(line) {
            free(line);
        }
    }
    return 0;
}

/*
* Name: PrintCommand
*
* Description: Prints a Command structure as returned by parse on stdout.
*
*/

void
EvaluateCommando(Pgm *current_pgm, Command *cmd)
{
    int nmb_args = 0;

    while (*((current_pgm->pgmlist)+(nmb_args + 1)) != NULL) {
        nmb_args++;
    }
    printf("nmb_args: %d\n", nmb_args);

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
    //printf("prog1: %s, prog1[1] %s\n", *prog1, prog1[1]);

        Execute_if_exist(prog1, cmd);

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

void
Execute_if_exist (char *const prog[], Command *cmd) { //TBI
    //printf("prog[0] = %s\n", Get_path(prog[0]));
    //if (access(Get_path(prog[0]), F_OK)+1) {
        execvp((prog[0]), prog);
        perror("execvp failed");
        exit(1);
    // } else {
    //     perror("Command isn't recognised");
    //     exit(1);
    // }

    //Old way of doing it
    // size_t total_len = strlen(com) + strlen(path) + 1; //strlen doesn't count the \0
    // char search_path[total_len];
    // char *start = &search_path[0];
    // strncpy(start, path, total_len);
    // strncat(start, com, total_len);
    // printf("%s\n", search_path);
    // //char *start = Get_path(com);
    // //char search_path[strlen(com) + strlen(path) + 1] = *start;
    // int ret = 0;
    // if (access(start, F_OK)+1) {
    //     int pid = fork();
    //     if (pid == 0) { //child
    //         ret = execlp(search_path, com, arg ,(char *) NULL); //Path, name
    //     } else { //parent
    //         wait(NULL);
    //         //printf(">");
    //     }
    // } else {
    //     return false;
    // }

    // //printf("execlp: %d\n",ret);
    // return true;
}

/*char* Get_path(char *com) {
    size_t total_len = strlen(com) + strlen(path) + 1; //strlen doesn't count the \0
    char search_path[total_len];
    char *start = &search_path[0];
    strncpy(start, path, total_len);
    strncat(start, com, total_len);
    return start;
}*/



int Run_pipe2(Command *cmd)
{
    // int des_p[2];
    // if(pipe(des_p) == -1) {
    //     perror("Pipe failed");
    //     exit(1);
    // }

    int nmb_pipes = 0;
    Pgm *current = cmd->pgm;
    //printf("Current: %s\n", *(current->next->next->pgmlist));
    while (current->next != NULL){
        nmb_pipes++;
        current = current->next;
    }
    //printf("Nmb pipes: %d\n", nmb_pipes);
    int fds[nmb_pipes+1][2]; //fd for read and write for all future processes, starting from the first process
    memset( fds, 0, (nmb_pipes+1)*2*sizeof(int) );

    if (nmb_pipes == 0) {

        //Check if exit typed
        char com_exit[5] = "exit";
        char *ce = &(com_exit[0]);
        if (strcmp(*(cmd->pgm->pgmlist), ce) == 0) {
            exit(0); //Do we need to think about something else here?
        }
        int pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        if (pid == 0) { //Doesn't work if arg is NULL, check for nmb of args
            Pgm *current_pgm = cmd->pgm;
            Redirections(cmd);
            EvaluateCommando(current_pgm, cmd);
        }
        wait(NULL);
    }
    else
    { //Create pipes and execute processes

        fds[0][0] = STDIN_FILENO; //first one should read from standard input
        fds[nmb_pipes][1] = STDOUT_FILENO; //Last one should write to standard output

        for (int i = 0; i<nmb_pipes; i++) {
            int fd[2];
            //*(fds+i) = fd;
            if (pipe(fd) == -1) {
                fprintf(stderr, "Pipe failed");
                return -1;
            }
            fds[i][1] = fd[1]; //write to this pipe
            fds[i+1][0] = fd[0]; //read from this pipe
        }

        for (int i = 0; i <= nmb_pipes; i++) { //Fixa med nya pipes
            int pid = fork();
            if (pid == -1) {
                perror("Fork failed");
                exit(1);
            }
            if(pid == 0)
            {
                Pgm *current_pgm = cmd->pgm;
                for (int j = nmb_pipes; (j-i)>0; j-- ) { //Make current_pgm the the command to be executed
                    current_pgm = current_pgm->next;
                }

                char *one = *(current_pgm->pgmlist);
                *(one+strlen(one)) = '\0';
                char *two = *((current_pgm->pgmlist)+1);
                *(two+strlen(two)) = '\0';
                char *const prog1[3] = {one, two, 0};
                //printf("i = %d, fds = [%d %d], one = %s, two = %s\n", i, fds[i][0], fds[i][1], one, two);
                Redirections(cmd);
                //Fixa med i
                if (fds[i][1] != STDOUT_FILENO ) {
                    //printf("Closing stdout: i = %d, fds = %d\n", i, fds[i][1]);
                    close(STDOUT_FILENO);  //closing stdout
                    dup2(fds[i][1], STDOUT_FILENO); //replacing stdout with pipe write
                    close(fds[i][1]);
                }
                if (fds[i][0] != STDIN_FILENO) {
                    //printf("Closing stdin: i = %d, fds = %d\n", i, fds[i][0]);
                    close(STDIN_FILENO);  //closing stdin
                    dup2(fds[i][0], STDIN_FILENO); //replacing stdin with pipe read
                    close(fds[i][0]);
                }

                //close pipes not used
                for (int k = 0; k<i; k++) { //for lower
                    //printf("Closing: i = %d, fds[k,0] = %d, fds[k,1] = %d\n", i, fds[k][0], fds[k][1]);
                    if (fds[k][0] != STDIN_FILENO) {
                        close(fds[k][0]);
                    }
                    close(fds[k][1]);
                }
                for (int l = i+1; l<=nmb_pipes; l++) { //for higher
                    //printf("Closing: i = %d, fds[k,0] = %d, fds[k,1] = %d\n", i, fds[l][0], fds[l][1]);
                    close(fds[l][0]);
                    if (fds[l][1] != STDOUT_FILENO) {
                        close(fds[l][1]);
                    }
                }
                EvaluateCommando(current_pgm, cmd);
            }
        }
    }

    if (nmb_pipes>0) {

        for (int m = 1; m<nmb_pipes; m++) {
            close(fds[m][0]);
            close(fds[m][1]);
        }
        close(fds[0][1]); //close write of first pipe
        close(fds[nmb_pipes][0]); //close read from last pipe
    }

    //Wait for all children
    for (int i = 0; i<= nmb_pipes; i++) {
        wait(0);
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
    char *const prog1[3] = {one, two, 0}; //{*(current_pgm->pgmlist), *((current_pgm->pgmlist)+1), 0 };

    printf("one: %lu %lu \n", sizeof(one), strlen(one));
    printf("print %s, %s\n", (prog1[0]), (prog1[1]));
}

void
PrintCommand (int n, Command *cmd)
{
    printf("Parse returned %d:\n", n);
    printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );  //If stdin set
    printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" ); //If stdout set
    printf("   bg    : %s\n", cmd->bakground ? "yes" : "no"); //Ending command with &
    PrintPgm(cmd->pgm); //Commands entered
} //?: operator = condition true? -> evaluate 1st, otherwise evanuate 2nd

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
