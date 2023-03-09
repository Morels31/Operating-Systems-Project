
#include "server_headers.h"



/*
 *  This function will be called with the error() macro, 
 *  prints and logs the error, than sends a SIGINT to all the processes,
 *  requesting to do a safe shutdown.
 *
 *    'str' = error string
 *    'errNo' = errno value
 *    'func' = name of the function where the error happened
 *    'line' = line at which the error happened
 */

void errorHandler(const char *str, int errNo, const char *func, int line){
	time_t t;
	if(time(&t)==-1) fatalError("time() failed");

	printf("ERROR: %s. (func: '%s' line: '%d') ERRNO (%d): %s\n", str, func, line, errno, strerror(errno));
	fflush(stdout);
	
	msgS msg;
	snprintf(msg.txt, BUFF_SIZE, "%s. (func: '%s' line: '%d') ERRNO (%d): %s\n", str, func, line, errno, strerror(errno));
	msg.type = ERR_MSG;

	logMsg(msg);

	if(kill(0, SIGINT)==-1) fatalError("kill() failed");					//Send a SIGINT signal to all the process of the group (request to safe-shutdown)
	pause();
	exit(1);
}
