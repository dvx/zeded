/* ZEDED (c) David Titarenco 2010-11
 * Licence: MIT
 * TODO: still plenty of stuff
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>

/* Max pathway memory, ergo pathways aren't turing-complete (and could potentially overflow :P) */
#define MAX_PAGE_SIZE 65535

/* Random macro */
#define RNDLIM(i) (int)(i*rand()/(RAND_MAX+1.0))

/* The VM struct and declaration */
struct __s_zeded_vm {
	const char *appname;					// application name
	const char *version;					// VM version
	unsigned char* shared_memory;			// shared memory between pathways, currently not implemented
	/* The route (code pathway) struct and declaration
	 * TODO: memory should be a variable-size buffer for the sake of Turing-completeness
	 */
	struct __s_vm_route {
		const char *file_name;				// code pathway filename
		unsigned char* buffer_content;		// read the file into this buffer
		unsigned char memory[MAX_PAGE_SIZE];// the pathway memory
		int buffer_size;					// helper buffer size
		int code_pointer;					// current location of code pointer, used to traverse the source
		int memory_pointer;					// current location of memory pointer, used to dump the memory to screen
		int accumulator;					// the accumulator for the particular pathway
	} **pathways;
	int num_pathways;						// how many pathways?
} VM = { NULL };

/* Some helper typedefs for C and C++ (note the scope resolution)
 * TODO: Actually use bool more liberally
 */
#ifdef __cplusplus
	typedef struct __s_zeded_vm::__s_vm_route VM_ROUTE;
#else
	typedef struct __s_vm_route VM_ROUTE;
	typedef int bool;
	enum bool { false, true };
#endif

/* prototypes
 * TODO: Fix some of these prototypes so they take a VM, not pointers (to pointers to pointers) of pathways
 */
void titleCard();								// intro (and help) screen
void initVM(char*);								// set up VM appname and version
int allocVMPathways(VM_ROUTE***, int, char**);	// malloc some memory for the pathways and call all other functions, i.e. initVMPathwayFromFile(), and runVMPathways()
int initVMPathwayFromFile(VM_ROUTE*, char*);	// make sure all files exist and populate pathway buffers
void runVMPathways(VM_ROUTE***, int);			// traverse the pathways, usually simply by code_pointer++
int checkforCollapse(VM_ROUTE***, int);			// check for pathway collapse (ALL pathways must point to the same value for a collapse to return true)
void freeVMPathways(VM_ROUTE***, int);			// clean up the pathways and their buffers
void cleanupVM(int);							// calls freeVMPathways() and also cleans up any memory allocated by the VM

int main(int argc, char *argv[]) {
	initVM(argv[0]);

	if (argc < 3) {
		titleCard();
		exit(0);
	}

	signal(SIGINT, cleanupVM);

	srand((unsigned)(time(0)));

	if (allocVMPathways(&VM.pathways, argc-1, argv) == 0)
		runVMPathways(&VM.pathways, VM.num_pathways);

	cleanupVM(0);
	return 0
}

void titleCard() {
	printf ("Usage: %s p1.bin p2.bin p3.bin ... pn.bin \n", VM.appname);
	printf ("       where at least two execution pathways are provided\n       pathways must be binary files >= 2 bytes\n");
}

int initVMPathwayFromFile(VM_ROUTE *v, char *name) {
	FILE *file;
	unsigned char *buffer;
	unsigned long fileLen;

	//Open file
	file = fopen(name, "rb");
	if (!file) {
		titleCard();
		fprintf(stderr, "Error @ %s: unable to open file\n", name);
		return 1;
	}

	//Get file length
	fseek(file, 0, SEEK_END);
	fileLen=ftell(file);
	//fprintf(stderr, "%d bytes\n", fileLen);
	fseek(file, 0, SEEK_SET);

	//Allocate memory
	buffer = (unsigned char *)malloc(fileLen+1);
	if (!buffer) {
		titleCard();
		fprintf(stderr, "Error @ %s: could not allocate memory\n", name);
		fclose(file);
		return 1;
	}

	//Read file contents into buffer
	fread(buffer, fileLen, 1, file);
	fclose(file);

	//Return buffer
	v->file_name = name;
	v->buffer_size = fileLen;
	v->buffer_content = buffer;

	if (v->buffer_size < 2) {
		titleCard();
		fprintf(stderr, "Error @ %s: not enough steps\n", name);
		return 1;
	}

	return 0;
}

