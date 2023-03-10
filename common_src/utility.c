#ifdef SERVER
	#include "../server_src/server_headers.h" 
#elif CLIENT
	#include "../client_src/client_headers.h"
#else
	#include "common_headers.h"
#endif



/*
 *  Clears linux standard input buffer.
 *  (not the <stdio.h> stdin buffer)
 */

void clearStdin(void){
	struct pollfd pIn;												//initialize required argument for calling the poll function
	pIn.fd = 0;
	pIn.events = POLLIN;
	char buff[BUFF_SIZE];
	int ret;
	while(1){														//loop until stdin is empty
		while((ret = poll(&pIn, 1, 0))<0) if(errno!=EINTR) error("poll() failed");	//call poll(check if stdin has available characters to read)
		if(!ret) break;																//if there aren't available characters in stdin, return
		while(read(0, buff, BUFF_SIZE)==-1) if(errno!=EINTR) error("read() failed");	//else read those characters and loop
	}
}



/*
 *  Reads a line from the linux standard input,
 *  copies the line in an newly allocated area, or,
 *  if 'optionalDest' is not NULL, in the area pointed by it. 
 *
 *  And, if the 'optionalTotChars' variable is not NULL,
 *  it saves the number of characters of the returned string,
 *  in the integer pointed by it.
 *  
 *	(the function doesn't crash if CTRL+D is entered)
 *  
 *    'askStr' = the prompt string that will be printed before the user input. es: "Enter text: ".
 *    'errStr' = the string that will be printed if the readed string is larger than 'maxLen'.
 *    'maxLen' = the maximum length of the string.
 *    'optionalDest' = a pointer to an optional already allocated char buffer.
 *    'optionalTotChars' = an optional integer pointer,
 *      where will be saved the number of characters in the returned string.
 *
 *    returns a pointer to the new string, or,
 *    returns NULL if input is empty.  
 */

char *readLine(char *askStr, char *errStr, int maxLen, char *optionalDest, size_t *optionalTotChars){
	ssize_t readed;
	char buff[maxLen+2];
	while(1){														//loop until string is valid
		printNow(askStr);															
		while((readed = read(0, buff, maxLen+1))<0) if(errno!=EINTR) error("read() failed"); //read from standard input
		buff[readed] = '\0';										//set a null byte at the end of the string

		if(!readed || readed==1 && buff[0]=='\n'){					//if string is empty return NULL
			if(optionalTotChars) *optionalTotChars = 0;					
			return NULL;
		}
		if(buff[readed-1]=='\n'){									//if last char is newline, remove it
			buff[readed-1] = '\0';
			readed--;
		}
		if(readed<=maxLen) break;									//if string is shorter or equal to 'maxLen' exit loop
		printNow(errStr);
		clearStdin();												//else clear stdin and loop
	}

	char *resultStr;
	if(optionalDest) resultStr = optionalDest;						//if optionalDest is not NULL, save result string there
	else{															//else allocate space for result string
		resultStr = malloc((strlen(buff) + 1) * sizeof(char));
		if(!resultStr) error("malloc() failed");
	}
	
	if(optionalTotChars) *optionalTotChars = readed;
	strcpy(resultStr, buff);										//copy string(included \0) from buffer to str
	memset(buff, 0, (maxLen + 2) * sizeof(char));					//for security set all the characters of the buffer to 0
	return resultStr;
}



/*
 *  Generates a random string of length 'length' in a newly allocated area, or,
 *  if 'optionalDest' is not NULL in the area pointed by it.
 *  The string will be composed only by characters inside the RAND_STR_FULL_CHARSET string.
 *
 *    'length' = the desired length of the string
 *    'optionalDest' = a pointer to an optional already allocated char buffer.
 *
 *    returns a pointer to the the random string, or
 *    returns NULL if 'length' is 0.
 */

char *randomString(size_t length, char *optionalDest){
		if(!length) return NULL;

		const char *charset = RAND_STR_FULL_CHARSET; 
		size_t charNum = strlen(charset);
		
		char *randomString;
		if(optionalDest) randomString = optionalDest;
		else{
			randomString = malloc((length + 1) * sizeof(char));
			if(!randomString) error("malloc() failed");
		}

		for (size_t i=0; i<length; i++) randomString[i] = charset[rand() % charNum];
		randomString[length] = '\0';
		return randomString;
}



