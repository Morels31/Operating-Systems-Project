
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h> 
#include <poll.h>
#include <ctype.h>
#include <sys/wait.h>
#include <crypt.h>
#include <termios.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define printNow(s) { printf("%s", s); fflush(stdout); }


//the two types types of records/dynArr
#define MAIN_TYPE 0
#define USER_TYPE 1

#define RAND_STR_FULL_CHARSET "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-_<'>?/#&@+-=()[]{}"
#define NAME_CHARSET " '"
#define USERNAME_CHARSET "-_"
#define PASSWORD_CHARSET "-_<'>?/#&@+-=()[]{}"
#define HASH_CHARSET "./"

#define SINGLE_NUM_SEPARATOR ','
#define KEY_VALUE_SEPARATOR ':'
#define QUERY_ITEMS_SEPARATOR ';'

#define MAX_GENERIC_LEN 100
#define MAX_NAME_LEN MAX_GENERIC_LEN
#define MAX_NUM_LEN 14
#define MAX_N_NUMS 10
#define MAX_USERNAME_LEN MAX_GENERIC_LEN
#define MIN_PASSWORD_LEN 7
#define MAX_PASSWORD_LEN MAX_GENERIC_LEN
#define HASH_LEN 86

#define MAX_NUMS_LEN ( (MAX_NUM_LEN + 1) * MAX_N_NUMS - 1 )
#define MAX_MAIN_REC_STR_LEN ( MAX_NAME_LEN + 1 + MAX_NUMS_LEN )
#define MAX_USER_REC_STR_LEN ( MAX_USERNAME_LEN + 1 + HASH_LEN )
#define MAX_REC_STR_LEN ( MAX_MAIN_REC_STR_LEN > MAX_USER_REC_STR_LEN ? MAX_MAIN_REC_STR_LEN : MAX_USER_REC_STR_LEN )

#define BUFF_SIZE 4096
#define MIN_BUFF_SIZE ( MAX_REC_STR_LEN + 2 )

#define SESSION_TOKEN_LEN 80
#define DEFAULT_SERVER_PORT 34334
#define DEFAULT_SERVER_IP "127.0.0.1"



enum userPermissions{
	NO_PERM = '0',
	READ_PERM = '1',
	READ_WRITE_PERM = '2',
	TOT_PERM
};

enum requestType{
	TOKEN_REQ = '0',
	SEARCH_REQ = '1',
	ADD_REQ = '2',
	DEL_REQ = '3',
	TOT_REQ
};

enum responseType{
	SUCCESS_RESP = '0',
	FAIL_RESP = '1',
	INV_REQ_RESP = '2',
	INV_USERNAME_RESP = '3',
	INV_PASSWORD_RESP = '4',
	TOO_MANY_TRY_RESP = '5',
	TOT_RESP
};


//utility.c
void clearStdin(void);
char *readLine(char *askStr, char *errStr, int maxLen, char *optionalDest, size_t *optionalTotChars);
char *randomString(size_t length, char *optionalDest);
int isFileFinished(int fd);
int readLineFromFile(int fd, char *buff, char **p1, char **p2);
int checkGenericString(char *str, const char *charset, size_t maxSize);
int checkNameString(char *name);
int checkNumString(char *num);
int checkNumsString(char *nums);
int checkUsernameString(char *username);
int checkPasswordString(char *psw);
int checkHashString(char *hash);
int checkTokenString(char *token);
int checkRecordString(char *str, unsigned char recType);
void formatNameString(char *name);
char *readNameString(char *optionalDest, size_t *optionalTotChars);
char *readNumsString(char *optionalDest, size_t *optionalTotChars);
char *readUsernameString(char *optionalDest, size_t *optionalTotChars);
char *hash(char *str, char *optionalDest);
char *readPassword(char *optionalDest);
size_t readMainRecordString(char *dest);
size_t readUserRecordString(char *dest);
unsigned long countFileLines(char *filename);
int writeToSocket(char *str, size_t len, int sockFd);
size_t readFromSocket(char *dest, int sockFd);
