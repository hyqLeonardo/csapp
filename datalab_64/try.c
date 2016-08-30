#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include <iostream>

#define ALIGNMENT 	4
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE  (ALIGN(sizeof(size_t)))

#define PUT(p, val)	(*(unsigned int *)(p)) = (val)

int main() {

	int size = 10;
	printf("size_t_size is %u\n", SIZE_T_SIZE);
	printf("aligned size is %u\n", ALIGN(size + SIZE_T_SIZE));
	printf("NULL is : %d\n", NULL);

	void *ptr = (void *)size;
	printf("ptr is %p, has length %d, ptr+4 is %p\n", ptr, sizeof(ptr), ptr+4);
	printf("\n");

	int a = 0;
	int data = 8;
	void *bp = &a;
	printf("generic pointer bp is at address %p\n", bp);
	printf("before PUT, int bp point at is %d\n", *(int *)(bp));
	printf("and the byte after a is %.2x\n", *((char *)(bp) + 4));
	PUT(bp, data);
	printf("after PUT, int bp point at is %d\n", *(int *)(bp));
	printf("and the byte after a is %.2x\n", *((char *)(bp) + 4));
	int i;

	for (i = 0; i < 4; i++) {
		printf("%.2x ", *((char *)(bp)+i));
	}
	printf("\n");

	// printf("content at bp is %s\n", *bp);


	return 0;
}