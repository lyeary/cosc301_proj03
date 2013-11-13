#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>


#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// function declarations
void *mmalloc(size_t);
void mfree(void *);
void dump_memory_map(void);
void modify_header(int*, int, int);
//void get_header(int*, int*, int*);
int get_offset(int*, int*);
int is_buddy(int*, int*);
int req_size(int);
int* buddy(int, int*);
void rebuddy();
int* find_block(int);
int inFreelist(int*);
void update_freelist(int*);


//const int HEAPSIZE = (1*1024*1024); // 1 MB
const int HEAPSIZE = 1024;
const int MINIMUM_ALLOC = sizeof(int) * 2;

// global file-scope variables for keeping track
// of the beginning of the heap.
int *heap_begin = NULL;
int *freelist = NULL;


void *mmalloc(size_t request_size) {
    // if heap_begin is NULL, then this must be the first
    // time that malloc has been called.  ask for a new
    // heap segment from the OS using mmap and initialize
    // the heap begin pointer.
    if (!heap_begin) {
        heap_begin = mmap(NULL, HEAPSIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	modify_header(heap_begin, HEAPSIZE, 0);
	freelist = heap_begin;
        //atexit(dump_memory_map);
    }
    int u_size = req_size((int)request_size);
    //printf("U SIZE %d\n", u_size);
    int* result = find_block(u_size);

    if (result == NULL) {
        printf("No Memory Allocated\n");
	return NULL;
    }
    int* allocate_block = buddy(u_size, result);
    if (allocate_block == freelist) {
        int freelist_offset = *(freelist + 1);
	freelist = (freelist + ((freelist_offset)/4) );
    }
    else {
	update_freelist(allocate_block);
    }

    *(allocate_block + 1) = 0;
    allocate_block = allocate_block + 2;
    void* toReturn = (void*)allocate_block;
    return toReturn;
}

void mfree(void *memory_block) {
    int* mem = (int*)memory_block;
    mem = mem - 2;
    int size = *(mem + 0); 
    if (freelist > mem) { //adding to front of freelist
	int new_offset = get_offset(mem, freelist);
        //printf("offset = %d\n", new_offset);
	modify_header(mem, size, new_offset);
	freelist = mem;
    }
    else {
	int n=1;
    	int* s = freelist;
    	while (n==1) {
	    int* next_free_block = s + (*(s + 1)/4);
	    if (next_free_block > mem) {
                //printf("freeing size %d\n", size);
	    	int new_off = get_offset(s, mem);
	    	int mem_new_off = get_offset(mem, next_free_block); 
		modify_header(s, *(s+0), new_off);
                //printf("new offset for s %d\n", *(s+1));	    	
		modify_header(mem, size, mem_new_off);
                //printf("new offset for mem %d\n", *(mem+1));
	   	 n=0;
	    }
            s = next_free_block;
    	}
    }
    //puts("Beginning rebuddy()");
    rebuddy(); 
}

void dump_memory_map() { 
    int* s = heap_begin;
    int flag = 1;
    int size;
    printf("\n");
    printf("Dump Memory Map: \n");
    while (flag) {
	size = *(s+0);
	int result = get_offset(heap_begin, s);
	if (size+result == HEAPSIZE) {
	    printf("Block size: %d", size);
	    printf(" Offset %d", result);
            int i = inFreelist(s);
            if (i) {
            	printf(" Free\n");
	    }
	    else {
            	printf(" Allocated\n");
	    }
            	printf("\n");
		flag = 0;
	}
        else if ( (*(s + 1) == 0)) {
	    printf("Block size: %d", size);
	    printf(" Offset %d", result);
            printf(" Allocated\n");
        }
	else {
	    printf("Block size: %d", size);
	    printf(" Offset %d", result);
            printf(" Free\n");
	}
	s = s + (size/4);
    }
}

int inFreelist(int* block) {
   int* f = freelist;
   int check = 0;
   int end = 0;
   while( end==0 ) {
    //printf("block address: %p\n", block);
    //printf("f address: %p\n", f);
     if (f == block){ 
	    check = 1;
	    end=1;
      }
      if (*(f+1) == 0) {
         //printf("ending with %d\n", *(f+0));
         end=1;
      }
      f = f + ((*(f + 1))/4);
	
   }
   return check;
}

void modify_header(int* block, int size, int offset) {
    *(block + 0) = size;
    *(block + 1) = offset;
}

int* find_block(int size) {
    int* temp = freelist;

    if ((*(temp + 1))==0) { //initially
	return temp;
    }
    while ((*(temp + 1))!=0) {
	if ( (*(temp + 0)) < size) {
	    temp = temp + (*(temp + 1)/4);
	}
	else {
	    return temp;
	}
    }
    return NULL;	
}

//void get_header(int* block, int* size, int* offset) {
//    *size = *(block + 0);
//    *offset = *(block + 1);
//}

int get_offset(int* first_block, int* second_block) { 
    return (int)((second_block-first_block)*4);
}

int is_buddy(int* first_block, int* second_block) {
    int count = 0;
    uint64_t first = (uint64_t) (first_block-heap_begin);
    uint64_t second = (uint64_t) (second_block-heap_begin);
    int i=0;
    uint64_t xor = (first^second);
    for (; i<64; i++) {
	if ((xor&0x1)==1) {
	    count++;
	}
        xor = xor>>1;
    }
    if (count==1) {
	return 1;
    }
    else {
	return 0;
    } 	
}

int req_size(int size) {
    size = size+8;
    int i = MINIMUM_ALLOC;
    int flag = 0;
    while(flag==0) {
	if (i>=size) {
	    flag=1;
	}
	else {
	i = i*2;
	}
    }
    return i;
}

int* buddy(int size, int* block) {
    int next_size = (*(block + 0))/2;
    if (next_size < size) { 
	return block;
    }
    else {
	int off = *(block + 1);  
        //printf("off = %d\n", off);
	modify_header(block, next_size, next_size);
	if (off==0) {
	    int* new_block = (block+(next_size/4));
	    modify_header( new_block , next_size, 0);
	}
	else {
            int* new_block = (block+(next_size/4));
	    modify_header( new_block , next_size, (off-next_size)); 
	}
	buddy (size, block); 
    }
}

void update_freelist(int* block) {
    int flag=1;
    int* temp = freelist;
    int* s = temp + (*(temp+1)/4); 
    while (flag) { 
        if (s==block) {
	    int new_offset = (*(temp+1)) + (*(s+1));
            modify_header(temp, *(temp+0), new_offset);
	    flag=0;
        }
        else {
	    s = s + (*(s+1)/4);
        }
    }
}

void rebuddy() {
  int* search = freelist;
  int* check_buddy;
  int all_good;
  int flag = 1;

  while (flag) {
  all_good = 0;
    while ( (*(search + 1)) != 0) {
	check_buddy = search + (*(search + 1)/4);
	if ( (*(search + 0)) == *(check_buddy + 0) ){
	    int result = is_buddy(search, check_buddy); 
	    if (result==1) {
                if ( (*(check_buddy+1))==0 ) {
		    modify_header(search,((*(search+0))+(*(check_buddy+0))), 0);
		}
		else {
		    modify_header(search,((*(search+0))+(*(check_buddy+0))), (*(search + 1))+(*(check_buddy+1)));
		}
		all_good++;
		search = check_buddy + (*(check_buddy + 1)/4);
		check_buddy = search + (*(search + 1)/4);
	    }
	    else {
		search = check_buddy;
		check_buddy = check_buddy + (*(check_buddy + 1)/4);
	    }
	}
	else {
	     search = check_buddy;
	     check_buddy = check_buddy + (*(check_buddy + 1)/4);
      }
    }
      if (all_good==0) {
        flag=0;
      }

  search = freelist;
  }
}