/*
 *  Checks if there are still characters to read in a file.
 *  (if the cursor of the given file descriptor, it's not at EOF)
 *  (assumes that no other processes or threads are using the file)
 *
 *    'fd' = valid file descriptor.
 *
 *    returns 0 if there are characters to be read, else
 *    returns 1
 */

int isFileFinished(int fd){
	off_t pos, end; 
	if((pos = lseek(fd, 0, SEEK_CUR))==-1) error("lseek() failed");	//saves the current cursor position
	if((end = lseek(fd, 0, SEEK_END))==-1) error("lseek() failed");	//saves the end position 
	if(lseek(fd, pos, SEEK_SET)==-1) error("lseek() failed");			//restores the cursor position
	return pos==end;
}



/*
 *  This function reads a line from an already opened file.
 *  buffering the actual reads in a user defined buffer,
 *  and having saved context beetween different calls.
 *
 *  Every call of the function can have a different "state":
 *    -start/re-start (miss):
 *      The only time an actual read from the file occurs,
 *      if there isn't anything left to read it returns -1.
 *    -in-stock (hit):
 *      The next line is already fully in the buffer.
 *    -restock (miss):
 *      The next line is only half present in the buffer,
 *      the cursor is moved backwards to the beginning
 *      of the half line, and the function is resetted
 *      to the re-start state.
 *    -end (hit):
 *      This call will return the last line, and,
 *      in the next call -1 will be returned.
 *  
 *  The assumptions are that:
 *    In the first call:
 *      -The cursor of the i-o session is at the beginning of the file.
 *      -The variable pointed by 'p2' is NULL.
 *    No other processes or threads are modifying the file.
 *
 *    'fd' = file descriptor of the already opened file.
 *    'buff' = pointer to a char buffer of size BUFF_SIZE.
 *    'p1' = pointer to a char pointer variable, where the result will be saved.
 *    'p2' = pointer to a char pointer variable, that will point to the last char
 *      of the result string, or to NULL if the next call will be a "start/re-start".
 *
 *  returns -1 if there isn't anything left to read, else
 *  returns the size of the readed line
 */

int readLineFromFile(int fd, char *buff, char **p1, char **p2){
	int readed, nChars;

	if(*p2==NULL){													//start or re-start
		while((readed = read(fd, buff, BUFF_SIZE-1))<0) if(errno!=EINTR) error("read() failed"); //read from file
		if(!readed) return -1;										//nothing left to read
		buff[readed] = '\0';
		*p1 = *p2 = buff;
	}
	else{														
		(*p2)++;
		*p1 = *p2;
	}

	while(**p2!='\0' && **p2!='\n') (*p2)++;
	if(**p2=='\0'){													//"restock" or "end" without final \n
		if((*p2)-(*p1)==BUFF_SIZE-1) error("a line in the file is too long");
		if(isFileFinished(fd)) return *p1==*p2 ? -1 : (*p2 - *p1);	//"end" without final \n (this should never happen)
																	//"restock"
		if(lseek(fd, -(*p2 - *p1), SEEK_CUR)==-1) error("lseek() failed");
		*p2 = NULL;
		return readLineFromFile(fd, buff, p1, p2);
	}
	else{															//"in-stock"
		**p2 = '\0';
		nChars = (*p2 - *p1);
		if(*((*p2)+1)=='\0') *p2 = NULL;							//"in-stock" with no data left in the buffer (so "end" with final \n, or clean "in-stock")
		return nChars;
	}
} 



/*
 *  Checks if 'str' is a valid string:
 *  (it's not empty, has <= 'maxSize' chars,
 *  and contains only alphanumeric characters,
 *  or any of the characters in the string pointed by 'charset').
 *
 *    'str' = string to check.
 *    'charset' = string containing the additional valid characters,
 *      other than the alphanumeric ones.
 *    'maxSize' = the maximum valid size of the string.
 *
 *    returns 0 if 'str' is valid, else
 *    returns 1
 */

