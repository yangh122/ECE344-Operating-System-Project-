#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


int *wait_que;
int count = 0;

static void check_error(int ret, const char *message) {
    if (ret != -1) {
        return;
    }
    int err = errno;
    perror(message);
    exit(err);
}

int main(int argc, char *argv[]){
    if (argc < 2) {
        return EINVAL;
    }

    
    wait_que = malloc(sizeof(int)*(argc));

    for (int i = 1; i < argc; i++){
		int in_pipefd[2] = {0};
    	check_error(pipe(in_pipefd), "pipe");
	
		pid_t pid = fork();
    	check_error(pid, "child");


		if(i == argc - 1){
			if (pid > 0){
				check_error(close(in_pipefd[0]), "parent_in[0]");
				check_error(close(in_pipefd[1]), "parent_in[1]"); 
				wait_que[i-1] = pid;
				count++;
			}
			else{
				check_error(close(in_pipefd[0]), "parent_in[0]");        
				check_error(close(in_pipefd[1]), "parent_in[1]");
				check_error(execlp(argv[i], argv[i], NULL), "execlp");

			}			

		}
		else{
			if (pid > 0){
				check_error(dup2(in_pipefd[0], STDIN_FILENO), "dup2_parent");
				check_error(close(in_pipefd[0]), "child_in[0]");
				check_error(close(in_pipefd[1]), "child_in[1]"); 
				wait_que[i-1] = pid;
				count++;
        	}
			else{
			 
				check_error(dup2(in_pipefd[1], STDOUT_FILENO), "dup2_child"); 
				check_error(close(in_pipefd[0]), "child_in[0]");        
				check_error(close(in_pipefd[1]), "child_in[1]");
				check_error(execlp(argv[i], argv[i], NULL), "execlp");
			}
		}

    }



    for (int j = 0; j < count; j++){
        int wstatus;
        int wait_pid = wait_que[j];
        check_error(waitpid(wait_pid, &wstatus, 0), "wait");
        assert(WIFEXITED(wstatus));
        int exit_status = WEXITSTATUS(wstatus);
        if (exit_status != 0){
			return WEXITSTATUS(wstatus);
		}
    }
    
    return 0;
}