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

ssize_t send_fd(int fd,void *data,size_t bytes,int sendfd){
	struct msghdr sender; //prepare a msghdr structure to store message and send it
	struct iovec iov[1];
	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	}control_un; // the control message in msghdr structure
	struct cmsghdr *p = NULL;
	sender.msg_control = control_un.control;
	sender.msg_controllen = sizeof(control_un.control);

	p = CMSG_FIRSTHDR(&sender);
	p->cmsg_len = CMSG_LEN(sizeof(int));
	p->cmsg_level = SOL_SOCKET;
	p->cmsg_type = SCM_RIGHTS;
	*((int*)CMSG_DATA(p)) = sendfd;

	sender.msg_name = NULL;
	sender.msg_namelen = 0;
	iov[0].iov_base = data;
	iov[0].iov_len = bytes;
	sender.msg_iov = iov;
	sender.msg_iovlen = 1;
	return sendmsg(fd,&sender,0);
}

int main(int argc,char* argv[]){
	int fd;
	ssize_t n;
	if(argc != 4)
		return -1;
	if((fd = open(argv[2],atoi(argv[3]))) < 0)
		return -1;
	if((n = send_fd(atoi(argv[1]),(void*)"",1,fd))< 0)
		return -1;
	return 0;
}