int checkGenericString(char *str, const char *charset, size_t maxSize){
	if(!str) error("NULL argument");
	size_t l = strlen(str);
	if(!l || l>maxSize) return 1;

	char *p;
	for(size_t i=0; i<l; i++){
		if(!isalnum(str[i])){
			p = (char *) charset;
			while(*p!='\0' && str[i]!=*p) p++;
			if(*p=='\0') return 1;
		}
	}
	return 0;
}



/*
 *  Checks if 'name' is a valid name string:
 *  (it's not empty, has <= MAX_NAME_LEN chars,
 *  and contains only alphanumeric characters, spaces or ')
 *
 *    'name' = string to check.
 *
 *	  returns 0 if 'name' is valid, else
 *    returns 1
 */

int checkNameString(char *name){
	return checkGenericString(name, NAME_CHARSET, MAX_NAME_LEN);
}



/*
 *  Checks if 'num' is a valid single-number string:
 *  (has <= MAX_NUM_LEN chars, and contains only numeric characters or +)
 *
 *    'num' = string to check.
 *
 *    returns 0 if 'num' is valid, else
 *    returns 1
 */

int checkNumString(char *num){
	if(!num) error("NULL argument");
	size_t l = strlen(num);
	if(!l || l>MAX_NUM_LEN) return 1;
	for(size_t i=0; i<l; i++) if(!isdigit(num[i]) && num[i]!='+') return 1;
	return 0;
}



/*
 *  Checks if 'nums' is a valid multiple-numbers string:
 *  (has <= MAX_NUMS_LEN chars, contains <= MAX_N_NUMS substrings (single numbers),
 *  and each substring is a valid number)
 *
 *    'nums' = string to check.
 *
 *    returns 0 if 'nums' is valid, else
 *    returns 1
 */

int checkNumsString(char *nums){
	if(!nums) return 0;
	size_t l = strlen(nums);
	if(!l) return 0;
	if(l>MAX_NUMS_LEN) return 1;
	
	int res;
	char *p1;
	char *p2 = nums;
	for(int i=0; i<MAX_N_NUMS; i++){
		p1 = p2;
		while(*p2!='\0' && *p2!=SINGLE_NUM_SEPARATOR) p2++;
		if(*p2=='\0') return checkNumString(p1);

		*p2 = '\0';
		res = checkNumString(p1);
		*p2++ = SINGLE_NUM_SEPARATOR;
		if(res) return 1;
	}
	return 1;
}



/*
 *  Check if 'username' is a valid username:
 *  (it's not empty, has <= MAX_USERNAME_LEN chars,
 *  and contains only alphanumeric characters, - or _)
 *
 *    'username' = string to check.
 *
 *	  returns 0 if 'username' is valid, else
 *    returns 1
 */

int checkUsernameString(char *username){
	return checkGenericString(username, USERNAME_CHARSET, MAX_USERNAME_LEN);
}



/*
 *  Check if 'psw' is a valid password:
 *  (it's not empty, has <= MAX_PASSWORD_LEN and >= MIN_PASSWORD_LEN chars,
 *  and contains only alphanumeric characters, or any of the characters in the PASSWORD_CHARSET string).
 *
 *    'password' = string to check.
 *
 *	  returns 0 if 'password' is valid, else
 *    returns 1
 */

int checkPasswordString(char *psw){
	if(!psw) error("NULL argument");
	if(strlen(psw)<MIN_PASSWORD_LEN) return 1;
	return checkGenericString(psw, PASSWORD_CHARSET, MAX_PASSWORD_LEN);
}



/*
 *  Check if 'hash' is a valid hash:
 *  (has  exactly HASH_LEN chars,
 *  and contains only alphanumeric characters, / or .)
 *
 *    'hash' = string to check.
 *
 *	  returns 0 if 'hash' is valid, else
 *    returns 1
 */
int checkHashString(char *hash){
	if(!hash) error("NULL argument");
	if(strlen(hash)!=HASH_LEN) return 1;
	return checkGenericString(hash, HASH_CHARSET, HASH_LEN);
}



/*
 *  Checks if 'token' is a valid token:
 *  (has exactly SESSION_TOKEN_LEN chars,
 *  and contains only characters in the RAND_STR_FULL_CHARSET string)
 *
 *  'token' = token to check.
 *
 *  returns 0 if 'token' is valid, else
 *  returns 1
 */

