
#include "server_headers.h"


int msgQueue;
int sem;

pid_t serverPid, loggerPid;

dArrS *mainDynArr = NULL;
dArrS *privUsersDynArr;
dArrS *normUsersDynArr;

int mainSocket;
unsigned port = DEFAULT_SERVER_PORT;



int main(int argc, char **argv){

	parseCmdLine(argc, argv, &port);

	/* setups the semaphore and the message queue global variables */
	srand(time(NULL));
	if((msgQueue = msgget(IPC_PRIVATE, 0600))==-1) fatalError("msgget() failed");
	if((sem = semget(IPC_PRIVATE, TOT_SEMAPHORES_N, IPC_CREAT | 0600))==-1) fatalError("semget() failed");

	if(mkdir(RESOURCES_FOLDER, 0600)==-1) if(errno!=EEXIST) fatalError("mkdir() failed");
	if(mkdir(LOG_FOLDER, 0600)==-1) if(errno!=EEXIST) fatalError("mkdir() failed");

	/* tries to access the RECOVERY_DATA_FILENAME, if it exists means that the last shutdown was forced and data has to be recovered */
	if(!access(RECOVERY_DATA_FILENAME, F_OK)) mainDynArr = recoverMainDynArr();


	loggerPid = fork();
	if(loggerPid==-1) fatalError("fork() failed");
	if(!loggerPid) loggerProcess();
	
	serverPid = fork();
	if(serverPid==-1) fatalError("fork() failed");
	if(!serverPid) serverProcess();


	struct sigaction act;
	act.sa_flags = 0;
	act.sa_restorer = NULL;
	if(sigfillset(&act.sa_mask)==-1) fatalError("sigfillset() failed");
	if(sigdelset(&act.sa_mask, SIGQUIT)==-1) fatalError("sigdelset() failed");

	act.sa_handler = sigIntMainHandler;
	if(sigaction(SIGINT, &act, NULL)==-1) fatalError("sigaction() failed");
	act.sa_handler = sigAlrmMainHandler;
	if(sigaction(SIGALRM, &act, NULL)==-1) fatalError("sigaction() failed");


	semaphore(MAIN_WRITE_SEM, 1);
	semaphore(MAIN_READ_SEM, TOT_READ_TOKENS);

	semaphore(USER_WRITE_SEM, 1);
	semaphore(USER_READ_SEM, TOT_READ_TOKENS);


	pid_t pid;
	int retVal;
	while((pid = wait(&retVal))==-1) if(errno!=EINTR) fatalError("wait() failed");
	if(pid==serverPid){
		if(retVal>>8!=0) fatalError("(MAIN) server process failed")
		else while(waitpid(loggerPid, &retVal, 0)==-1) if(errno!=EINTR) fatalError("waitpid() failed");
	}
	else fatalError("(MAIN) logger process failed")
	
	if(semctl(sem, 123, IPC_RMID)==-1) fatalError("semctl() failed");
	exit(0);
}



/*
 *  SIGINT handler for the main process,
 *  starts a timer of MAIN_SAFE_SHUTDOWN_TIMEOUT.
 *
 *    'x' = dummy variable
 */

void sigIntMainHandler(int x){
	alarm(MAIN_SAFE_SHUTDOWN_TIMEOUT);
}



/*
 *  SIGALRM handler for the main process,
 *  if executed means that the saef shutdown timed out.
 *  send a SIGQUIT to all the processes,
 *  forcing the shutdown and then quits.
 *
 *    'x' = dummy variable
 */

void sigAlrmMainHandler(int x){
	printf("(MAIN) Safe-shutdown timed out. Exiting...\n");
	fflush(stdout);
	kill(0, SIGQUIT);
	exit(2);
}



/*
 *  SIGINT handler for the server process,
 *  tries to do a safe shutdown, 
 *  saving all the necessary data
 *
 *    'x' = dummy variable
 */
