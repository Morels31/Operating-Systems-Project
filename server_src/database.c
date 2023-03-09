
#include "server_headers.h"


/*
 *  Initializes a new record, 
 *  and sets its key and value pointers
 *  to the already allocated 'key' and 'value' strings.
 *
 *    'key' = pointer to an already allocated valid key string
 *    'value' = pointer to an already allocated valid value string
 *
 *    returns a pointer to the new record
 */

recS *initRecord(char *key, char *value){
	if(!key) error("NULL argument");
	recS *newRec = malloc(sizeof(struct recordStruct));
	if(!newRec) error("malloc() failed");
	
	newRec->key = key;
	newRec->value = value;
	return newRec;
}



/*
 *  Deletes a record.
 *  Deallocating both its key and value strings,
 *  and then the record itself.
 *
 *    'rec' = pointer to the record to delete
 */

void delRecord(recS *rec){
	if(!rec) error("NULL argument");
	free(rec->key);
	if(rec->value) free(rec->value);
	free(rec);
}



/*
 *  Allocates a classic array,
 *  capable of holding 2^'power' record pointers.
 *
 *    'power' = the power of 2 from which the size of the array will be calculated
 *
 *    returns a pointer to the newly allocated array
 */

recS **initArr(unsigned power){
	recS **newArr = calloc(twoPow(power), sizeof(recS *));
	if(!newArr) error("calloc() failed");
	return newArr;
}



/*
 *  Initializes a dynamic array,
 *  already capable of holding 2^'power' records.
 *  
 *    'power' = the power of 2 from which the starting size of the array will be calculated
 *  
 *    returns a pointer to the newly allocated dynamic array
 */

dArrS *initDynArr(unsigned power){
	dArrS *newDynArr = calloc(1, sizeof(dArrS));
	if(!newDynArr) error("calloc() failed");
	newDynArr->arr = initArr(power);
	newDynArr->maxSize = twoPow(power);								//set 'maxSize' to 2^'power'
	newDynArr->power = power;
	return newDynArr;
}



/*
 *  Deletes a Dynamic Array.
 *  Deallocating all of its records,
 *  the array, and the dynamicArrayStruct itself.
 *  (assumes that no other processes or threads are modifying the dynamic array)  
 *
 *    'dynArr' = pointer to the Dynamic Array to delete
 */

void delDynArr(dArrS *dynArr){
	if(!dynArr) error("NULL argument");
	for(unsigned long i=0; i<dynArr->size; i++) delRecord(dynArr->arr[i]);
	free(dynArr->arr);
	free(dynArr);
}



/*
 *  Appends a record to a Dynamic Array.
 *  (checking if the array needs to be expanded)
 *  (assumes that no other processes or threads are modifying the dynamic array)  
 * 
 *    'rec' = pointer to an already allocated valid record.
 *    'dynArr' = pointer to a Dynamic Array.
 *
 *    returns 0 if the record has been successfully appended, or
 *    returns 1 if the maximum size of the dynamic array has been reached.
 */

int appendRecToDynArr(recS *rec, dArrS *dynArr){
	if(!rec || !dynArr) error("NULL argument");
	if(dynArr->size+1 > dynArr->maxSize){							//checks if the dynamic array needs to be expanded
		if(dynArr->power>=DYNARR_MAX_POSSIBLE_POWER) return 1;		//check for limit size
		recS **newArr = initArr(dynArr->power+1);					//initialize an array double the size of the current one
		memcpy(newArr, dynArr->arr, dynArr->maxSize * sizeof(recS *)); //move all the records to the new array
		free(dynArr->arr);
		dynArr->arr = newArr;
		dynArr->power++;
		dynArr->maxSize <<= 1;
	}
	dynArr->arr[dynArr->size] = rec;								//append the record
	dynArr->size++;
	return 0;
}



/*
 *  Finds the correct index in the array,
 *  where the record with the key string 'key' should be inserted.
 *  
 *  The correct index is the one where, if the new record,
 *  containing the key string 'key', would be inserted there,
 *  the array would still be alphabetically sorted by the key strings.
 *
 *  This functions is tail recursive.
 *  (needs to be compiled with optimizations enabled to take advantage of it). 
 *  And should not be used directly, because it doesn't handle limit cases.
 *  Use the findIndexFromKey() wrapper instead.
 *
 *    'key' = pointer to a valid key string.
 *    'dynArr' = pointer to a dynamic array.
 *    'retVal' = pointer to an integer variable where the correct index will be saved.
 *    'p1' = index of the first record to be considered.
 *    'p2' = index of the last record to be considered.
 *    
 *    returns 1 if a record with the key string 'key' is already present in the array, else
 *    returns 0
 */

