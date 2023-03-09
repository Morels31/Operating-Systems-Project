/*
 *  Elenco Telefonico
 * 
 *    Realizzazione di un servizio "elenco telefonico" supportato da un server che
 *    gestisce sequenzialmente o in concorrenza (a scelta) le richieste dei client
 *    (residenti in generale su macchine diverse).
 *
 *    L'applicazione client deve fornire le seguenti funzioni:
 *      1. Aggiunta di un record all'elenco telefonico (operazione
 *      accessibile solo ad utenti autorizzati).
 *      2. Ricerca di un record all'interno dell'elenco telefonico
 *      (anche in questo caso l'operazione deve essere accessibile solo
 *      ad utenti autorizzati).
 *
 *    Si precisa che la specifica richiede la realizzazione sia dell'applicazione
 *    client che di quella server.
 *
 *    Si fa presente inoltre che l'insieme degli utenti autorizzati a consultare
 *    l'elenco telefonico non e' in generale coincidente con l'insieme
 *    degli utenti autorizzati a modificarlo.
 */

#define _GNU_SOURCE			//neded for semtimedop()
#define _DEFAULT_SOURCE     //needed for inet_aton() and inet_ntoa()

#include "../common_src/common_headers.h"

#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>



#define twoPow(x) ((unsigned long)1<<x)
#define error(str) { errorHandler(str, errno, __func__, __LINE__); }
#define fatalError(str) { printf("FATAL ERROR: %s. (func: %s() line: %d)\nERRNO (%d): %s\n", str, __func__, __LINE__, errno, strerror(errno)); fflush(stdout); kill(0, SIGQUIT); exit(2); }
#define logMsg(msg) { while(msgsnd(msgQueue, &msg, strlen(msg.txt)+1, 0)==-1) if(errno!=EINTR) fatalError("msgsnd() failed"); }
#define semaphore(nSem, nToken) { while(semop(sem, &(struct sembuf){nSem,nToken,0}, 1)==-1) if(errno!=EINTR) error("semop() failed"); }



//the index of the varius semaphores
enum semaphores{
	MAIN_READ_SEM,
	MAIN_WRITE_SEM,
	USER_READ_SEM,
	USER_WRITE_SEM,
	TOT_SEMAPHORES_N
};

#define TOT_READ_TOKENS 20

#define startMainRead() { semaphore(MAIN_WRITE_SEM, -1); semaphore(MAIN_READ_SEM, -1); semaphore(MAIN_WRITE_SEM, 1); }
#define endMainRead() { semaphore(MAIN_READ_SEM, 1); }
#define startMainWrite() { semaphore(MAIN_WRITE_SEM, -1); semaphore(MAIN_READ_SEM, -TOT_READ_TOKENS); }
#define endMainWrite() { semaphore(MAIN_WRITE_SEM, 1); semaphore(MAIN_READ_SEM, TOT_READ_TOKENS); }

#define startUserRead() { semaphore(USER_WRITE_SEM, -1); semaphore(USER_READ_SEM, -1); semaphore(USER_WRITE_SEM, 1); }
#define endUserRead() { semaphore(USER_READ_SEM, 1); }
#define startUserWrite() { semaphore(USER_WRITE_SEM, -1); semaphore(USER_READ_SEM, -TOT_READ_TOKENS); }
#define endUserWrite() { semaphore(USER_WRITE_SEM, 1); semaphore(USER_READ_SEM, TOT_READ_TOKENS); }



#define RESOURCES_FOLDER "server_resources/"
#define LOG_FOLDER RESOURCES_FOLDER "logs/"

#define MAIN_DB_FILENAME RESOURCES_FOLDER "main_db.txt"
#define PRIV_USERS_DB_FILENAME RESOURCES_FOLDER "priv_user_db.txt"
#define NORM_USERS_DB_FILENAME RESOURCES_FOLDER "norm_user_db.txt"
#define BASE_LOG_FILENAME LOG_FOLDER "server_log"
#define RECOVERY_DATA_FILENAME RESOURCES_FOLDER "recovery_data.txt"

#define MAIN_SAFE_SHUTDOWN_TIMEOUT 30
#define SEMAPHORE_SAFE_SHUTDOWN_TIMEOUT 12

#define BUFF_SIZE 4096

#define DYNARR_MAX_POSSIBLE_POWER 16
#define DYNARR_MAX_POSSIBLE_SIZE twoPow(DYNARR_MAX_POSSIBLE_POWER)

#define SERVER_BACKLOG 100
#define SERVER_SESSION_TIMEOUT 300
#define SOCKET_READ_TIMEOUT SERVER_SESSION_TIMEOUT
#define SOCKET_WRITE_TIMEOUT 10

#define FAILED_LOGIN_SLEEP 5
#define MAX_LOGIN_TRY 5


//global variables
extern int msgQueue;
extern int sem;



//database.c
typedef struct recordStruct{
	char *key;
	char *value;
} recS;

typedef struct dynamicArrayStruct{
	unsigned power;
	unsigned long size;
	unsigned long maxSize;
	struct recordStruct **arr;
} dArrS;

recS *initRecord(char *key, char *value);
void delRecord(recS *rec);
recS **initArr(unsigned power);
dArrS *initDynArr(unsigned power);
void delDynArr(dArrS *dynArr);
int appendRecToDynArr(recS *rec, dArrS *dynArr);
int findIndexFromKeyRecursive(char *key, dArrS *dynArr, unsigned long *retVal, unsigned long p1, unsigned long p2);
int findIndexFromKey(char *key, dArrS *dynArr, unsigned long *retVal);
int addRecToDynArr(recS *rec, dArrS *dynArr);
int removeRecFromDynArr(char *key, dArrS *dynArr);
void printDynArr(dArrS *dynArr);
unsigned neededPow(unsigned long n);
size_t recordToString(recS *rec, char *dest);
recS *stringToRecord(char *str);
void exportDynArr(dArrS *dynArr, char *filename);
dArrS *importDynArr(char *filename, unsigned char dynArrType);
dArrS *recoverMainDynArr(void);


//error_handler.c
void errorHandler(const char *str, int errNo, const char *func, int line);


//logger.c
typedef struct loggerMsgStruct{
	long type;
	char txt[BUFF_SIZE];
} msgS;

enum msgType{
	INVALID_MSG,
	ERR_MSG,
	WARN_MSG,
	INFO_MSG,
	SUCCESSFULL_SAFE_SHUTDOWN,
	RECOVERY_ADD_REC_MSG,
	RECOVERY_DEL_REC_MSG,
	TOT_MSG_TYPES
};

void sigIntLoggerHandler(int x);
void sigAlrmLoggerHandler(int x);
char *writeTime(char *dest);
void loggerProcess(void);
void logg(long type, char *txt);


//server.c
typedef struct connectionThreadStruct{
	pthread_t tid;
	int socket;
	struct sockaddr_in addr;
} connThS;

int main(int argc, char **argv);
void sigIntMainHandler(int x);
void sigAlrmMainHandler(int x);
void safeShutdown(int x);
void serverProcess(void);
void connectionThread(void *v);
void serverConsoleThread(void *dummy);
void parseCmdLine(int argc, char **argv, int *port);