void safeShutdown(int x){
	printNow("\nSafe shutdown started.\n");

	struct timespec t;
	struct sembuf op;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	/* exports the main dynamic array */
	op.sem_num = MAIN_WRITE_SEM;
	t.tv_sec = SEMAPHORE_SAFE_SHUTDOWN_TIMEOUT;
	t.tv_nsec = 0;
	while(semtimedop(sem, &op, 1, &t)==-1) if(errno!=EINTR) fatalError("semtimedop() failed, or reached timeout");
	if(mainDynArr) exportDynArr(mainDynArr, MAIN_DB_FILENAME);
	printNow("Saved main dynamic array.\n");

	/* exports the user dynamic arrays */
	op.sem_num = USER_WRITE_SEM;
	t.tv_sec = SEMAPHORE_SAFE_SHUTDOWN_TIMEOUT;
	t.tv_nsec = 0;
	while(semtimedop(sem, &op, 1, &t)==-1) if(errno!=EINTR) fatalError("semtimedop() failed, or reached timeout");
	if(privUsersDynArr) exportDynArr(privUsersDynArr, PRIV_USERS_DB_FILENAME);
	if(normUsersDynArr) exportDynArr(normUsersDynArr, NORM_USERS_DB_FILENAME);
	printNow("Saved user dynamic arrays.\n");
	
	msgS msg;
	msg.type = SUCCESSFULL_SAFE_SHUTDOWN;
	sprintf(msg.txt, "!");
	logMsg(msg);

	printNow("Safe shutdown successfully completed.\n");
	
	/* if successfull deletes the RECOVERY_DATA_FILENAME, so that the next time data doesn't have to be recovered */
	if(unlink(RECOVERY_DATA_FILENAME)==-1) fatalError("unlink() failed");

	if(close(mainSocket)==-1) fatalError("close() failed");
	exit(0);
}



/*
 *  The function where will execute the server process
 */

void serverProcess(void){
	if(!mainDynArr) mainDynArr = importDynArr(MAIN_DB_FILENAME, MAIN_TYPE);
	privUsersDynArr = importDynArr(PRIV_USERS_DB_FILENAME, USER_TYPE);
	normUsersDynArr = importDynArr(NORM_USERS_DB_FILENAME, USER_TYPE);

	struct sigaction act;
	act.sa_flags = 0;
	act.sa_restorer = NULL;
	if(sigfillset(&act.sa_mask)==-1) fatalError("sigfillset() failed");
	if(sigdelset(&act.sa_mask, SIGQUIT)==-1) fatalError("sigdelset() failed");

	act.sa_handler = safeShutdown;
	if(sigaction(SIGINT, &act, NULL)==-1) fatalError("sigaction() failed");

	msgS msg;
	msg.type = INFO_MSG;
	sprintf(msg.txt, "Server is starting.");
	logMsg(msg);


	if((mainSocket = socket(AF_INET, SOCK_STREAM, 0))==-1) error("socket() failed");

	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);

	if(bind(mainSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr))==-1) error("bind() failed");
	if(listen(mainSocket, SERVER_BACKLOG)==-1) error("listen() failed");

	if(port==DEFAULT_SERVER_PORT) printNow("The port has not been selected,\nso will be used the default one.\n(execute with -h for help)\n\n");
	sprintf(msg.txt, "Server started on port %u.\n", port);
	printNow(msg.txt);
	logMsg(msg);

	/* starts the server console thread */
	pthread_t tid;
	if(pthread_create(&tid, NULL, (void *) serverConsoleThread, (void *) &tid)) fatalError("pthread_create() failed");

	socklen_t clientAddrLen;
	connThS *thData;

	/* listens for client connections */
	while(1){

		thData = malloc(sizeof(struct connectionThreadStruct));

		clientAddrLen = sizeof(struct sockaddr_in);
		while((thData->socket = accept(mainSocket, (struct sockaddr *) &thData->addr, &clientAddrLen))==-1) if(errno!=EINTR) error("accept() failed"); //wait for requests

		sprintf(msg.txt, "Received connection from '%s'", inet_ntoa(thData->addr.sin_addr));
		logMsg(msg);

		if(pthread_create(&thData->tid, NULL, (void *) connectionThread, (void *) thData)) fatalError("pthread_create() failed");

	}
	exit(2);
}


