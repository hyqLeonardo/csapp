#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

void handler(int sig) {
    printf("Stand up and strech!\n");
    alarm(3600);
}

int main() {
    signal(SIGALRM, handler);
    
    alarm(1800);
    while(1) {
	;
    }
    exit(0);
}
    
