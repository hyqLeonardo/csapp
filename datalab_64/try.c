#include <stdio.h>
#include <stdlib.h>
#include <string.h>
	
int main() {
	 char sentence[]="12 Rudolph ints years old";
	 char str[20];
	 int i;
	 int j;

	 int count = sscanf(sentence,"%d %d", &i, &j);
	 // printf ("%s -> %d\n",str,i);
	 printf("%d\n", count);


	return 0;
}