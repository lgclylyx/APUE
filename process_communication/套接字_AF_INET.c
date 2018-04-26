#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <assert.h>
#include <semaphore.h>

typedef struct pingm_pakcet{
	struct timeval tv_begin;	/*发送的时间*/
	struct timeval tv_end;		/*接收到的时间，not used*/
	short seq;					/*序列号*/
	int flag;		/*1，表示已经发送但没有接收到回应包0，表示接收到回应包*/
}pingm_pakcet;
static pingm_pakcet pingpacket[128];
static pingm_pakcet *icmp_findpacket(int seq);
static unsigned short icmp_cksum(unsigned char *data,  int len);
static struct timeval icmp_tvsub(struct timeval end,struct timeval begin);
static void icmp_statistics(char* ip_addr);
static void icmp_pack(struct icmp *icmph, int seq, struct timeval *tv, int length );
static int icmp_unpack(char *buf,int len);
static void *icmp_recv(void *argv);
static void *icmp_send(void *argv);
static void icmp_sigint(int signo);
static void icmp_usage();
#define K 1024
#define BUFFERSIZE 72					/*发送缓冲区大小*/
static char send_buff[BUFFERSIZE];
static char recv_buff[2*K];	/*为防止接收溢出，接收缓冲区设置大一些*/
static struct sockaddr_in dest;		/*目的地址*/
static int rawsock = 0;					/*发送和接收线程需要的socket描述符*/
static pid_t pid=0;						/*进程PID*/
static int alive = 0;					/*是否接收到退出信号*/
static short packet_send = 0;			/*已经发送的数据包有多少*/
static short packet_recv = 0;			/*已经接收的数据包有多少*/
static struct timeval tv_begin, tv_end,tv_interval;/*全局的消耗时间*/
static sem_t lock;						/*对多线程共享变量的互斥，实际有没有互斥都是一样的，只是想将互斥加进去*/

int main(int argc, char** argv){
	if(argc < 2){
		icmp_usage();
		exit(0);
	}
	int recv_size = 128  * K;
	struct protoent *protocol = NULL;

	pid = getuid(); /*填充ICMP报文中的id域*/

	sem_init(&lock,0,1);/*初始化信号量*/

	protocol = getprotobyname("icmp");
	assert(protocol != NULL);           /*获取协议*/

	rawsock = socket(AF_INET,SOCK_RAW,protocol->p_proto);
	assert(rawsock >= 0);				/*创建原始套接字*/

	setsockopt(rawsock,SOL_SOCKET,SO_RCVBUF,&recv_size,sizeof(recv_size));/*设置接收缓冲区大小*/

	bzero(&dest,sizeof(dest));
	unsigned int inaddr = inet_addr(argv[1]); /*通过DNS或直接给出的方式找到对应的IP地址*/
	if(inaddr == INADDR_NONE){
		struct hostent *host = gethostbyname(argv[1]);
		assert(host != NULL);
		memcpy((char*)&dest.sin_addr.s_addr,host->h_addr_list[0],host->h_length);
	}else
		memcpy((char*)&dest.sin_addr.s_addr,&inaddr,sizeof(inaddr));
	dest.sin_family = AF_INET;
	inaddr = dest.sin_addr.s_addr;
	printf("PING %s (%ld.%ld.%ld.%ld) 56(84) bytes of data.\n",
			argv[1],
			(inaddr&0x000000FF)>>0,
			(inaddr&0x0000FF00)>>8,
			(inaddr&0x00FF0000)>>16,
			(inaddr&0xFF000000)>>24);

	signal(SIGINT,icmp_sigint);/*信号处理*/

	memset(pingpacket,0,sizeof(pingm_pakcet)*128);

	alive = 1;
	pthread_t send_id, recv_id;

	int err = 0;/*创建send && recv线程用于发送和接收*/
	err = pthread_create(&send_id,NULL,icmp_send,NULL);
	assert(err >= 0);
	err = pthread_create(&recv_id,NULL,icmp_recv,NULL);
	assert(err >= 0);

	pthread_join(send_id,NULL);
	pthread_join(recv_id,NULL);

	close(rawsock);
	icmp_statistics(argv[1]);
	return 0;
}

void icmp_usage(){
	printf("ping www.xxx.com/127.0.0.1\n");
}

void icmp_sigint(int signo){
	sem_wait(&lock);
	alive = 0;
	sem_post(&lock);

	gettimeofday(&tv_end,NULL);
	tv_interval = icmp_tvsub(tv_end,tv_begin);

	sem_wait(&lock);
	if(packet_send > packet_recv)
		packet_send--;
	sem_post(&lock);
}

void *icmp_send(void *argv){
	gettimeofday(&tv_begin,NULL);
	while(alive){
		int size = 0;
		struct timeval tv;/*not used*/

		pingm_pakcet * packet= icmp_findpacket(-1);
		if(packet){/*将发送的报文在本地做记录*/
			packet->seq = packet_send;
			packet->flag = 1;
			gettimeofday(&packet->tv_begin,NULL);
		}
		icmp_pack((struct icmp*)send_buff,packet_send,&tv,64);/*打包要发送的ICMP报文*/
		size = sendto(rawsock,send_buff,64,0,(struct sockaddr *)&dest,sizeof(dest));
		if(size < 0)
			continue;

		sem_wait(&lock);
		packet_send++;
		sem_post(&lock);

		sleep(1);
	}
	return NULL;
}