void connectionThread(void *v){
	connThS *thData = v;
	msgS msg;
	size_t readed;
	char *username, *hash, *token;	
	char userStr[BUFF_SIZE];
	char tokenStr[SESSION_TOKEN_LEN+2];
	char buff[BUFF_SIZE];
	char shortBuff[2];
	shortBuff[1] = '\0';
	
	struct timeval t;
	t.tv_usec = 0;
	t.tv_sec = SOCKET_READ_TIMEOUT;
	if(setsockopt(thData->socket, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t))==-1) error("setsockopt() failed"); //sets the socket read timeout

	t.tv_usec = 0;
	t.tv_sec = SOCKET_WRITE_TIMEOUT;
	if(setsockopt(thData->socket, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t))==-1) error("setsockopt() failed"); //sets the socket write timeout


	unsigned permission = NO_PERM;
	unsigned long index;
	int i;
	for(i=0; i<MAX_LOGIN_TRY; i++){

		if(!readFromSocket(userStr, thData->socket)) goto connection_exit; //listen client request

		if(userStr[0]!=TOKEN_REQ) goto connection_exit;					//the first requests have to be TOKEN_REQ
		if(checkRecordString(userStr+1, USER_TYPE)){					//check the validity of the arrived user record string
			shortBuff[0] = INV_REQ_RESP;
			writeToSocket(shortBuff, 1, thData->socket);
			goto connection_exit;
		}
	
		/* get the username and hash from request */
		hash = username = userStr + 1;
		while(*hash!=KEY_VALUE_SEPARATOR && *hash!='\0') hash++;
		if(*hash=='\0') fatalError("This error should not occur");
		*hash++ = '\0';

		/* username and password check */
		startUserRead();
		if(findIndexFromKey(username, normUsersDynArr, &index)){	//The user is a normal user
			if(strcmp(hash, normUsersDynArr->arr[index]->value)){	//invalid password
				shortBuff[0] = INV_PASSWORD_RESP;
			}
			else{													//psw confirmed, user has now read permissions
				permission = READ_PERM;
				endUserRead();
				break;
			}
		}
		else if(findIndexFromKey(username, privUsersDynArr, &index)){ //the user is a privileged user
			if(strcmp(hash, privUsersDynArr->arr[index]->value)){	//invalid password
				shortBuff[0] = INV_PASSWORD_RESP;
			}
			else{													//psw confirmed, user has now read and write permissions
				permission = READ_WRITE_PERM;
				endUserRead();
				break;
			}
		}
		else{														//the received user is not registered
			shortBuff[0] = INV_USERNAME_RESP;
		}
		endUserRead();
		sleep(FAILED_LOGIN_SLEEP);
		if(i+1<MAX_LOGIN_TRY) writeToSocket(shortBuff, 1, thData->socket);
	}

	/* check if the user has tried to login MAX_LOGIN_TRY times */
	if(permission==NO_PERM || i==MAX_LOGIN_TRY){					//the second check is just for safety
		shortBuff[0] = TOO_MANY_TRY_RESP;
		writeToSocket(shortBuff, 1, thData->socket);
		goto connection_exit;
	}


	token = tokenStr + 2;
	tokenStr[0] = SUCCESS_RESP;										//return success response to client
	tokenStr[1] = permission;										//and its permission level
	randomString(SESSION_TOKEN_LEN, token);
	if(writeToSocket(tokenStr, SESSION_TOKEN_LEN+2, thData->socket)) goto connection_exit; //try to send the client the response with his token


	msg.type = INFO_MSG;
	sprintf(msg.txt, "The user '%s', successfully logged with %s permissions.", username, permission==READ_WRITE_PERM?"read and write":permission==READ_PERM?"read":"invalid");
	logMsg(msg);


	/* the requests should have this format: "'x''token';'data'" where 'x' is the type of request */

	char *data;
	while(1){														//read client request
		if(!(readed = readFromSocket(buff, thData->socket))) break;
		if(readed<SESSION_TOKEN_LEN+3 || buff[SESSION_TOKEN_LEN+1]!=QUERY_ITEMS_SEPARATOR) {
			shortBuff[0] = INV_REQ_RESP;
			writeToSocket(shortBuff, 1, thData->socket);
			goto connection_exit;
		}
		buff[SESSION_TOKEN_LEN+1] = '\0';
		data = buff + SESSION_TOKEN_LEN+2;


		switch(buff[0]){
			case SEARCH_REQ:										//search request
				if(checkNameString(data)) goto connection_exit;		//check arrived data
				startMainRead();
				if(findIndexFromKey(data, mainDynArr, &index)){
					buff[0] = SUCCESS_RESP;
					recordToString(mainDynArr->arr[index], buff+1);
				}
				else{
					buff[0] = FAIL_RESP;
					buff[1] = '\0';
				}
				endMainRead();
				break;
			case ADD_REQ:											//add record request
				if(permission!=READ_WRITE_PERM) goto connection_exit;
				if(checkRecordString(data, MAIN_TYPE)) goto connection_exit; //check arrived data
				startMainWrite();
				if(addRecToDynArr(stringToRecord(data), mainDynArr)) buff[0] = FAIL_RESP;
				else{
					buff[0] = SUCCESS_RESP;
					msg.type = RECOVERY_ADD_REC_MSG;
					sprintf(msg.txt, "%s", data);
					logMsg(msg);
				}
				endMainWrite();
				buff[1] = '\0';
				break;
			case DEL_REQ:											//remove record request
				if(permission!=READ_WRITE_PERM) goto connection_exit;
				if(checkNameString(data)) goto connection_exit;		//check arrived data
				startMainWrite();
				if(removeRecFromDynArr(data, mainDynArr)) buff[0] = FAIL_RESP;
				else{
					buff[0] = SUCCESS_RESP;
					msg.type = RECOVERY_DEL_REC_MSG;
					sprintf(msg.txt, "%s", data);
					logMsg(msg);
				}
				endMainWrite();
				buff[1] = '\0';
				break;
			default:
				buff[0] = INV_REQ_RESP;
				buff[1] = '\0';
				break;
		}
		if(writeToSocket(buff, strlen(buff), thData->socket)) break;
	}


	connection_exit:
	if(close(thData->socket)==-1) error("close() failed");
	free(thData);
	pthread_exit(0);
}



