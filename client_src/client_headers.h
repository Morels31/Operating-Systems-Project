
#define _DEFAULT_SOURCE     //needed for inet_aton() and inet_ntoa()

#include "../common_src/common_headers.h"
#include <netdb.h>


#define error(str) { printf("ERROR: %s. (func: %s() line: %d)\nERRNO (%d): %s\n", str, __func__, __LINE__, errno, strerror(errno)); fflush(stdout); exit(1); }
#define fatalError(str) { printf("FATAL ERROR: %s. (func: %s() line: %d)\nERRNO (%d): %s\n", str, __func__, __LINE__, errno, strerror(errno)); fflush(stdout); exit(2); }



#define SOCKET_READ_TIMEOUT 10
#define SOCKET_WRITE_TIMEOUT 10



void parseCmdLine(int argc, char **argv, char **ip, char **hostname, int *port);