pingm_pakcet *icmp_findpacket(int seq)
{
	int i=0;
	pingm_pakcet *found = NULL;
	if(seq == -1){
		for(i = 0;i<128;i++){
			if(pingpacket[i].flag == 0){
				found = &pingpacket[i];
				break;
			}
		}
	}else if(seq >= 0){
		for(i = 0;i<128;i++){
			if(pingpacket[i].seq == seq){
				found = &pingpacket[i];
				break;
			}
		}
	}
	return found;
}

void icmp_pack(struct icmp *icmph, int seq, struct timeval *tv, int length ){
	icmph->icmp_type = ICMP_ECHO;
	icmph->icmp_code = 0;
	icmph->icmp_cksum = 0;
	icmph->icmp_seq = seq;
	icmph->icmp_id = pid & 0xFFFF;/*填充ICMP报文中的控制项*/
	for(char i = 0; i < length; i++){
		icmph->icmp_data[i] = i;/*填充数据*/
	}
	icmph->icmp_cksum = icmp_cksum((unsigned char *)icmph,length);/*计算校验和*/
}

unsigned short icmp_cksum(unsigned char *data,  int len)
{
	int sum=0;							/*计算结果*/
	int odd = len & 0x01;					/*是否为奇数*/
	/*将数据按照2字节为单位累加起来*/
	while( len & 0xfffe)  {
		sum += *(unsigned short*)data;
		data += 2;
		len -=2;
	}
	/*判断是否为奇数个数据，若ICMP报头为奇数个字节，会剩下最后一字节*/
	if( odd) {
		unsigned short tmp = ((*data)<<8)&0xff00;
		sum += tmp;
	}
	sum = (sum >>16) + (sum & 0xffff);	/*高低位相加*/
	sum += (sum >>16) ;					/*将溢出位加入*/

	return ~sum; 							/*返回取反值*/
}

void *icmp_recv(void *argv){
	struct timeval tv;
	tv.tv_usec = 200;
	tv.tv_sec = 0;
	fd_set readfd;
	while(alive){
		int ret = 0;
		FD_ZERO(&readfd);
		FD_SET(rawsock,&readfd);
		ret = select(rawsock+1, &readfd,NULL,NULL,&tv);/*使用select等待I/O*/
		if(ret <= 0)
			continue;
		int size = recv(rawsock,recv_buff,sizeof(recv_buff),0);
		if(errno == EINTR)/*被信号唤醒，一般不会*/
			continue;
		icmp_unpack(recv_buff,size);/*处理接收到的包*/
	}
	return NULL;
}

int icmp_unpack(char *buf,int len){
	struct ip *ip = (struct ip*)buf;/*SOCK_RAW套接字如果不设置SOCKETOPT中的IP_HDRINCL,将会不包含IP头*/
	int iphdrlen = ip->ip_hl*4;
	struct icmp *icmph = (struct icmp*)(buf + iphdrlen);/*跳过IP头，指向ICMP头*/

	len = len - iphdrlen;
	if(len < 8)
		return -1;

	if((icmph->icmp_type == ICMP_ECHOREPLY) && (icmph->icmp_id == pid)){/*验证是否是发往本进程的ICMP_ECHOREPLY包*/
		struct timeval tv_begin,tv_end,tv_interval;
		pingm_pakcet* packet = icmp_findpacket(icmph->icmp_seq);/*找到该包的发送时间*/
		if(packet == NULL)
			return -1;
		tv_begin = packet->tv_begin;
		gettimeofday(&tv_end,NULL);/*获取当前的时间*/
		tv_interval = icmp_tvsub(tv_end,tv_begin);/*计算时延*/
		int rtt = tv_interval.tv_sec*1000+tv_interval.tv_usec/1000;
		printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%d ms\n",
			len,
			inet_ntoa(ip->ip_src),
			icmph->icmp_seq,
			ip->ip_ttl,
			rtt);

		sem_wait(&lock);
		packet_recv++;
		sem_post(&lock);

		packet->flag = 0;
	}else
		return -1;
	return 0;
}

struct timeval icmp_tvsub(struct timeval end,struct timeval begin){
	struct timeval tv;
	/*计算差值*/
	tv.tv_sec = end.tv_sec - begin.tv_sec;
	tv.tv_usec = end.tv_usec - begin.tv_usec;
	/*如果接收时间的usec值小于发送时的usec值，从usec域借位*/
	if(tv.tv_usec < 0){
		tv.tv_sec --;
		tv.tv_usec += 1000000;
	}
	return tv;
}

void icmp_statistics(char* ip_addr){
	long time = (tv_interval.tv_sec * 1000 )+ (tv_interval.tv_usec/1000);
	printf("--- %s ping statistics ---\n",ip_addr);	/*目的IP地址*/
	printf("%d packets transmitted, %d received, %d%% packet loss, time %ldms\n",
		packet_send,									/*发送*/
		packet_recv,  									/*接收*/
		(packet_send-packet_recv)*100/packet_send, 	/*丢失百分比*/
		time);
}

