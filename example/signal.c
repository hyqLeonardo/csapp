#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#define MAXBUF   8192  /* Max I/O buffer size */


void unix_error(char *message) {
    perror(message);
    exit(0);
}

int Fork() {
    pid_t pid;

    if ((pid = fork()) < 0) 
	unix_error("fork error");
    else
	return pid;
}

void handler(int sig) {
    pid_t pid;
    
    if ((pid = waitpid(-1, NULL, 0)) > 0) { 
        printf("handler reaped child %d\n", (int)pid);
    }
    if (errno != ECHILD) 
	unix_error("waitpid error"); 
 
    sleep(2);
    return;
}

int main(int argc, char *argv[]) {
    int i, n;
    char buf[MAXBUF];

    if (signal(SIGCHLD, handler) == SIG_ERR)
	unix_error("signal error");
    
    /* parents create children */
    int num = atoi(argv[1]);
    for (i = 0; i < num; i++) {
	if (Fork() == 0) {
	    printf("Hello from child %d of group %d\n", (int)getpid(), (int)getpgrp());
	    sleep(1);
	    exit(0);
	}
    }

    /* parents wait for terminal input and then process it */
    /* manually restart the read call if it is interrupted */
    while ((n = read(0, buf, sizeof(buf))) < 0)
	if (errno != EINTR)
	    unix_error("read error");

    printf("parent processing input\n");
    while(1)
	;
    exit(0);
}