int checkforCollapse(VM_ROUTE ***v, int num) {
	int i, collapse = 1;
	for (i=0;i<num;++i) {
		VM_ROUTE* cvmr = (*v)[i]; // current vm route
		// if XOR of [0] with [n] != 0, then the bytes aren't the same, so turn off the collapse flag
		if ((cvmr->buffer_content[cvmr->code_pointer]^(*v)[0]->buffer_content[(*v)[0]->code_pointer]) && collapse)
			collapse = 0;
	}
	return collapse;
}

void runVMPathways(VM_ROUTE ***v, int num) {
	int i, rnd;
	for (i=0;i<num;++i) {
		VM_ROUTE* cvmr = (*v)[i]; // current vm route
		if (cvmr->buffer_content[cvmr->code_pointer] <= cvmr->buffer_size-1) {
			cvmr->code_pointer = cvmr->buffer_content[cvmr->code_pointer];
		} else {
			cvmr->code_pointer = cvmr->buffer_size-1;
		}
		//printf("LOADED %s: code pointer @ %d - buffer size: %d\n", cvmr->file_name, cvmr->code_pointer, cvmr->buffer_size);
	}
	while (true) {
		rnd = RNDLIM(num);
		for (i=0;i<num;++i) {
			VM_ROUTE* cvmr = (*v)[i]; // current vm route
			if (cvmr->code_pointer < cvmr->buffer_size-1) {
				//printf("[%s] CP[%x]-> %x\n", cvmr->file_name, cvmr->code_pointer, cvmr->buffer_content[cvmr->code_pointer]);
				cvmr->memory[cvmr->memory_pointer] = cvmr->buffer_content[cvmr->code_pointer];
				cvmr->memory_pointer++;

				cvmr->accumulator += cvmr->buffer_content[cvmr->code_pointer];
				cvmr->code_pointer++;
			}
		}

		if (checkforCollapse(v, num) == 1) {
			VM_ROUTE* rvmr = (*v)[rnd]; // random vm route

			// halt code
			if (rvmr->buffer_content[rvmr->code_pointer] == 0xFF)
				return;

			for (i = 0; i < rvmr->memory_pointer; ++i) {
				if (rvmr->memory[i] >= 32 && rvmr->memory[i] <= 126)
					printf("%c", rvmr->memory[i]);
			}

			memset((void*)&rvmr->memory, 0, sizeof(unsigned char)*MAX_PAGE_SIZE);
			rvmr->memory_pointer = 0;
			//printf("%s: collapse on CP[%x]-> %x -- ACC: %x\n", rvmr->file_name, rvmr->code_pointer, rvmr->buffer_content[rvmr->code_pointer], rvmr->accumulator );
			if (rvmr->accumulator <= rvmr->buffer_size-1) {
				rvmr->code_pointer = rvmr->accumulator;
				rvmr->accumulator = 0;
			} else {
				rvmr->code_pointer = rvmr->buffer_size-1;
				rvmr->accumulator = 0;
			}
		}
	}
	return;
}

int allocVMPathways(VM_ROUTE ***v, int num, char **argv) {
	int i;
	*v = (VM_ROUTE**)malloc(sizeof(VM_ROUTE*) * num);
	for (i=0;i<num;++i) {
		(*v)[i] = (VM_ROUTE *)malloc(sizeof(VM_ROUTE));
		// Set initial values. I don't like depending on calloc
		(*v)[i]->accumulator = 0;
		(*v)[i]->code_pointer = 0;
		(*v)[i]->memory_pointer = 0;
		if (initVMPathwayFromFile((*v)[i], argv[i+1]) == 0)
			VM.num_pathways++;
		else
			return 1;
	}
	return 0;
}

void freeVMPathways(VM_ROUTE ***v, int num) {
	int i;
	for (i=0;i<num;++i) {
		free((*v)[i]->buffer_content);
		free((*v)[i]);
	}
}

void initVM(char *_appname) {
	VM.appname = _appname;
	VM.version = "0.1";
}
void cleanupVM(int sig) {
	freeVMPathways(&VM.pathways, VM.num_pathways);
	free(VM.pathways);
	exit(sig);
}