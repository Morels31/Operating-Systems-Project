
#include "server_headers.h"


int logFd, recoveryFd;



/*
 *  SIGINT handler of the logger process.
 *  logs that a safe shutdown has been requested.
 *
 *    'x' = dummy variable
 */

void sigIntLoggerHandler(int x){
	msgS msg;
	msg.type = INFO_MSG;
	sprintf(msg.txt, "Safe shutdown requested.");
	logMsg(msg);
}



/*
 *  Formats and write the current time to an already 
 *  allocated char buffer pointer by 'dest'.
 *
 *  'dest' = a pointer to where the formatted time will be saved
 */

char *writeTime(char *dest){
	time_t rawtime;
	struct tm tS;
	if(time(&rawtime)==-1) fatalError("time() failed");
	if(!gmtime_r(&rawtime, &tS)) fatalError("gmtime() failed");
	int writed = sprintf(dest, "[%02d/%02d/%d %02d:%02d:%02d] ", tS.tm_mday, tS.tm_mon+1, (tS.tm_year+1900)%100, tS.tm_hour, tS.tm_min, tS.tm_sec);
	return dest + writed;
}



/*
 *  The function where will execute the logger process
 */

void loggerProcess(void){
	ssize_t readed, writed;
	size_t toWrite;
	char buff[BUFF_SIZE*2];
	char *p;
	msgS msg;

	struct sigaction act;
	act.sa_flags = 0;
	act.sa_restorer = NULL;
	if(sigfillset(&act.sa_mask)==-1) fatalError("sigfillset() failed");
	if(sigdelset(&act.sa_mask, SIGQUIT)==-1) fatalError("sigdelset() failed");

	act.sa_handler = sigIntLoggerHandler;
	if(sigaction(SIGINT, &act, NULL)==-1) fatalError("sigaction() failed");


	time_t rawtime;
	struct tm tS;
	char logFilename[BUFF_SIZE];
	if(time(&rawtime)==-1) fatalError("time() failed");
	if(!gmtime_r(&rawtime, &tS)) fatalError("gmtime() failed");
	int l = snprintf(logFilename, BUFF_SIZE-1, "%s_%02d%02d%02d_%02d%02d%02d.txt", BASE_LOG_FILENAME, (tS.tm_year+1900)%100, tS.tm_mon+1, tS.tm_mday, tS.tm_hour, tS.tm_min, tS.tm_sec);
	if(l>=BUFF_SIZE-1) fatalError("log filepath too long")

	int pid, fd;
	if((logFd = open(logFilename, O_WRONLY | O_CREAT | O_APPEND, 0600))==-1) fatalError("read() failed");
	if((recoveryFd = open(RECOVERY_DATA_FILENAME, O_WRONLY | O_CREAT | O_APPEND, 0600))==-1) fatalError("read() failed");

	msg.type = INFO_MSG;
	sprintf(msg.txt, "Logger started.");
	logMsg(msg);


	int loop = 1;
	while(loop){
		while((readed = msgrcv(msgQueue, &msg, BUFF_SIZE, 0, MSG_NOERROR))==-1) if(errno!=EINTR) fatalError("msgrcv() failed");

		if(msg.type<RECOVERY_ADD_REC_MSG){
			fd = logFd;
			p = writeTime(buff);
		}
		else fd = recoveryFd;
		
		switch(msg.type){
			case ERR_MSG:											//error message
				p += sprintf(p, "ERROR: %s\n", msg.txt);
				toWrite = p - buff;
				break;
			case WARN_MSG:											//warning message
				p += sprintf(p, "WARNING: %s\n", msg.txt);
				toWrite = p - buff;
				break;
			case INFO_MSG:											//info message
				p += sprintf(p, "INFO: %s\n", msg.txt);
				toWrite = p - buff;
				break;
			case SUCCESSFULL_SAFE_SHUTDOWN:							//this message tells the logger that can exit safely
				p += sprintf(p, "INFO: Successfully done a safe-shutdown.\n");		
				toWrite = p - buff;
				loop = 0;
				break;
			case RECOVERY_ADD_REC_MSG:								//a record has been added, will be logged to the recovery data file
				toWrite = sprintf(buff, "1%s\n", msg.txt);
				break;
			case RECOVERY_DEL_REC_MSG:								//a record has been removed, will be logged to the recovery data file
				toWrite = sprintf(buff, "0%s:\n", msg.txt);
				break;
			default:
				fatalError("invalid message type");
				break;
		}

		p = buff;
		while(toWrite>0){
			while((writed = write(fd, p, toWrite))<0) if(errno!=EINTR) fatalError("write() failed");
			toWrite -= writed;
			p += writed;
		}
		if(fsync(fd)==-1) fatalError("fsync() failed");
	}
	
	if(close(logFd)) fatalError("close() failed");
	if(close(recoveryFd)) fatalError("close() failed");
	if(msgctl(msgQueue, IPC_RMID, NULL)==-1) fatalError("msgctl() failed");
	exit(0);
}