int checkTokenString(char *token){
	if(!token) error("NULL argument");
	if(strlen(token)!=SESSION_TOKEN_LEN) return 1;
	return checkGenericString(token, RAND_STR_FULL_CHARSET, SESSION_TOKEN_LEN);
}



/*
 *  Checks if 'str' is a valid 'recType'-record string:
 *    If the record is a main-record:
 *      It's not empty, has <= MAX_MAIN_REC_STR_LEN chars,
 *      both it's name and nums substrings are valid,
 *      and there is only one KEY_VALUE_SEPARATOR.
 *    Else, if the record is a user-record:
 *      It's not empty, has <= MAX_USER_REC_STR_LEN chars,
 *      both it's username and hash substrings are valid, 
 *      and there is only one KEY_VALUE_SEPARATOR.
 *
 *  This function, utilizes function pointers, to set the correct ones
 *  that will later be used to check the two substrings.
 *
 *    'str' = string to check.
 *    'recType' = the type of record (only valid options are MAIN_TYPE or USER_TYPE).
 *
 *	  returns 0 if 'str' is valid, else
 *    returns 1
 */

int checkRecordString(char *str, unsigned char recType){
	if(!str) error("NULL argument");

	size_t maxLen, l;
	int (*checkStr1)(char *str);
	int (*checkStr2)(char *str);
	if(recType==MAIN_TYPE){
		maxLen = MAX_MAIN_REC_STR_LEN;
		checkStr1 = checkNameString;
		checkStr2 = checkNumsString;
	}
	else if(recType==USER_TYPE){
		maxLen = MAX_USER_REC_STR_LEN;
		checkStr1 = checkUsernameString;
		checkStr2 = checkHashString;
	}
	else error("invalid record type");

	l = strlen(str);
	if(!l || l>maxLen) return 1;
	
	int res;
	char *p = str;

	while(*p!='\0' && *p!=KEY_VALUE_SEPARATOR) p++;
	if(*p=='\0') return 1;
	*p = '\0';
	res = checkStr1(str);
	*p++ = KEY_VALUE_SEPARATOR;
	return res || checkStr2(p);
}



/*
 *  Formats the 'name' string so that:
 *    every first character of all the words is in uppercase,
 *    and the remainers are in lowercase.
 *  es:
 *    "mario ROSSI" -> "Mario Rossi"
 *  (the 'name' string will be edited directly)
 *
 *    'name' = string to format.
 */

void formatNameString(char *name){
	int x = 1;
	size_t l = strlen(name);
	for(size_t i=0; i<l; i++){
		if(x){
			if(islower(name[i])) name[i] = toupper((unsigned char) name[i]);
			x = 0;
		}
		else if(isupper(name[i])) name[i] = tolower((unsigned char) name[i]);
		if(name[i]==' ' || name[i]=='\'') x = 1;
	}
}



/*
 *  Reads from the linux standard input a valid name string,
 *  formats it, and saves it in an newly allocated area, or,
 *  if 'optionalDest' is not NULL, in the area pointed by it. 
 *
 *  And, if the 'optionalTotChars' variable is not NULL,
 *  it saves the number of characters of the returned string,
 *  in the integer pointed by it.
 *
 *    'optionalDest' = a pointer to an optional already allocated char buffer.
 *    'optionalTotChars' = an optional integer pointer,
 *      where will be saved the number of characters in the returned string.
 *
 *    returns a pointer to the new string
 */

char *readNameString(char *optionalDest, size_t *optionalTotChars){
	char askStr[] = "Enter Name: ";
	char longStrErr[] = "ERROR: Name too long.\n";
	char emptyStrErr[] = "ERROR: Name can't be empty.";
	char invalidStrErr[] = "ERROR: Name string not valid. (can contain only alphanumeric characters, spaces or ')";

	char *returnStr;
	while(1){														//loop until string is valid
		while(!(returnStr = readLine(askStr, longStrErr, MAX_NAME_LEN, optionalDest, optionalTotChars))) printf("%s\n", emptyStrErr); //read a non empty string
		if(!checkNameString(returnStr)) break;

		printf("%s\n", invalidStrErr);
		if(!optionalDest) free(returnStr);								
	}
	formatNameString(returnStr);									//format name string
	return returnStr;
}



