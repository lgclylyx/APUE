/*
int pipe(int pipefd[2]);
*/

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>


int main(){
	pid_t child_b, child_c;
	char* argvb[] = {"/bin/cat",NULL};
	char* argvc[] = {"/usr/bin/ls","-l",NULL};
	int fd[2];
	pipe(fd);

	if((child_b = fork()) != 0){
		close(fd[1]);
		dup2(fd[0],0);
		close(fd[0]);
		execve("/usr/bin/cat",argvb,NULL);
	}

	close(fd[0]);

	if((child_c = fork()) != 0){
		dup2(fd[1],1);
		close(fd[1]);
		execve("/bin/ls",argvc,NULL);
	}
	close(fd[1]);
	waitpid(child_b,NULL,0);
	return 0;
}
