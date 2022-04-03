/* $begin shellmain */
//#include "csapp.h"
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128
char path[MAXLINE];		/* Default command path : /bin/ */
char path2[MAXLINE];	/* Default command path : /usr/bin */
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void pipe_command(char *argv[][MAXARGS], int pos, int input_fd);

int main() 
{
	char cmdline[MAXLINE]; /* Command line */
	strcpy(path, "/bin/");
	strcpy(path2, "/usr/bin/");
	while (1) {
		/* Read */
		printf("CSE4100-SP-P1> ");                   
		fgets(cmdline, MAXLINE, stdin); 
		if (feof(stdin)) {
			exit(0);
		}
		/* Evaluate */
		eval(cmdline);
		printf("eval end\n");
	}
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
	char *argv[MAXARGS][MAXARGS]; /* Argument list execve() */
	char buf[MAXLINE];   /* Holds modified command line */
	int bg;              /* Should the job run in bg or fg? */
	char *pipe;			 /* Points to the pipe character '|' */
	int pipe_num = 0;		 /* Number of pipe in command */
	char *temp;			 /* Pointer for parsing the command by '|' */
	pid_t pid;           /* Process id */
	char pcmd[MAXARGS][MAXLINE]; /* command line parsed for commands with pipe */

	strcpy(buf, cmdline);

	if((pipe = strchr(buf, '|'))) {
		temp = buf;
		temp[strlen(temp)] = '|';
		while((pipe = strchr(temp, '|'))) {
			*pipe = '\0';
			strcpy(pcmd[pipe_num], temp);
			strcat(pcmd[pipe_num], "\0");
			pipe_num++;
			temp = pipe + 1;
		}
		pcmd[pipe_num][0] = '\0';
		for(int i = 0; pcmd[i][0] != '\0'; i++) {
			bg = parseline(pcmd[i], argv[i]);
		}
		argv[pipe_num][0] = NULL;
		printf("%s / %s\n", argv[0][0], argv[0][1]);
		pipe_command(argv, 0, STDIN_FILENO);

	}
	else {
		bg = parseline(buf, argv[0]); 
		if (argv[0][0] == NULL)  
			return;   /* Ignore empty lines */
		if (!builtin_command(argv[0])) { //quit -> exit(0), & -> ignore, other -> run
			if((pid = Fork()) == 0) {
				if(execve(strcat(path, argv[0][0]), argv[0], environ) < 0 && execve(strcat(path2, argv[0][0]), argv[0], environ) < 0) {	//ex) /bin/ls ls -al &
					printf("%s: Command not found.\n", argv[0][0]);
					exit(0);
				}
			}

			/* Parent waits for foreground job to terminate */
			if (!bg){ 
				int status;
				if (waitpid(pid, &status, 0) < 0)
					unix_error("watfg: waitpid error");
			}
			else//when there is backgrount process!
				printf("%d %s", pid, cmdline);
		}
	}
	return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
	if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit")) /* quit command */
		exit(0);  
	if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 1;
	if(!strcmp(argv[0], "cd")) {	/* Change directory */
		if(chdir(argv[1]) < 0) printf("cd : No such file or directory\n");
		return 1;
	}
	return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
	char *delim;         /* Points to first space delimiter */
	int argc;            /* Number of args */
	int bg;              /* Background job? */

	buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
	while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;
	/* Build the argv list */
	argc = 0;
	while ((delim = strchr(buf, ' '))) {
		argv[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ' ')) /* Ignore spaces */
			buf++;
	}
	argv[argc] = NULL;

	if (argc == 0)  /* Ignore blank line */
		return 1;

	/* Should the job run in the background? */
	if ((bg = (*argv[argc-1] == '&')) != 0)
		argv[--argc] = NULL;

	return bg;
	
}
/* $end parseline */

void pipe_command(char *argv[][MAXARGS], int pos, int input_fd) {
	int fd[2];
	pid_t pid;
	int pipe_ret;

	if(argv[pos + 1][0] == NULL) {
		Dup2(input_fd, STDIN_FILENO);
		Close(input_fd);
		if(!builtin_command(argv[pos])) {
			if(execve(strcat(path, argv[pos][0]), argv[pos], environ) < 0 && execve(strcat(path2, argv[pos][0]), argv[pos], environ)) {
				printf("%s: Command not found.\n", argv[pos][0]);
				exit(0);
			}
		}
		//exit(0);
	}
	else {
		pipe_ret = pipe(fd);
		if(pipe_ret < 0) {
			printf("pipe error\n");
			exit(0);
		}
		if((pid = Fork()) == 0) {
			Close(fd[0]);
			if(input_fd != STDIN_FILENO) {
				Dup2(input_fd, STDIN_FILENO);
				Close(input_fd);
			}
			Dup2(fd[1], STDOUT_FILENO);
			Close(fd[1]);
			
			if(!builtin_command(argv[pos])) {
				if(execve(strcat(path, argv[pos][0]), argv[pos], environ) < 0 && execve(strcat(path2, argv[pos][0]), argv[pos], environ) < 0) {
					printf("%s: Command not found.\n", argv[pos][0]);
					exit(0);
				}
			}
			//exit(0);
		}
		
		int status;
		Close(fd[1]);
		pipe_command(argv, pos + 1, fd[0]);
		if(waitpid(pid, &status, 0) < 0)
			unix_error("watfg: waitpid error");
	}
}