int findIndexFromKeyRecursive(char *key, dArrS *dynArr, unsigned long *retVal, unsigned long p1, unsigned long p2){
	unsigned long diff = p2 - p1;
	int cmp;
	if(diff<=1){													//base case, the correct index is 'p1' or 'p2'
		if(diff<=0){
			if(diff<0) error("This error should never occur");
			*retVal = p1;
			return !strcmp(key, dynArr->arr[p1]->key);
		}
		cmp = strcmp(key, dynArr->arr[p1]->key);
		if(cmp>0){
			*retVal = p2;
			return !strcmp(key, dynArr->arr[p2]->key);
		}
		*retVal = p1;
		return !cmp;
	}

	unsigned long half = (diff>>1) + p1;							//assign to 'half', the index between 'p1' and 'p2'
	cmp = strcmp(key, dynArr->arr[half]->key);
	if(cmp<0) p2 = half;											//if 'key' is "less" than the one at index 'half', 'p2' = 'half'
	else if(cmp>0) p1 = half;										//else if it's "bigger", 'p1' = 'half'
	else{															//if it's equal, the result has been found
		*retVal = half;
		return 1;
	}
	return findIndexFromKeyRecursive(key, dynArr, retVal, p1, p2);
}



/*
 *  Wrapper of the findIndexFromKeyRecursive() function.
 *  (assumes that no other processes or threads are modifying the dynamic array)  
 *
 *    'key' = pointer to a valid key string.
 *    'dynArr' = pointer to a dynamic array.
 *    'retVal' = pointer to an integer variable where the correct index will be saved.
 *    
 *    returns 1 if a record with the key string 'key' is already present in the array, else
 *    returns 0
 */

int findIndexFromKey(char *key, dArrS *dynArr, unsigned long *retVal){
	if(!key || !dynArr || !retVal) error("NULL argument");
	*retVal = 0;
	if(!dynArr->size) return 0;
	if(strcmp(key, dynArr->arr[dynArr->size-1]->key)>0){
		*retVal = dynArr->size;
		return 0;
	}	
	return findIndexFromKeyRecursive(key, dynArr, retVal, 0, dynArr->size-1);
}



/*
 *  Adds a record to a dynamic array.
 *  (checking if the array needs to be expanded)
 *  (assumes that no other processes or threads are modifying the dynamic array)  
 *
 *    'rec' = pointer to an already allocated valid record.
 *    'dynArr' = pointer to a dynamic array.
 *
 *    returns 0 if the record has been successfully added, or
 *    returns 1 if the maximum size of the dynamic array has been reached.
 */

int addRecToDynArr(recS *rec, dArrS *dynArr){
	if(!rec || !dynArr) error("NULL argument");

	/* if the dynamic array is empty or the record should be placed last, append the record */
	if(!dynArr->size || strcmp(rec->key, dynArr->arr[dynArr->size-1]->key)>0)
		return appendRecToDynArr(rec, dynArr);

	unsigned long index;
	if(findIndexFromKey(rec->key, dynArr, &index)){					//if there's already a record with the same key, overwrites it
		delRecord(dynArr->arr[index]);
		dynArr->arr[index] = rec;
		return 0;
	}

	if(dynArr->size+1 > dynArr->maxSize){							//check if the dynamic array needs to be expanded
		if(dynArr->power>=DYNARR_MAX_POSSIBLE_POWER) return 1;		//check for limit size
		recS **newArr = initArr(dynArr->power+1);					//initialize an array double the size of the current one
		memcpy(newArr, dynArr->arr, index*sizeof(recS *));			//correctly move all the records
		memcpy(newArr+index+1, dynArr->arr+index, (dynArr->size-index)*sizeof(recS *));
		free(dynArr->arr);
		dynArr->arr = newArr;
		dynArr->power++;
		dynArr->maxSize <<= 1;
	}																//move of one position, the records after where the new one will be placed
	else memmove(dynArr->arr+index+1, dynArr->arr+index, (dynArr->size-index)*sizeof(recS *));
	dynArr->arr[index] = rec;																
	dynArr->size++;
	return 0;
}



/*
 *  Removes and deletes the record with key string 'key' from a dynamic array.
 *  (the dimensions of the dynamic array will not be eventually halfed,
 *  because it assumes that the space will later be re-used)
 *  (assumes that no other processes or threads are modifying the dynamic array)  
 *
 *  'key' = the key of the record that has to be removed.
 *  'dynArr' = the dynamic array from which has to be removed.
 *
 *    returns 1 if there isn't a record with key string 'key', else
 *    returns 0 the record has been successfully removed
 */ 
