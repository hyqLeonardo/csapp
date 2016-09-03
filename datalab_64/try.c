#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>


#define WSIZE 		4
#define ALIGNMENT 	4
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE  (ALIGN(sizeof(size_t)))

#define PUT(p, val)	(*(unsigned int *)(p)) = (val)

#define GET_GROUP_LO(p1, p2)    (int)((p1 - p2)/WSIZE) + 4

int main() {

	int a = 1;
	int b = 2;
	int c = 3;

	void *ap = &a;
	void *bp = &b;
	void *cp = &c;

	printf("a is at %p\n", ap);
	printf("b is at %p\n", bp);
	printf("c is at %p\n", cp);

	printf("get_group_lo is %d\n", GET_GROUP_LO(cp, ap));
	return 0;
}