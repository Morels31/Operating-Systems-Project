
#include "client_headers.h"



int main(int argc, char **argv){
	char tokenStr[BUFF_SIZE];
	char respBuff[BUFF_SIZE];
	char buff[BUFF_SIZE];
	unsigned permission;
	char *token;
	size_t len;


	char *hostname = NULL;
	char *ip = DEFAULT_SERVER_IP;
	int port = DEFAULT_SERVER_PORT;
	parseCmdLine(argc, argv, &ip, &hostname, &port);


	int sock;
	if((sock = socket(AF_INET, SOCK_STREAM, 0))==-1) error("socket() failed");

	
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	
	struct hostent *he = NULL;
	if(hostname){
		if(!(he=gethostbyname(hostname))) error("Invalid hostname.");
		serverAddr.sin_addr = *((struct in_addr *)he->h_addr);
	}
	else{
		if(!inet_aton(ip, &serverAddr.sin_addr)){
			error("Invalid IP address.");
		}
	}

	
	struct timeval t;
	t.tv_usec = 0;
	t.tv_sec = SOCKET_READ_TIMEOUT;
	if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t))==-1) error("setsockopt() failed");

	t.tv_usec = 0;
	t.tv_sec = SOCKET_WRITE_TIMEOUT;
	if(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t))==-1) error("setsockopt() failed");


	if(connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr))==-1){
		if(errno==ECONNREFUSED || errno==ENETUNREACH || errno==113){
			printf("The server is offline, or the '%s' IP is not reachable.\n", ip);
			if(close(sock)==-1) error("close() failed");
			exit(1);
		}
		else error("connect() failed");
	}
	printNow("Successfully connected to server. Login:\n\n");

	const char connClosed[] = "The connection has been closed by the server\n";

	/* iterate until user and password are correct */
	while(1){
		buff[0] = TOKEN_REQ;
		len = readUserRecordString(buff+1);							//read username and password

		if(writeToSocket(buff, len+1, sock)){						//send a TOKEN_REQ to the server
			printNow(connClosed);
			goto client_exit;
		}

		if(!readFromSocket(tokenStr, sock)){						//read its response
			printNow(connClosed);
			goto client_exit;
		}


		switch(tokenStr[0]){										//examine response
			case SUCCESS_RESP:
				goto successfull_login;
			case TOO_MANY_TRY_RESP:
				printNow("Too many tries. Exiting.\n");
				goto client_exit;
			case INV_USERNAME_RESP:
				printNow("\nInvalid username, ");
				break;
			case INV_PASSWORD_RESP:
				printNow("\nInvalid password, ");
				break;
			default:
				printf("ERROR: Invalid response! (%s)\n", tokenStr);
				goto client_exit;
		}
		printNow("Retry to login:\n\n");
	}


	successfull_login:
	
	token = tokenStr + 2;
	permission = tokenStr[1];
	if(strlen(tokenStr)!=SESSION_TOKEN_LEN+2 || checkTokenString(token)){						//check response validity
		printf("ERROR: Invalid response length! (%s)\n", tokenStr);
		goto client_exit;
	}

	char *p;
	char cmdBuff[3];
	char errStr[] = "Invalid command, try again.\n\n";
	int tmp = sprintf(buff, "x%s%c", token, QUERY_ITEMS_SEPARATOR); //preset the buffer so that is ready for sending requests
	char *data = buff + tmp;

	/* the requests have this format: "'x''token';'data'" where 'x' is the type of request */

	while(1){
		printNow("\n\nAvailable commands:\n\t0: Exit\n\t1: Search record");
		if(permission==READ_WRITE_PERM) printNow("\n\t2: Add or overwrite record\n\t3: Remove record");
		while(!readLine("\n\nEnter command: ", errStr, 1, cmdBuff, NULL)) printf("%s", errStr);

		switch(atoi(cmdBuff)){
			case 0:
				printf("Exiting...\n");
				goto client_exit;
			case 1:
				readNameString(data, NULL);
				buff[0] = SEARCH_REQ;
				break;
			case 2:
				readMainRecordString(data);
				buff[0] = ADD_REQ;
				break;
			case 3:
				readNameString(data, NULL);
				buff[0] = DEL_REQ;
				break;
			default:
				printf("%s", errStr);
				continue;
		}

		if(writeToSocket(buff, strlen(buff), sock)){				//send the request to the server
			printNow(connClosed);
			goto client_exit;
		}

		if(!(len = readFromSocket(respBuff, sock))){						//read its response
			printNow(connClosed);
			goto client_exit;
		}

		switch(respBuff[0]){
			case SUCCESS_RESP:
				printf("Request successfully executed.\n");
				if(len>1){
					if(checkRecordString(respBuff+1, MAIN_TYPE)) error("Received invalid record string"); 
					p = respBuff+1;
					while(*p!='\0' && *p!=KEY_VALUE_SEPARATOR) p++;
					if(*p!='\0'){
						*p++='\0';
						printf("Name: %s\nNumbers: %s\n", respBuff+1, p);
					}
					else printf("There aren't numbers associated with the name '%s'\n", data);
				}
				break;
			case FAIL_RESP: 
				printf("Request failed.\n");
				break;
			case INV_REQ_RESP:
				error("The client did an invalid request.\n");
			default:
				error("Invalid response");
		}
	}


	client_exit:
	fflush(stdout);
	if(close(sock)==-1) error("close() failed");
	exit(0);
}



/*
 *  Parses the command line arguments.
 *  (if both the ip and hostname are specified,
 *  the hostname will be used)
 *
 *    'argc' = the main argc variable.
 *    'argv' = the main argv variable.
 *    'ip' = pointer to where will be saved the eventual ip.
 *    'hostname' = pointer to where will be saved the evntual hostname.
 *    'port' = a pointer where will be saved the evetual port.
 */

void parseCmdLine(int argc, char **argv, char **ip, char **hostname, int *port){

	for(int i=1; i<argc; i++){
		if(argv[i][0]!='-' || argv[i][0]=='\0') continue;
		switch(argv[i][1]){
			case 'n':
				if(i+1<argc) *hostname = argv[i+1];
				break;
			case 'a':
				if(i+1<argc) *ip = argv[i+1];
				break;
			case 'p':
				if(i+1<argc) *port = atoi(argv[i+1]);
				break;
			case 'h':
				printf("Options:\n\t-n (hostname)\n\t-a (ip addr)\n\t-p (port)\n\t-h display this help and exit\n");
				exit(0);
			default:
			invalid:
				printf("Invalid option '%s', use -h for help.\n", argv[i]);
				exit(1);
		}
		if(argv[i][2]!='\0') goto invalid;
	}
}










