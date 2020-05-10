/**
 * Simple shell interface 
 * Mastafa Awal
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h> 
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  


#define MAX_LINE		 80 /* 80 chars per line, per command */
#define EXIT_WORD		 "exit\n"
#define HISTORY_KW		 "!!\n"
#define CONCURRENCY_CHAR '&'
#define RDRCT_IN_CHAR	 '<'
#define RDRCT_OUT_CHAR	 '>'
#define	PIPE_CHAR		 '|'

//function declarations
int run_args(char **, int, int, int);
int chg_dir(char *);
int run_piped(char **, int);
int exec_args(char **);

int main(void)
{
    char *args[MAX_LINE/2 + 1];	/* command line (of 80) has max of 40 arguments */
   
    char *input_buffer = NULL;
    char *input_line = NULL;
    char *prev_cmd = NULL;
    char *cwd;
    cwd = calloc (sizeof(char), 101);
    size_t max_len = MAX_LINE;

	int should_run = 1;
	
    while (should_run){
		//Allocate memory
		input_buffer = calloc(sizeof(char), max_len);
		input_line = calloc(sizeof(char), max_len);
		
		if (input_buffer == NULL || input_line == NULL) {
			perror("Memory allocation failed");
			exit(EXIT_FAILURE);
		}
		//Tried to print out cwd but for some reason only got null values/
        //printf("mysh:%s$ ", getcwd(cwd, sizeof(cwd)));
        
        printf("mysh:~$ ");
        getline(&input_buffer, &max_len, stdin);
           
        fflush(stdout);
		
		if (strcmp(EXIT_WORD, input_buffer) == 0) {
			//If the exit words match, then should_run is false and exit.
			//printf("Exiting..\n");
			should_run = 0;
			exit(0);
		} 
		int idx = 0;
		int concurrent = 0;
		int rdr_in_idx = 0;
		int rdr_out_idx = 0;
		int pipe_idx = 0;
		

		//Check if command is history command
		if (strcmp(HISTORY_KW, input_buffer) == 0) {
			if (prev_cmd == NULL) {
				fprintf(stderr, "No commands in History.\nAttempting to continue...");
				//Continue using "echo" as command -- this gave me a seg-fault on 2 consequetive for some reason, so not using it
				//strncpy(input_buffer, "echo", 5);
			} else {
				strcpy (input_buffer, prev_cmd);
			}
		} else {
			//Else, set the current command as the new previous command
			free(prev_cmd);
			prev_cmd = calloc (sizeof(char), max_len);
			strcpy (prev_cmd, input_buffer);
		}
			
		//Concat until newline character is found
		while (input_buffer[idx] != '\n') {
			//printf( " IB: idx: %d, char: %c \n", idx, input_buffer[idx]);
			idx++;
		}
		strncpy(input_line, input_buffer, idx);
			
		//Reset index for use with tokenization
		idx = 0;
		char *tok;
		const char delimiter[2] = " ";
		
		while ((tok = strsep(&input_line, delimiter)) != NULL) {
			//Skip empty argument values
			//printf("curr tok: %s\n", tok);
			//Check if token is '&' and for redirects
			switch(tok[0]) {
				case CONCURRENCY_CHAR: concurrent = 1; break;
				case RDRCT_IN_CHAR: rdr_in_idx = idx; break; 
				case RDRCT_OUT_CHAR: rdr_out_idx = idx; break; 
				case PIPE_CHAR: pipe_idx = idx; break; 
			}
			if (tok[0] != '\0' && tok[0] != CONCURRENCY_CHAR) {
				args[idx] = tok;
				idx++;
			}
		}
		//Add the NULL terminator
		args[idx] = NULL;

	
		if (pipe_idx == 0) {
			//if both redirection indexes are greater than 0, then print a warning, as the shell is not coded to support it properly
			if (rdr_in_idx > 0 && rdr_out_idx > 0) {
				printf("WARNING: Both input and output redirection detected. This may cause unexpected results. Attempting to continue..");
			}
			//printf("calling run args..\n");
			run_args(args, concurrent, rdr_in_idx, rdr_out_idx);
		} else {
			run_piped(args, pipe_idx);
		}
		//Free memory 
		free(input_buffer);
		free(input_line);
		}
		
  		free(prev_cmd);
		return 0;
}
 
