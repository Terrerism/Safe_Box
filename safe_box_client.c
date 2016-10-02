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
#include <openssl/aes.h>
#include <openssl/md5.h>

#define COMMAND_BUFSIZE 1024 // input command buf size
#define READ_BUFSIZE 1000000 // Readable Max file size
#define O_BINARY 0x8000 // file open option
#define BLOCK_SIZE 4096 // optimized I/O block size
#define CRYPT_BLOCK 16 // Encrypt/Decrypt block size

typedef enum {true, false} bool;

void error_handling(char *message);
bool encrypt_block(unsigned char* cipherText, unsigned char* plainText, unsigned char *key);
bool decrypt_block(unsigned char* cipherText, unsigned char* plainText, unsigned char *key);

int main(int argc, char **argv)
{
	char command[4]; // command string
	char filename[100]; // filename string
	char filenamelen_string[4]; // filenamelen string
	char filelen_string[8]; // filelen string
	char passphrase[100]; // passphrase string
	int writelen = 0; // write size
	int readlen = 0; // read size
	int writelen_total = 0; // total write size
	int readlen_total = 0; // total read size
	int filenamelen = 0; // filenamelen int
	int filelen; // filelen int
	int inputfile; // file to read
	int outputfile; // file to write
	int i, j, k;
	unsigned char *plainbuf; // buf of plainbuf, size is CRYPT_BLOCK
	unsigned char *cipherbuf; // buf of cipherbuf, size is CRYPT_BLOCK
	unsigned char *plaintext; // readtext from file, writetext to file
	unsigned char *ciphertext; // sendtext to client / receivetext from client
	char temp; // temp char
	struct stat fileinfo; // fileinfo struct
	int lessblock; // less size
	int bufsize; // I/O block size

	unsigned char mdData[MD5_DIGEST_LENGTH+1]; // hash function output
	int sock;
	struct sockaddr_in serv_addr;

	if(argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
		error_handling("connect() error!");
	// TCP IP connection part
	
	
	// Inner process part
	printf("Usage : <command> <filename> <passphrase>\n");
	while(1) {
		// initialize
		writelen = 0;
		readlen = 0;
		writelen_total = 0;
		readlen_total = 0;
		temp = '\0';
		memset(command, '\0', strlen(command));
		memset(filename, '\0', strlen(filename));
		memset(filenamelen_string, '\0', strlen(filenamelen_string));
		memset(passphrase, '\0', strlen(passphrase));
		memset(filelen_string, '\0', strlen(filelen_string));
		memset(mdData, 0, MD5_DIGEST_LENGTH+1);

		// input command
		printf("input command(quit to q) : ");
		scanf("%s", command);

		// if command is q, exit program
		if(!strcmp(command, "q"))
			break;

		scanf("%s %s", filename, passphrase);

		// write start
		temp = write(sock, "$", 1);

		// write command
		writelen = write(sock, command, 3);
		
		// convert filename len type from int to char
		sprintf(filenamelen_string, "%03d", strlen(filename));
		filenamelen_string[strlen(filenamelen_string)] = '\0';

		// write filename len
		writelen = 0;
		writelen = write(sock, filenamelen_string, 3);

		// write filename
		writelen_total = 0;
		writelen = write(sock, filename, strlen(filename));

		// hash function
		MD5((unsigned char*)passphrase, strlen(passphrase), mdData);

		if(!strcmp(command, "put")) {
			// file open
			inputfile = open(filename, O_RDONLY | O_BINARY);
			if(inputfile == -1) {
				error_handling("invalid filename!");
			}
		
			fstat(inputfile, &fileinfo);
			// calculate ciphertext length
			// filelen is ciphertext length
			// fileinfo.st_size is plaintext length
			lessblock = fileinfo.st_size % CRYPT_BLOCK;
			filelen = fileinfo.st_size + CRYPT_BLOCK - lessblock;

			// malloc buf
			if((plaintext = malloc(fileinfo.st_size)) == NULL)
				error_handling("malloc() error!");
			if((ciphertext = malloc(filelen)) == NULL)
				error_handling("malloc() error!");
			if((plainbuf = malloc(CRYPT_BLOCK)) == NULL)
				error_handling("malloc() error!");
			if((cipherbuf = malloc(CRYPT_BLOCK)) == NULL)
				error_handling("malloc() error!");

			// convert filesize to string
			sprintf(filelen_string, "%07d", filelen);

			// send filesize
			writelen = 0;
			writelen = write(sock, filelen_string, 7);

			// file read
			readlen_total = 0;
			while(readlen_total != fileinfo.st_size) {
				readlen = read(inputfile, plaintext+readlen_total, BLOCK_SIZE);
				readlen_total += readlen;
			}

			// Encrypt file
			j = fileinfo.st_size;
			for(i=0 ; i<fileinfo.st_size; i+=CRYPT_BLOCK) {
				memset(plainbuf, '\0', CRYPT_BLOCK);
				memset(cipherbuf, '\0', CRYPT_BLOCK);
				if(j < CRYPT_BLOCK)
					memcpy(plainbuf, plaintext+i, j);
				else
					memcpy(plainbuf, plaintext+i, CRYPT_BLOCK);
				encrypt_block(cipherbuf, plainbuf, mdData);
				memcpy(ciphertext+i, cipherbuf, CRYPT_BLOCK);
				j -= CRYPT_BLOCK;
			}

			// file send to client
			printf("file send start.... ");
			bufsize = BLOCK_SIZE;
			while(filelen != 0) {
				if(filelen < BLOCK_SIZE);
					bufsize = filelen;
				writelen = write(sock, ciphertext, bufsize);
				filelen -= writelen;
			}
			printf("complete..!!\n");
			
			close(inputfile);
			if(plaintext != NULL)
			 	free(plaintext);
			if(ciphertext != NULL)
				free(ciphertext);
			if(plainbuf != NULL)
				free(plainbuf);
			if(cipherbuf != NULL)
				free(cipherbuf);
		}

		else if(!strcmp(command, "get")) {
			// receive initial char
			read(sock, &temp, 1);

			// server's filecount is zero
			if(temp == '0') {
				printf("no file in server!\n");
				continue;
			}

			// filename is unvalid
			else if(temp == 'n') {
				printf("unvalid file name (server dont't have file <%s>)\n", filename);
				continue;
			}
			else {
				// read filelen
				readlen = 0;
				readlen = read(sock, filelen_string, 7);
				filelen_string[7] = '\0';

				// convert filelen to int
				filelen = atoi(filelen_string);

				// malloc buf
				if((plaintext = malloc(filelen)) == NULL)
					error_handling("malloc() error");
				if((ciphertext = malloc(filelen)) == NULL)
					error_handling("malloc() error");
				if((plainbuf = malloc(filelen)) == NULL)
					error_handling("malloc() error");
				if((cipherbuf = malloc(filelen)) == NULL)
					error_handling("malloc() error");

				// open file
				outputfile = open(filename, O_CREAT | O_TRUNC | O_BINARY | O_WRONLY, 0666);
				if(outputfile == -1)
					error_handling("open() error");

				// receive file
				readlen_total = filelen;
				writelen_total = 0;
				bufsize = BLOCK_SIZE;
				while(readlen_total != 0) {
					if(readlen_total < BLOCK_SIZE)
						bufsize = readlen_total;

					readlen = read(sock, ciphertext+writelen_total, bufsize);
					if(readlen == 0)
						break;
					readlen_total -= readlen;
					writelen_total += readlen;

					readlen = 0;
				}

				// received file Decrypt
				printf("file Decrypt...");
				j = filelen;
				for(i=0 ; i<filelen; i+=CRYPT_BLOCK) {
					memset(plainbuf, '\0', CRYPT_BLOCK);
					memset(cipherbuf, '\0', CRYPT_BLOCK);
					memcpy(cipherbuf, ciphertext+i, CRYPT_BLOCK);
					decrypt_block(cipherbuf, plainbuf, mdData);
					if(j == 0) {
						lessblock = strlen(plainbuf);
						memcpy(plaintext+i, plainbuf, lessblock);
					}
					else
						memcpy(plaintext+i, plainbuf, CRYPT_BLOCK);
					j -= CRYPT_BLOCK;
				}
				printf("Complete!\n");

				filelen = filelen - CRYPT_BLOCK + lessblock;

				// write file
				bufsize = BLOCK_SIZE;
				writelen_total = 0;
				while(filelen != 0) {
					if(filelen < BLOCK_SIZE)
						bufsize = filelen;
					writelen = write(outputfile, plaintext+writelen_total, bufsize);
					filelen -= writelen;
					writelen_total += writelen;
				}

				printf("file receive complete..!!\n");

				close(outputfile);
				if(plaintext != NULL)
					free(plaintext);
				if(ciphertext != NULL)
					free(ciphertext);
				if(plainbuf != NULL)
					free(plainbuf);
				if(cipherbuf != NULL)
					free(cipherbuf);
			}
		}
		else {
			printf("Uncorrect command!!\nUsage : <command> <filename> <passphrase>\n");
		}
	}

	close(sock);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

bool encrypt_block(unsigned char* cipherText, unsigned char* plainText, unsigned char *key)
{
	AES_KEY encKey;

	if(AES_set_encrypt_key(key, 128, &encKey) < 0)
		return false;
	AES_encrypt(plainText, cipherText, &encKey);
	return true;
}

bool decrypt_block(unsigned char* cipherText, unsigned char* plainText, unsigned char *key)
{
	AES_KEY decKey;

	if(AES_set_decrypt_key(key, 128, &decKey) < 0)
		return false;
	AES_decrypt(cipherText, plainText, &decKey);
	return true;
}
