#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <cstdio>
#include <signal.h>
#include <cstdlib>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

ssize_t recv_fd(int fd, void *data,size_t bytes, int* recvfd){
	struct msghdr recv;
	struct iovec iov[1];
	size_t n;

	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	}control_un;
	struct cmsghdr *p = NULL;
	recv.msg_control = control_un.control;
	recv.msg_controllen = sizeof(control_un.control);

	recv.msg_name = NULL;
	recv.msg_namelen = 0;

	iov[0].iov_base = data;
	iov[0].iov_len = bytes;
	recv.msg_iov = iov;
	recv.msg_iovlen = 1;

	if((n = recvmsg(fd,&recv,0)) < 0)
		return n;

	if((p = CMSG_FIRSTHDR(&recv)) != NULL && p->cmsg_len == CMSG_LEN(sizeof(int))){
		if(p->cmsg_level != SOL_SOCKET)
			;
		if(p->cmsg_type != SCM_RIGHTS)
			;
		*recvfd = *((int*)CMSG_DATA(p));
	}else
	*recvfd = -1;

	return n;
}

int my_open(const char* pathname, int mode){
	int fd, sockfd[2],status;
	pid_t childpid;
	char c, argsockfd[10], argmode[10];

	socketpair(AF_LOCAL,SOCK_STREAM,0,sockfd);// create a pair of no_name socket

	if((childpid = fork())==0){
		close(sockfd[0]);
		snprintf(argsockfd,sizeof(argsockfd),"%d",sockfd[1]);
		snprintf(argmode,sizeof(argmode),"%d",mode);

		execl("/home/xxx/openfile","openfile",argsockfd,pathname,argmode,(char*)NULL);//execve openfile
		//dont't never reach here
	}

	close(sockfd[1]);
	while(waitpid(childpid,&status,0) < 0)
		;

	recv_fd(sockfd[0],&c,1,&fd); // receive fd from openfile. "receive fd" don't mean receive fd number, but receive a pointer to the
								//  file structure in sender process space and install this file pointer to files_struct int this task_struct. 

	close(sockfd[0]);
	return fd;
}

int main(int argc, char** argv){
	int fd , n;
	char buf[256];

	if(argc != 2)
		;

	if((fd = my_open(argv[1],O_RDONLY)) < 0)
		;

	while((n = read(fd,buf,256)) > 0)
		write(1,buf,n);

	return 0;
}
