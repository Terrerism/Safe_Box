#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define FILE_BUFSIZE 1000000 // Max file size
#define O_BINARY 0x8000 // file open option
#define BLOCK_SIZE 4096 // optimized I/O block size

// storage struct
typedef struct storage_type {
	char *filename; // filename
	char *buf; // I/O buf
	int filelen; // filelen
} storage;

void error_handling(char *message);

int main(int argc, char **argv)
{
	int serv_sock;
	int clnt_sock;
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	int clnt_addr_size;

	char startletter; // start initial
	char command[4]; // command string
	char filenamelen_string[4]; // filename size string
	char filelen_string[8]; // file size string
	char filename[100]; // filename string
	int writelen = 0; // write size
	int readlen = 0; // read size
	int writelen_total = 0; // total write size
	int readlen_total = 0; // total read size
	int filenamelen; // file name size
	int filelen; // file size
	int i, j;
	int outputfile; // write fd
	int inputfile; // read fd
	struct stat fileinfo; // file info
	int filecount = 0; // server stored file count
	storage server_storage[10]; // server disk
	int bufsize; // optimized I/O block size

	if(argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind() error");

	if(listen(serv_sock, 5) == -1)
		error_handling("listen() error");

	clnt_addr_size = sizeof(clnt_addr);
	clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_addr, &clnt_addr_size);
	if(clnt_sock == -1)
		error_handling("accept() error");
	// TCP connect part

	// read start
	while(read(clnt_sock, &startletter, 1) != 0) {
		// initialize
		writelen = 0;
		readlen = 0;
		writelen_total = 0;
		readlen_total = 0;
		memset(command, '\0', strlen(command));
		memset(filenamelen_string, '\0', strlen(filenamelen_string));
		memset(filelen_string, '\0', strlen(filelen_string));
		memset(filename, '\0', strlen(filename));

		// read command
		readlen = read(clnt_sock, command, 3);
		command[3] = '\0';

		// read filename len
		readlen = 0;
		readlen = read(clnt_sock, filenamelen_string, 3);
		filenamelen_string[3] = '\0';

		// convert filenamelen to int
		filenamelen = atoi(filenamelen_string);

		// read filename
		readlen = 0;
		readlen = read(clnt_sock, filename, filenamelen);
		filename[filenamelen] = '\0';
		printf("read file name from client is <%s>\n", filename);

		if(!strcmp(command, "put")) {
			printf("command is put\n");
			// read filelen
			readlen = 0;
			readlen = read(clnt_sock, filelen_string, 7);
			filelen_string[7] = '\0';
	
			// convert filelen to int
			filelen = atoi(filelen_string);
			server_storage[filecount].filelen = filelen;

			// malloc buf
			server_storage[filecount].buf = malloc(4096);
			if(server_storage[filecount].buf == NULL)
				error_handling("malloc() error");

			// open file
			outputfile = open(filename, O_CREAT | O_TRUNC | O_BINARY | O_WRONLY, 0666);
			if(outputfile == -1)
				error_handling("open() error");

			// file receive and write
			printf("file receive start.... ");
			bufsize = BLOCK_SIZE;
			while(filelen != 0) {
				if(filelen < BLOCK_SIZE)
					bufsize = filelen;

				readlen = read(clnt_sock, server_storage[filecount].buf, bufsize);
				if(readlen == 0)
					break;
				filelen -= readlen;

				writelen = write(outputfile, server_storage[filecount].buf, readlen);

				readlen = 0;
			}
			printf("complete..!!\n");
			
			close(outputfile);

			// update server file list
			server_storage[filecount].filename = malloc(filenamelen);
			strncpy(server_storage[filecount].filename, filename, filenamelen);
			server_storage[filecount].filename[filenamelen] = '\0';
			filecount++;
			printf("stored file count : %d\n", filecount);
			printf("--------------Server Stored File List ---------------\n");
			for(i=0 ; i<filecount ; i++) 
				printf("%d. filename : %s / filesize : %d\n", i+1, server_storage[i].filename, server_storage[i].filelen);
			printf("-----------------------------------------------------\n");
		}

		else if(!strcmp(command, "get")) {
			printf("command is get\n");
			if(filecount == 0) {
				// server's file count is 0, no file
				write(clnt_sock, "0", 1);
				continue;
			}
			j = 1;
			for(i=0 ; i<filecount ; i++) {
				j = strcmp(filename, server_storage[i].filename);
				if(j == 0)
					break;
			}
			if(j != 0) {
				// input filename is unvalid
				write(clnt_sock, "n", 1);
				continue;
			}

			write(clnt_sock, "k", 1);

			inputfile = open(filename, O_RDONLY | O_BINARY);
			if(inputfile ==-1)
				error_handling("open() error!");

			// convert filesize to string
			sprintf(filelen_string, "%07d", server_storage[i].filelen);
			filelen_string[strlen(filelen_string)] = '\0';

			// send filesize
			writelen = 0;
			writelen = write(clnt_sock, filelen_string, 7);

			// file read and send
			printf("file send start... ");
			bufsize = BLOCK_SIZE;
			filelen = server_storage[i].filelen;
			while(filelen != 0) {
				if(filelen < BLOCK_SIZE)
					bufsize = filelen;

				readlen = read(inputfile, server_storage[i].buf, bufsize);
				if(readlen == 0)
					break;
				filelen -= readlen;
				writelen = write(clnt_sock, server_storage[i].buf, bufsize);

				readlen = 0;
			}

			printf("complete...!!\n");
			close(inputfile);
		}
		else {}
	}
	close(clnt_sock);

	// memory free
	for(i=0 ; i<filecount ; i++) {
		if(server_storage[i].buf != NULL)
			free(server_storage[i].buf);
		if(server_storage[i].filename != NULL)
			free(server_storage[i].filename);
	}

	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