//Method to execute a shell 
int run_args (char** input_args, int concurrent, int rdr_in_idx, int rdr_out_idx) {

	pid_t child;
	child = fork();
	int status;
	int fd;
	int cfd;
	char **args;
	char *rdr_in_arg;
	char *rdr_out_arg;
	
	args = calloc(sizeof(char), MAX_LINE);

	if (child < 0) {
		perror("Error forking");
	} else if (child == 0) {
		
		//If concurrent, then set the child process into a new process group
		if (concurrent) {
			printf("Running in background...");
			setpgid(0,0);
		}
		
		if (rdr_in_idx == 0 && rdr_out_idx == 0) {
			args = input_args;
			exec_args(args);
		}
		
		if (rdr_in_idx > 0) {
			rdr_in_arg = calloc(sizeof(char), MAX_LINE);
			//Copy the redirection argument
			rdr_in_arg = *(input_args + rdr_in_idx + 1);
			
			//Copy the exec arguments, which appears before the redirection argument
			int idx = 0;
			while (idx != rdr_in_idx) {
				//printf("Adding %s to arg array", input_args[idx]);
				args[idx] = input_args[idx];
				idx++;
			} 
			//Add null terminator
			args[idx] = NULL;
			
			//Create file descriptors
			printf("Opening descriptors for rdrin");
			
			fd = open(rdr_in_arg, O_RDONLY);
			
			if (fd < 0) {
				perror("Error opening File Descriptor");
			}	
			
			cfd = dup2(fd, 0);
			
			if (cfd < 0) {
				perror("Error creating copy of file descriptor");
			} 
			
			exec_args(args);
			close (fd);
		} 
		
		if (rdr_out_idx > 0) {
			rdr_out_arg = calloc(sizeof(char), sizeof(input_args));
			//Copy the redirection argument
			rdr_out_arg = *(input_args + rdr_out_idx + 1);
			//printf("Redirect out with cmd %s..\n", rdr_out_arg);
			
			//Copy the exec arguments, which appears before the redirection argument
			int idx = 0;
			while (idx != rdr_out_idx) {
				printf("Adding %s to arg array", input_args[idx]);
				args[idx] = input_args[idx];
				idx++;
			} 
			//Add null terminator
			args[idx] = NULL;
		
			//Create file descriptors
			fd = creat(rdr_out_arg, O_WRONLY);
			if (fd < 0) {
				perror("Error opening File Descriptor");
			}	
			cfd = dup2(fd, 1);
			if (cfd < 0) {
				perror("Error creating copy of file descriptor");
			} 
			exec_args(args);
			close (fd);
			} 
		 } else {
		
			if (concurrent == 1) {
				//Don't wait for it to finish
			} else {
				//If not concurrent, then wait for child to finish
				waitpid(child, &status, 0);
				fflush(stdout);
		}
	}
	
	return 0;
}

int run_piped(char** args_array, int pipe_idx) {
	
	char **arg1;
	char **arg2;
	int p[2];
	int idx;
	int status1;
	int status2;
	pid_t child1;
	pid_t child2;

	arg1 = calloc(sizeof(char), MAX_LINE);
	arg2 = calloc(sizeof(char), MAX_LINE);	

	//Get first argument
	for (idx = 0; idx < pipe_idx; idx++) {
		//strcpy(arg1[idx], args_array[idx]);
		arg1[idx] = args_array[idx];
	}
	//Append Null character
	arg1[idx] = NULL;
	
	//Get second argument
	
	idx = pipe_idx + 1;
	
	while (args_array[idx] != NULL) {
		arg2[idx - pipe_idx - 1] = args_array[idx];
		idx++;
	}
	//Append Null character
	arg2[idx] = NULL;
	
	//Create pipe
	if (pipe(p) < 0) {
		perror("Creation of pipe failed");
	}
	
	child1 = fork();
	
	if (child1 < 0) {
		perror("Creation of child failed");
	} 
	
	if (child1 == 0) {
		//First child process - process the first command
		//Close the write end
		close(p[0]);
		dup2(p[1], STDOUT_FILENO);
		exec_args(arg1);
		close(p[1]);
		
	} else {
		//Parent process
		//Wait for child1 to complete
		close(p[1]);
		waitpid(child1, &status1, 0);
		//Create another child
		child2 = fork();
		if (child2 < 0) {
			perror("Creation of child failed");
		}
		if (child2 == 0) {
			//Second child process -- process the second command
			close(p[1]);
			dup2(p[0], STDIN_FILENO);
			exec_args(arg2);
			close(p[0]);
		} else {
			//Close pipe
			close (p[0]);
			close (p[1]);
		}
		//Wait for child2 to exit
		waitpid(child2, &status2, 0); 
		close(p[0]);
	}
		
	return 0;
}

int chg_dir(char* args) {
	if (chdir(args) < 0) {
		perror("change directory failed");
		return 1;
	}
	return 0;
}

int exec_args (char **args) {
		//Check if the first arg is cd - if it is, invoke chg_dir
		if (strcmp(args[0], "cd") == 0) {
			//exit(0);
			chg_dir(args[1]);
			return 1;
		} else if (execvp(args[0], args) < 0) {
			perror("Error executing command");
			return 1;
		}
		return 0;
}
