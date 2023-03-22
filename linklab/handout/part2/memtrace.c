//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
  mallocp=dlsym(RTLD_NEXT, "malloc");
  if ((error=dlerror())!=NULL)
    exit(1);
  
  freep=dlsym(RTLD_NEXT, "free");
  if ((error=dlerror())!=NULL)
    exit(1);

  callocp=dlsym(RTLD_NEXT, "calloc");
  if ((error=dlerror())!=NULL)
    exit(1);

  reallocp=dlsym(RTLD_NEXT, "realloc");
  if ((error=dlerror())!=NULL)
    exit(1);
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...

  LOG_STATISTICS(n_allocb, n_allocb/(n_malloc+n_calloc+n_realloc), n_freeb);
  if (n_allocb-n_freeb > 0){
    LOG_NONFREED_START();
    
    item *cur = list->next;
    while (cur){
      if (cur->cnt>0){
        LOG_BLOCK(cur->ptr, cur->size, cur->cnt);
      }
      cur=cur->next;
    }
  }
  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...

void *malloc(size_t size){
  
  void *tp=mallocp(size);
  alloc(list, tp, size);

  LOG_MALLOC(size, tp);
  n_malloc++;
  n_allocb+=size;

  return tp;
}

void *calloc(size_t nmemb, size_t size){

  void *tp=callocp(nmemb, size);
  alloc(list, tp, nmemb*size);

  LOG_CALLOC(nmemb, size, tp);
  n_calloc++;
  n_allocb+=nmemb*size;

  return tp;
}

void *realloc(void *ptr, size_t size){
  
  item *cur=find(list, ptr);
  if (cur&&cur->cnt>0){
    n_freeb+=cur->size;
    dealloc(list, ptr);
  }

  void *tp=reallocp(ptr, size);
  alloc(list, tp, size);

  LOG_REALLOC(ptr, size, tp);
  n_realloc++;
  n_allocb+=size;

  return tp;
}

void free(void *ptr){
  LOG_FREE(ptr);

  item *cur=find(list, ptr);
  if (cur&&cur->cnt>0){
    n_freeb+=cur->size;
    dealloc(list, ptr);
  }
  freep(ptr);
}