void serverConsoleThread(void *dummy){
	char buff[BUFF_SIZE];
	char *key, *value;
	recS *rec;
	msgS msg;
	int command = 1;
	printf("Server console initialized.");
	char askStr[] = "\n\nAvailable commands:\n\t- Administration:\n\t\t0: Safe shutdown.\n\t- Main dynamic array:\n\t\t1: Print main dynamic array.\n\t\t2: Add main record. (or modify an already existing one)\n\t\t3: Remove main record.\n\t- Privileged users dynamic array:\n\t\t4: Print privileged users dynamic array.\n\t\t5: Add privileged user. (or modify password of an already existing one)\n\t\t6: Remove privileged user.\n\t- Normal users dynamic array:\n\t\t7: Print normal users dynamic array.\n\t\t8: Add normal user. (or modify password of an already existing one)\n\t\t9: Remove normal user.\n\nEnter command: ";
	char errStr[] = "Invalid command, try again.\n\n";
	while(command){												//loop until a safe shutdown command is received
		while(!readLine(askStr, errStr, 1, buff, NULL)) printf("%s", errStr);
		command = atoi(buff);
		switch(command){
			case 0:													//safe shutdown
				if(kill(0, SIGINT)) fatalError("kill() failed");
				break;
			case 1:													//print main dynamic array
				printf("\n\n\n\n\n- - - - - Main dynamic array - - - - -\n");
				startMainRead();
				printDynArr(mainDynArr);
				endMainRead();
				break;
			case 2:													//add main record
				msg.type = RECOVERY_ADD_REC_MSG;
				readMainRecordString(msg.txt);
				startMainWrite();
				if(addRecToDynArr(stringToRecord(msg.txt), mainDynArr)) printf("Maximum size reached, can't add the record.\n");
				else printf("Main record added.\n");
				endMainWrite();
				logMsg(msg);
				break;
			case 3:													//remove main record
				msg.type = RECOVERY_DEL_REC_MSG;
				readNameString(msg.txt, NULL);
				startMainWrite();
				if(removeRecFromDynArr(msg.txt, mainDynArr)) printf("There isn't a main record with name '%s'.\n", msg.txt);
				else{
					logMsg(msg);
					printf("The main record with name '%s' has been removed.\n", msg.txt);
				}
				endMainWrite();
				break;
			case 4:													//print privileged users dynamic array
				printf("\n\n\n\n\n- - - Privileged users dynamic array - - -\n");
				startUserRead();
				printDynArr(privUsersDynArr);
				endUserRead();
				break;
			case 5:													//add privileged user
				key = readUsernameString(NULL, NULL);
				value = readPassword(NULL);
				rec = initRecord(key, value);
				startUserWrite();
				if(addRecToDynArr(rec, privUsersDynArr)){
					printf("Maximum size reached, can't add the record.\n");
					delRecord(rec);
				}
				else{ 
					if(!removeRecFromDynArr(key, normUsersDynArr)){
						printf("The user '%s' was a normal user, and has been promoted to privileged.\n", key);
						exportDynArr(normUsersDynArr, NORM_USERS_DB_FILENAME);
					}
					else printf("The user '%s' has been added to the privileged users dynamic array.\n", key);
					exportDynArr(privUsersDynArr, PRIV_USERS_DB_FILENAME);
				}
				endUserWrite();
				break;
			case 6:													//remove privileged user
				readUsernameString(buff, NULL);
				startUserWrite();
				if(removeRecFromDynArr(buff, privUsersDynArr)) printf("The user '%s' is not a privileged user.\n", buff);
				else printf("The user '%s' has been removed from the privileged users dynamic array.\n", buff);
				exportDynArr(privUsersDynArr, PRIV_USERS_DB_FILENAME);
				endUserWrite();
				break;
			case 7:													//print normal users dynamic array
				printf("\n\n\n\n\n- - - Normal users dynamic array - - -\n");
				startUserRead();
				printDynArr(normUsersDynArr);
				endUserRead();
				break;
			case 8:													//add normal user
				key = readUsernameString(NULL, NULL);
				value = readPassword(NULL);
				rec = initRecord(key, value);
				startUserWrite();
				if(addRecToDynArr(rec, normUsersDynArr)){
					printf("Maximum size reached, can't add the record.\n");
					delRecord(rec);
				}
				else{ 
					if(!removeRecFromDynArr(key, privUsersDynArr)){
						printf("The user '%s' was a privileged user, and has been declassed to normal.\n", key);
						exportDynArr(privUsersDynArr, PRIV_USERS_DB_FILENAME);
					}
					else printf("The user '%s' has been added to the normal users dynamic array.\n", key);
					exportDynArr(normUsersDynArr, NORM_USERS_DB_FILENAME);
				}
				endUserWrite();
				break;
			case 9:													//remove normal user
				readUsernameString(buff, NULL);
				startUserWrite();
				if(removeRecFromDynArr(buff, normUsersDynArr)) printf("The user '%s' is not a normal user.\n", buff);
				else printf("The user '%s' has been removed from the normal users dynamic array.\n", buff);
				exportDynArr(normUsersDynArr, NORM_USERS_DB_FILENAME);
				endUserWrite();
				break;
			default:												//invalid command
				printf("%s", errStr);
				break;
		}
	}
	pthread_exit(0);
}



/*
 *  Parses the command line arguments.
 *
 *    'argc' = the main argc variable.
 *    'argv' = the main argv variable.
 *    'port' = a pointer where will be saved the evetual port.
 */

void parseCmdLine(int argc, char **argv, int *port){

	for(int i=1; i<argc; i++){
		if(argv[i][0]!='-' || argv[i][0]=='\0') continue;
		switch(argv[i][1]){
			case 'p':
				if(i+1<argc) *port = atoi(argv[i+1]);
				break;
			case 'e':
				printf("%s\n", (char[8]){67,108,97,117,100,105,111,0});
				fflush(stdout);
				exit(3);
			case 'h':
				printf("Options:\n\t-p (port)\n\t-h display this help and exit\n");
				exit(0);
			default:
			invalid:
				printf("Invalid option '%s', use -h for help.\n", argv[i]);
				exit(1);
		}
		if(argv[i][2]!='\0') goto invalid;
	}
}