/*
 *  Reads from the linux standard input multiple valid number strings,
 *  and save them following this method: "num1,num2,numN".
 *  
 *  The result will be saved in an newly allocated area, or,
 *  if 'optionalDest' is not NULL, in the area pointed by it. 
 *
 *  And, if the 'optionalTotChars' variable is not NULL,
 *  it saves the number of characters of the returned string,
 *  in the integer pointed by it.
 *
 *    'optionalDest' = a pointer to an optional already allocated char buffer.
 *    'optionalTotChars' = an optional integer pointer,
 *      where will be saved the number of characters in the returned string.
 *  
 *    returns a pointer to the new string, or,
 *    returns NULL if no numbers are readed.  
 */

char *readNumsString(char *optionalDest, size_t *optionalTotChars){
	char longStrErr[] = "ERROR: Number too long.\n";
	char invalidStr[] = "ERROR: Number not valid. (can contain only numeric characters or +)\n";

	char buff[BUFF_SIZE];
	char *p = buff;
	char *check;
	char askStr[100];

	size_t readed;
	size_t totChars = 0;
	for(int i=0; i<MAX_N_NUMS; i++){								//loop for MAX_N_NUMS times or until emptyline is readed
		sprintf(askStr, "Enter number n°%d or press enter to continue: ", i+1);
		while( (check = readLine(askStr, longStrErr, MAX_NUM_LEN, p, &readed)) && checkNumString(p)) printNow(invalidStr); //read from standard input and save to 'p'
		totChars += readed;
		if(!check) break;											//if emptyline is readed, exit loop
		p += readed;													
		*p++ = SINGLE_NUM_SEPARATOR;								//append a separator
		totChars += 1;
	}

	if(optionalTotChars) *optionalTotChars = totChars?totChars-1:0;	//if 'optionalTotChars' is not NULL, saves there the correct total number of characters
	if(!totChars) return NULL;										//if 0 numbers has been readed, return NULL
	
	buff[totChars-1] = '\0';										//remove the last separator
	char *resultStr;
	if(optionalDest) resultStr = optionalDest;						//if 'optionalDest' is not NULL save the result there
	else{															//else save to a newly allocated area
		resultStr = malloc((totChars + 1) * sizeof(char));
		if(!resultStr) error("malloc() failed");
	}

	strcpy(resultStr, buff);										//copy string(included \0) from buffer to result destination
	return resultStr;												//return a pointer to str
}



/*
 *  Reads from the linux standard input a valid username string,
 *  and saves it in an newly allocated area, or,
 *  if 'optionalDest' is not NULL, in the area pointed by it. 
 *
 *  And, if the 'optionalTotChars' variable is not NULL,
 *  it saves the number of characters of the returned string,
 *  in the integer pointed by it.
 *
 *    'optionalDest' = a pointer to an optional already allocated char buffer.
 *    'optionalTotChars' = an optional integer pointer,
 *      where will be saved the number of characters in the returned string.
 *
 *    returns a pointer to the new string
 */

char *readUsernameString(char *optionalDest, size_t *optionalTotChars){
	char askStr[] = "Enter Username: ";
	char longStrErr[] = "ERROR: Username too long.\n";
	char emptyStrErr[] = "ERROR: Userame can't be empty.";
	char invalidStrErr[] = "ERROR: Username not valid. (can contain only alphanumeric characters, - or _)";

	char *returnStr;
	while(1){														//loop until string is valid
		while(!(returnStr = readLine(askStr, longStrErr, MAX_USERNAME_LEN, optionalDest, optionalTotChars))) printf("%s\n", emptyStrErr); //read a non empty string
		if(!checkUsernameString(returnStr)) break;					//if string is valid, exit loop

		printf("%s\n", invalidStrErr);
		if(!optionalDest) free(returnStr);								
	}
	return returnStr;
}