int removeRecFromDynArr(char *key, dArrS *dynArr){
	if(!key || !dynArr) error("NULL argument");
	unsigned long index;
	if(!findIndexFromKey(key, dynArr, &index)) return 1;			//if there's not a record with the string key 'key' return 1

	delRecord(dynArr->arr[index]);									//delete record
	if(index+1<dynArr->size) memmove(dynArr->arr+index, dynArr->arr+index+1, (dynArr->size-index-1)*sizeof(recS *)); 
	else if(index+1>dynArr->size) fatalError("This error should never occur"); //paranoic error

	dynArr->size--;
	return 0;
}



/*
 *  Prints a dynamic array, and its stats.
 *
 *    'dynArr' = pointer to a dynamic array.
 */

void printDynArr(dArrS *dynArr){
	if(!dynArr) error("NULL argument");
	printf("\nSize = %lu,   Max Size = %lu,   Power = %u\n\n", dynArr->size, dynArr->maxSize, dynArr->power);
	for(unsigned long i=0; i<dynArr->size; i++) printf("[%lu] Key: \"%s\",  Value: \"%s\"\n", i, dynArr->arr[i]->key, dynArr->arr[i]->value);
	printf("\n\n");
	fflush(stdout);
}



/*
 *  Calculates the minimum needed power
 *  of the dynamic array to store 'n' records.
 *  (basically a log2() function)
 *
 *    'n' = the number of records
 *
 *    returns the required power to store 'n' records
 */

unsigned neededPow(unsigned long n){
	unsigned x = 0;
	while(n){
		n>>=1;
		x++;
	}
	return x;
}



/*
 *  Takes a pointer to a generic record, and merges
 *  its key and value strings togheter in a single string,
 *  separating them with the KEY_VALUE_SEPARATOR char.
 *  Saves the result in the buffer pointed by 'dest'.
 *  
 *  (usefull for sending records from server, or exporting them)
 *  (assumes that the buffer pointed by 'dest' has a size of BUFF_SIZE)
 *
 *  The result string will look like this: "key:value"
 *
 *    'rec' = a pointer to a generic record.
 *    'dest' = a pointer to a buffer where the result will be saved.
 *    
 *    returns the size of the writed string.
 */

size_t recordToString(recS *rec, char *dest){	
	if(!rec || !dest) fatalError("NULL argument");
	size_t keyLen = strlen(rec->key);
	size_t valueLen = rec->value?strlen(rec->value):0;
	if(keyLen+valueLen+1 >= BUFF_SIZE) fatalError("tried copying a string longer than BUFF_SIZE to buffer"); //this error should never occur 
	char *p = dest;

	strcpy(p, rec->key);
	p += keyLen;
	*p++ = KEY_VALUE_SEPARATOR;
	if(rec->value) strcpy(p, rec->value);
	p += valueLen;
	*p = '\0';

	return (size_t) (p - dest);
}



/*
 *  Converts a valid generic record string to a record.
 *
 *  (usefull for receiving records from clients, or importing them)
 *  (assumes that the record string it's already been checked and it's valid)
 *
 *    'str' = string to convert.
 *
 *    returns a pointer to the newly allocated record
 */

recS *stringToRecord(char *str){
	if(!str) error("NULL argument");

	char *p = str;
	while(*p!=KEY_VALUE_SEPARATOR && *p!='\0') p++;
	if(*p=='\0') error("invalid string");							//this error should never occur
	
	size_t keyLen, valueLen;
	*p = '\0';
	keyLen = p - str;
	valueLen = strlen(p+1);
	if(keyLen+valueLen+1>MAX_REC_STR_LEN) error("invalid string");	//superfluous, this error should never occur
	
	char *key = malloc((keyLen + 1) * sizeof(char));
	if(!key) error("malloc() failed"); 
	strcpy(key, str);
	*p = KEY_VALUE_SEPARATOR;

	char *value;
	if(!valueLen){
		value = NULL;
		*++p = '\0';
	}
	else{
		value = malloc((valueLen + 1) * sizeof(char));
		if(!value) error("malloc() failed");
		strcpy(value, p+1);
	}

	return initRecord(key, value);	
}



/*
 *  Exports the dynamic array pointed by 'dynArr' to a file named 'filename'.
 *  Writes the data to a temporary file, and then "renames" it to the final one,
 *  so that if exporting fails, the last export is still valid.
 *  (overwrites the file, if it already exists)
 *  (assumes that no other processes or threads are using the file,
 *  or modifying the Dynamic Array)
 *
 *    'dynArr' = pointer to a dynamic array.
 *    'filename' = the name of the file where will be exported 'dynArr'
 */

