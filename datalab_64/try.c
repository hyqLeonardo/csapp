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

int main() {
	size_t size = 1024;
	char *file = "/folder";
	char cwd[size];

	getcwd(cwd, size);

	char path[strlen(file) + strlen(cwd) + 1];
	strcpy(path, cwd);
	strcat(path, file);
	printf("%s\n", path);

	// int open_fd;
	// if ((open_fd = open(path, O_RDONLY)) < 0) {
	// 	perror("open file error");
	// 	exit(1);
	// }

	// struct stat buffer;
	// int status;	

	// if (status = stat(path, &buffer) == -1) {
	// 	printf("no such file\n");
	// 	return 1;
	// }

	// char data_buf[buffer.st_size];
	// if (read(open_fd, data_buf, buffer.st_size) < 0) {
	// 	perror("read file error");
	// 	exit(1);
	// }

	// printf("%s\n", data_buf);

	// printf("%d\n", S_ISREG(buffer.st_mode));

	// DIR *open_dir;
	// if ((open_dir = opendir(path)) == NULL) {
	// 	perror("open directory error");
	// 	exit(1);
	// }
	// struct dirent *read_dir;
	// int i = 0;
	// while ((read_dir = readdir(open_dir)) != NULL) {
	// 	printf("%s\n", read_dir->d_name);
	// }
	// closedir(open_dir);

	// char *(str[5]);
	// (*str)[0] = 'a';
	// (*str)[1] = 'b';
	// (*str)[2] = 'c';
	// (*str)[3] = '\0';
	// printf("print str as a pointer : %p\n", str);
	// printf("print first char of str : %c\n", **str);
	// printf("print the address of first char : 0x%x\n", *str);

	char *host_name = "bing.com";
	struct hostent *host_buf;
	struct in_addr **addr_list;
	int i;

	if ((host_buf = gethostbyname(host_name)) == NULL) {
		perror("gethostbyname error");
		return 1;
	}

	addr_list = (struct in_addr **)host_buf->h_addr_list;

	printf("Official name is : %s\n", host_buf->h_name);
	for (i = 0; addr_list[i] != NULL; i++) {
		printf("%s\n", inet_ntoa(*addr_list[0]));	
	}
	printf("\n");


	return 0;
}