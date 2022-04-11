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
void pipe_command(char *argv[][MAXARGS], int count, int input_fd, int bg);

/* Signal handlers */
void SIGINThandler(int sig);
void SIGTSTPhandler(int sig);
void SIGCHLDhandler(int sig);

/* Backgroud variables & functions */
//Job status list
#define NONE		0
#define RUNF		1	//running in foreground 
#define RUNB		2	//running in background
#define SUSB		3	//suspended in background
#define DONE		4	//Done job
#define KILL		5	//Killed job

typedef struct job {
	pid_t pid;			//Process ID
	int id;				//Job ID
	char cmd[MAXLINE];	//Command
	int state;			//Job state

} job;
job jobList[MAXARGS];

void addjob(pid_t pid, char* cmd, int state);
void clearjob(int i);
void printjob();
void donejob();

int numJobs = 1;
int childpid = -1;
char tempcmd[MAXLINE];
/* Begin main */
int main() 
{
	for(int i = 0; i < MAXARGS; i++) {
		clearjob(i);
	}
	char *fgetsR;
	char cmdline[MAXLINE]; /* Command line */
	strcpy(path, "/bin/");
	strcpy(path2, "/usr/bin/");
	while (1) {
		signal(SIGTSTP, SIGTSTPhandler);
		signal(SIGINT, SIGINThandler);
		signal(SIGCHLD, SIGCHLDhandler);
		/* Read */

		printf("CSE4100-SP-P1 <%d>> ", numJobs);
		fgetsR = fgets(cmdline, MAXLINE, stdin);
		if (feof(stdin)) {
			exit(0);
		}
		/* Evaluate */
		eval(cmdline);
		donejob();

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
	int state;


	strcpy(buf, cmdline);

	if((pipe = strchr(buf, '|'))) {
		temp = buf;
		temp[strlen(temp)] = '|';
		while((pipe = strchr(temp, '|'))) {
			*pipe = '\0';
			strcpy(pcmd[pipe_num], temp);
			strcat(pcmd[pipe_num], "\n");
			pipe_num++;
			temp = pipe + 1;
		}
		pcmd[pipe_num - 1][strlen(pcmd[pipe_num - 1]) - 1] = '\0';
		pcmd[pipe_num][0] = '\0';
		for(int i = 0; pcmd[i][0] != '\0'; i++) {
			bg = parseline(pcmd[i], argv[i]);
			state = bg + 1;
		}
		argv[pipe_num][0] = NULL;
		if((pid = Fork()) == 0) {
			if(bg == 0) {
				signal(SIGINT, SIG_DFL);
				signal(SIGTSTP, SIGTSTPhandler);
			}
			else {
				childpid = getpid();
				signal(SIGINT, SIG_IGN);
				signal(SIGTSTP, SIG_IGN);
			}
			pipe_command(argv, 0, STDIN_FILENO, bg);
	
		}
		if(!bg) {
			childpid = pid;
			strcpy(tempcmd, cmdline);
			int status;
			if(waitpid(pid, &status, WUNTRACED) < 0) {
				unix_error("waitfg: waitpid error");
			}
			if(WIFEXITED(status)) {
				childpid = -1;
			}
		}
		else {
			printf("%d %s", pid, cmdline);
			addjob(pid, cmdline, state);
		}
	}
	else {
		bg = parseline(buf, argv[0]);
		state = bg + 1;
		if (argv[0][0] == NULL)  
			return;   /* Ignore empty lines */
		if (!builtin_command(argv[0])) { //quit -> exit(0), & -> ignore, other -> run
			if((pid = Fork()) == 0) {
				if(bg == 0) {
					signal(SIGINT, SIG_DFL);
					signal(SIGTSTP, SIGTSTPhandler);
				}
				else {
					childpid = getpid();
					signal(SIGINT, SIG_IGN);
					signal(SIGTSTP, SIG_IGN);
				}
				if((execve(strcat(path, argv[0][0]), argv[0], environ) < 0) && (execve(strcat(path2, argv[0][0]), argv[0], environ) < 0)) {	//ex) /bin/ls ls -al &
					printf("%s: Command not found.\n", argv[0][0]);
					exit(0);
				}
			}

			/* Parent waits for foreground job to terminate */
			if (!bg){
				childpid = pid;
				strcpy(tempcmd, cmdline);
				int status;
				if (waitpid(pid, &status, WUNTRACED) < 0) {
					unix_error("waitfg: waitpid error");
				}
				if(WIFEXITED(status)) {
					childpid = -1;
				}

			}
			else {//when there is backgrount process!
				printf("%d %s", pid, cmdline);
				addjob(pid, cmdline, state);
			}
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
	if(!strcmp(argv[0], "jobs")) {
		printjob();
		return 1;
	}
	if(!strcmp(argv[0], "kill")) {
		int jobId;
		int status;
		if(!argv[1]) {
			printf("Usage : kill %%job id\n");
			return 1;
		}
		if(argv[1][0] != '%') {
			printf("Usage : kill %%job id\n");
			return 1;
		}
		if(argv[1][1]) {
			jobId = atoi(&argv[1][1]);
			if(jobList[jobId].state == NONE) {
				printf("No Such Job\n");
				return 1;
			}
			else {
				Kill(jobList[jobId].pid, SIGKILL);
				jobList[jobId].state = KILL;
				return 1;
			}
		}
		else {
			printf("Usage : kill %%job id\n");
			return 1;
		}
	}
	if(!strcmp(argv[0], "bg")) {
		int jobId;
		int status;
		if(!argv[1]) {
			printf("Usage : bg %%job id\n");
			return 1;
		}
		if(argv[1][0] != '%') {
			printf("Usage : bg %%job id\n");
			return 1;
		}
		if(argv[1][1]) {
			jobId = atoi(&argv[1][1]);
			if(jobList[jobId].state == NONE || jobList[jobId].state == DONE || jobList[jobId].state == KILL) {
				printf("No Such Job\n");
				return 1;
			}
			else if(jobList[jobId].state == RUNB) {
				printf("Already Running\n");
				return 1;
			}
			else {
				printf("[%d]\trunning  \t%s", jobList[jobId].id, jobList[jobId].cmd);
				Kill(jobList[jobId].pid, SIGCONT);
				jobList[jobId].state = RUNB;
				return 1;
			}
		}
		return 1;
	}
	if(!strcmp(argv[0], "fg")) {
		int jobId;
		int status;
		if(!argv[1]) {
			printf("Usage : fg %%job id\n");
			return 1;
		}
		if(argv[1][0] != '%') {
			printf("Usage : fg %%job id\n");
			return 1;
		}
		if(argv[1][1]) {
			jobId = atoi(&argv[1][1]);
			if(jobList[jobId].state == NONE || jobList[jobId].state == DONE || jobList[jobId].state == KILL) {
				printf("No Such Job\n");
				return 1;
			}
			else if(jobList[jobId].state == RUNF) {
				printf("Already Running\n");
				return 1;
			}
			else if(jobList[jobId].state == SUSB || jobList[jobId].state == RUNB){
				Kill(jobList[jobId].pid, SIGCONT);
				jobList[jobId].state = RUNF;
				printf("[%d]\trunning  \t%s", jobList[jobId].id, jobList[jobId].cmd);
				while(1) {
					if(jobList[jobId].state != RUNF) {
						break;
					}
				}
				//return 1;
			}
		}
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
	char *quotes1;		 /* Points to the quotes  */
	char *quotes2;
	int argc;            /* Number of args */
	int bg;              /* Background job? */
	char temp;


	buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
	while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;
	/* Build the argv list */
	argc = 0;
	while ((delim = strchr(buf, ' '))) {
		if(*buf == '\"' || *buf == '\'') {
			temp = *buf;
			buf += 1;
			quotes1 = strchr(buf, temp);
			delim = quotes1;
		}
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
	else if(argv[argc - 1][strlen(argv[argc - 1]) - 1] == '&') {
		bg = 1;
		argv[argc - 1][strlen(argv[argc - 1]) - 1] = '\0';

	}
	return bg;

}
/* $end parseline */

void pipe_command(char *argv[][MAXARGS], int count, int input_fd, int bg) {
	int fd[2];
	pid_t pid;
	int pipe_ret;

	if(argv[count + 1][0] == NULL) {
		Dup2(input_fd, STDIN_FILENO);
		Close(input_fd);
		if(!builtin_command(argv[count])) {
			if((execve(strcat(path, argv[count][0]), argv[count], environ) < 0) && (execve(strcat(path2, argv[count][0]), argv[count], environ) < 0)) {
				printf("%s: Command not found.\n", argv[count][0]);
				exit(0);
			}
		}
		exit(0);
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

			if(!builtin_command(argv[count])) {
				if((execve(strcat(path, argv[count][0]), argv[count], environ) < 0) && (execve(strcat(path2, argv[count][0]), argv[count], environ) < 0)) {
					printf("%s: Command not found.\n", argv[count][0]);
					exit(0);
				}
			}
			exit(0);
		}

		int status;
		Close(fd[1]);
		pipe_command(argv, count + 1, fd[0], bg);
		if(waitpid(pid, &status, 0) < 0)
			unix_error("waitfg: waitpid error");
		while(1) {	//test
			printf("hi");
			if(WIFEXITED(status)) break;
		}
	}
}
void SIGINThandler(int sig) {
	printf("\nCSE4100-SP-P1> \n");
}

void SIGCHLDhandler(int sig) {
	pid_t pid;
	int status;
	while ((pid = waitpid(-1, &status, WNOHANG | WCONTINUED)) > 0) {
		if (WIFEXITED(status) || WIFSIGNALED(status)) {
			for(int i = 0; i < MAXARGS; i++) {
				if(jobList[i].pid == pid && jobList[i].state != NONE) {
					jobList[i].state = DONE;
					
				}
			}
		}
	}
}

void SIGTSTPhandler(int sig) {
	pid_t pid;
	int status;
	if(childpid == -1) return;
	kill(childpid, SIGSTOP);
	addjob(childpid, tempcmd, SUSB);
	

	childpid = -1;
}



void addjob(pid_t pid, char* cmd, int state) {
	jobList[numJobs].pid = pid;
	jobList[numJobs].id = numJobs;
	strcpy(jobList[numJobs].cmd, cmd);
	jobList[numJobs].state = state;
	numJobs++;
}

void clearjob(int i) {
	jobList[i].pid = 0;
	jobList[i].id = 0;
	jobList[i].cmd[0] = '\0';
	jobList[i].state = NONE;

}

void printjob() {
	for(int i = 0; i < MAXARGS; i++) {
		if(jobList[i].state == RUNB) {
			printf("[%d]\trunning  \t%s", jobList[i].id, jobList[i].cmd);
		}
		else if(jobList[i].state == SUSB) {
			printf("[%d]\tsuspended\t%s", jobList[i].id, jobList[i].cmd);
		}
	}
}

void donejob() {
	for(int i = 0; i < MAXARGS; i++) {
		if(jobList[i].state == DONE) {
			printf("[%d]\tDone     \t%s", jobList[i].id, jobList[i].cmd);
			jobList[i].state = NONE;
		}
		else if(jobList[i].state == KILL) {
			jobList[i].state = NONE;
		}
	}
	int count;
	for(count = MAXARGS - 1; count > 0; count--) {
		if(jobList[count].state == RUNF || jobList[count].state == RUNB || jobList[count].state == SUSB) {
			break;
		}
		numJobs = count;
	}
}
