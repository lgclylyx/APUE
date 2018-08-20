#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include<pthread.h>

using std::string;

void* worker1(void* arg) {
	int fd = *((int*)arg);
	string str("aaaaaaaaaaaaaaaaa\n");
	for(int i = 0; i < 100; i++) {
		write(fd, str.c_str(), str.size());
	}
	return NULL;
}

void* worker2(void* arg) {
        int fd = *((int*)arg);
        string str("bbbbbbbbbbbbbbbbb\n");
        for(int i = 0; i < 100; i++) {
                write(fd, str.c_str(), str.size());
        }
	return NULL;
}

void* worker3(void* arg) {
        int fd = *((int*)arg);
        string str("ccccccccccccccccc\n");
        for(int i = 0; i < 100; i++) {
                write(fd, str.c_str(), str.size());
        }
	return NULL;
}


int main() {
	int fd = open("test.dat", O_CREAT|O_EXCL, 0666);
	close(fd);
	fd = open("test.dat", O_APPEND|O_WRONLY);
	if(fd == -1) {
		return 1;
	}
	
	pthread_t pids[3];	

	pthread_create(pids, NULL, worker1, &fd);
	pthread_create(pids+1, NULL, worker2, &fd);
	pthread_create(pids+2, NULL, worker3, &fd);
	
	for(int i =0; i < sizeof(pids)/sizeof(pthread_t); i++) {
		pthread_join(pids[i], NULL);
	}
	
	close(fd);
	return 0;
}