/*
 *  Hashes the string pointed by 'str' using the sha512crypt algorithm.
 *  checks it's validity, and then saves it in an newly allocated area, or,
 *  if 'optionalDest' is not NULL, in the area pointed by it.)
 *
 *    'str' = string to hash.
 *    'optionalDest' = a pointer to an optional already allocated char buffer, long at least HASH_SIZE+1.
 *
 *    returns a pointer to the new string
 */

char *hash(char *str, char *optionalDest){
	struct crypt_data data;
	memset(&data, 0, sizeof(struct crypt_data));
	if(!crypt_r(str, "$6$", &data)) error("crypt_r() failed");
	if(data.output[0]=='*') error("crypt_r() failed");
	char *p = data.output;
	for(int i=0; i<3; i++){
		while(*p!='$' && *p!='\0') p++;
		if(*p=='\0') error("generated invalid hash");
		p++;
	}
	if(checkHashString(p)) error("generated invalid hash");

	char *resultStr;
	if(optionalDest) resultStr = optionalDest;
	else{
		resultStr = malloc((HASH_LEN + 1) * sizeof(char));
		if(!resultStr) error("malloc() failed");
	}

	strcpy(resultStr, p);
	memset(&data, 0, sizeof(struct crypt_data));
	return resultStr;
}



/*
 *  Disables the input echo of the terminal, reads a password,
 *  checks it's validity, re-enables the echo of the terminal,
 *  then calls the hash() function to hash it (saving the result in an newly
 *  allocated area, or if 'optionalDest' is not NULL, in the area pointed by it.)
 *  and finally clears the password buffer for security purpose.
 *
 *  (disabling terminal input echo found on: https://stackoverflow.com/questions/1786532/c-command-line-password-input)
 *
 *    'optionalDest' = a pointer to an optional already allocated char buffer, long at least HASH_SIZE+1.
 *
 *    returns a pointer to the new string
 */

char *readPassword(char *optionalDest){
	char askStr[] = "Enter Password: ";
	char longStrErr[] = "The entered password is too long.\n";
	char emptyStrErr[] = "Password can't be empty!";
	static struct termios oldt, newt;

	tcgetattr( STDIN_FILENO, &oldt);								//saving the old settings of STDIN_FILENO and copy settings for restoring
	newt = oldt;
	newt.c_lflag &= ~(ECHO);										//setting the approriate bit in the termios struct
	tcsetattr( STDIN_FILENO, TCSANOW, &newt);						//enabling new STDIN_FILENO settings -> disable input echo

	char buff[MAX_PASSWORD_LEN+1];
	size_t readed;
	while(1){
		while(!readLine(askStr, longStrErr, MAX_PASSWORD_LEN, buff, &readed)) printf("\n%s\n", emptyStrErr);
		printNow("\n");
		if(!checkPasswordString(buff)) break;
		printf("The entered password is invalid. (has to be at least %d chars, and has to contain only alphanumeric chars or any of %s)\n", MIN_PASSWORD_LEN, PASSWORD_CHARSET);
		fflush(stdout);
	}

	tcsetattr( STDIN_FILENO, TCSANOW, &oldt);						//resetting old STDIN_FILENO settings -> re-enable input echo

	char *resultStr = hash(buff, optionalDest);
	memset(buff, 0, (MAX_PASSWORD_LEN+1) * sizeof(char));
	return resultStr;
}



/*
 *  Reads both a name and nums string from standard input,
 *  and saves them in a single string,
 *  in the location pointed by 'dest'.
 *  
 *  (usefull for reading a main record from a client)
 *
 *    'dest' = a pointer to an already allocated char buffer
 *
 *    returns the number of characters in the final string.
 */

size_t readMainRecordString(char *dest){
	size_t readed;
	char *p = dest;

	readNameString(p, &readed);
	p += readed;
	*p++ = KEY_VALUE_SEPARATOR;
	readNumsString(p, &readed);
	p += readed;
	*p = '\0';
	return (size_t) (p - dest);
}



/*
 *  Reads both an username and password string from standard input,
 *  hashes the password and saves the result in a single string,
 *  in the location pointed by 'dest'.
 *  
 *  (usefull for reading an user record from a client)
 *
 *    'dest' = a pointer to an already allocated char buffer
 *
 *    returns the number of characters in the final string.
 */