void exportDynArr(dArrS *dynArr, char *filename){
	if(!dynArr || !filename) fatalError("NULL argument");
	char tmpFilename[strlen(filename)+5];
	sprintf(tmpFilename, "%s.tmp", filename);

	int fd;
	if((fd = open(tmpFilename, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600))==-1) fatalError("open() failed");

	char *p;
	ssize_t writed;
	size_t recordSize, toWrite;
	char buff[BUFF_SIZE];
	for(unsigned long i=0; i<dynArr->size; i++){
		recordSize = recordToString(dynArr->arr[i], buff);
		buff[recordSize++] = '\n';
		buff[recordSize] = '\0';

		p = buff;
		toWrite = recordSize;
		while(toWrite>0){
			while((writed = write(fd, p, toWrite))<0) if(errno!=EINTR) fatalError("write() failed");
			toWrite -= writed;
			p += writed;
		}
	}
	if(close(fd)==-1) fatalError("close() failed");

	if(unlink(filename)==-1 && errno!=ENOENT) fatalError("unlink() failed");
	if(link(tmpFilename, filename)==-1) fatalError("link() failed");
	if(unlink(tmpFilename)==-1) fatalError("unlink() failed");
}



/*
 *  Imports a dynamic array from a file.
 *  (assumes that no other processes or threads are modifying the file)
 *
 *  'filename' = the name of the file frow which will be imported the dynamic array.
 *  'dynArrType' = the type of dynamic array (only valid options are MAIN_TYPE or USER_TYPE).
 *
 *  returns a pointer to the newly created dynamic array.
 */

dArrS *importDynArr(char *filename, unsigned char dynArrType){
	if(!filename) error("NULL argument");
	if(dynArrType!=MAIN_TYPE && dynArrType!=USER_TYPE) error("invalid dynamic array type");
	
	unsigned long nRec = countFileLines(filename);
	unsigned pow = neededPow(nRec);
	dArrS *dynArr = initDynArr(pow);
	
	int fd;
	if((fd = open(filename, O_RDONLY | O_CREAT, 0600))==-1) error("open() failed");

	char *p1;
	char *p2 = NULL;
	char buff[BUFF_SIZE];
	while(readLineFromFile(fd, buff, &p1, &p2)!=-1){				//read all the lines of the file
		if(!checkRecordString(p1, dynArrType)){						//if the record is valid, add it to the dynamic array
			if(addRecToDynArr(stringToRecord(p1), dynArr)) fatalError("Maximum size of dynamic array reached while importing it.");
		}
		else printf("Tried importing an invalid %s-record: '%s'\n", dynArrType==MAIN_TYPE?"main":"user", p1);
	}

	close(fd);
	return dynArr;
}



/*
 *  This function will be called if a fatal error happened the last time the program was run.
 *  Recovers the main dynamic array, starting by importing the last valid export,
 *  and then replaying the actions done before the fatal-error.
 *  (assumes that no other processes or threads are modifying the files)
 *
 *  returns the recovered dynamic array.
 */

dArrS *recoverMainDynArr(void){
	printNow("Recovering main dynamic array.\n");
	dArrS *dynArr = importDynArr(MAIN_DB_FILENAME, MAIN_TYPE);		//import the last valid export
	
	int fd;
	if((fd = open(RECOVERY_DATA_FILENAME, O_RDONLY | O_CREAT, 0600))==-1) error("open() failed");

	char *p;
	char *p1;
	char *p2 = NULL;
	char buff[BUFF_SIZE];
	size_t readed;
	while((readed = readLineFromFile(fd, buff, &p1, &p2))!=-1){		//read all the lines of the file
		if(readed>0 && !checkRecordString(p1+1, MAIN_TYPE)){		//checks if the record is valid
			if(p1[0]=='1'){											//if the first character of the line is '1', add the record
				if(addRecToDynArr(stringToRecord(p1+1), dynArr)) fatalError("Maximum size of dynamic array reached while recovering it.")
				continue;
			}
			else if(p1[0]=='0'){									//else if it's '0' remove the record
				p = p1 + 1;
				while(*p!=KEY_VALUE_SEPARATOR && *p!='\0') p++;
				*p = '\0';
				removeRecFromDynArr(p1+1, dynArr);
				continue;
			}
		}
		printf("Tried recovering an invalid main-record: '%s'\n", p1);
	}
	printNow("Successfully recovered main dynamic array.\n");
	return dynArr;
}