size_t readUserRecordString(char *dest){
	size_t readed;
	char *p = dest;

	readUsernameString(p, &readed);
	p += readed;
	*p++ = KEY_VALUE_SEPARATOR;
	readPassword(p);
	p += HASH_LEN;
	*p = '\0';
	return (size_t) (p - dest);
}



/*
 *  Returns how many lines are in a file.
 *  Creating oneother process that will execve to the "wc" program,
 *  and reading it's standard output with a pipe.
 *
 *  (usefull for initializing Dynamic Arrays already at the correct size) 
 *
 *    'filename' = name of the file
 *
 *    returns the number of lines in the input file
 */

unsigned long countFileLines(char *filename){
	if(access(filename, F_OK)){										//if file doesn't exist, return 0
		if(errno==ENOENT) return 0;
		else error("access() failed");
	}
	if(access(filename, R_OK)) error("read permission denied to file"); //check if this process has read permission over the file
	
	int pipefd[2];
	if(pipe(pipefd)) error("pipe() failed");						//initialize the pipe
	
	pid_t pid = fork();
	if(pid == -1) error("fork() failed");
	if(!pid){														//child process
		if(close(pipefd[0])==-1) error("close() failed");
		if(dup2(pipefd[1], 1)==-1) error("dup2() failed");			//replace standard output fd with the input one of the pipe
		execlp("wc", "wc", filename, "-l", NULL);					//execve to the "wc" program, with the setting to read lines 
		if(close(pipefd[1])==-1) error("close() failed");
		exit(1);
	}
	if(close(pipefd[1])==-1) error("close() failed");
	
	int retCode;
	waitpid(pid, &retCode, 0);										//parent waits for the child process
	if(retCode>>8){													//and if it failed returns 0
		if(close(pipefd[0])==-1) error("close() failed");
		return 0;
	}

	char buff[BUFF_SIZE];
	while(read(pipefd[0], buff, BUFF_SIZE-1)<0) if(errno!=EINTR) error("read() failed"); //reads result from output of the pipe
	if(close(pipefd[0])==-1) error("close() failed");

	unsigned long long res = strtoll(buff, NULL, 10);				//converts the result
	if(res == LLONG_MAX || res == LLONG_MIN) error("strtoll() failed");
	if(res>=ULONG_MAX) error("input file is too big");
	return (unsigned long) res;
}



/*
 *  Writes to a valid, already opened socket,
 *  the string pointed by 'str' of length 'len'.
 *
 *    'str' = the string to write.
 *    'len' = the length of 'str'
 *
 *    returns 0 if the write was successfull, or
 *    returns 1 if the connection has been closed or timed out.
 */

int writeToSocket(char *str, size_t len, int sockFd){
	if(len>=BUFF_SIZE) error("tried writing to socket a string longer than BUFF_SIZE");
	if(!str) error("NULL argument");

	size_t toWrite = len;
	ssize_t writed;
	char *p = str;
	
	while(toWrite>0){
		while((writed = send(sockFd, p, toWrite, MSG_NOSIGNAL))<0){
			if(errno==EPIPE || errno==EAGAIN || EWOULDBLOCK) return 1; //return 1 if connection has been closed or timed out
			else if(errno!=EINTR) error("send() failed");
		}
		toWrite -= writed;
		p += writed;
	}
	return 0;
}



/*
 *  Reads from a valid, already opened socket,
 *  saving the result to the buffer of size BUFF_SIZE pointed by 'dest'.
 *  (assumes that an eventual socket read timeout is
 *  already been configured with the setsockopt() function)
 *
 *    'dest' = pointer to a char buffer of size BUFF_SIZE
 *    
 *    returns 0 if the connection has been closed, timed out, or the data was too long). or
 *    returns the number of readed characters.
 */

size_t readFromSocket(char *dest, int sockFd){
	if(!dest) error("NULL argument");
	ssize_t readed;
	
	while((readed = read(sockFd, dest, BUFF_SIZE-1))<0){
		if(errno==EAGAIN || EWOULDBLOCK) return 0;					//return 0 if timed out
		if(errno!=EINTR) error("read() failed");
	}
	if(readed>=BUFF_SIZE-1) return 0;
	dest[readed] = '\0';
	return readed;
}
