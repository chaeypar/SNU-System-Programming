/*
 * mm-2019-16022.c - The malloc package which balances the utilization of the memory block and the throughput rate
 * 
 * It uses Explicit Free List Implementation for the efficiency and it is implemented based on double word alignment.
 * When it needs to search the available free block with appropriate size, it only searches through the freed blocks. 
 * For each memory block, there is a header and a footer that contains the information about the size and whether it is allocated.
 * In freed memory blocks, they also store the pointer of the previous and the next freed memory block of it.
 *  
 * Especially for the mm_realloc function, there is an improvement for its memory utilization.
 * Instead of just directly using mm_malloc and mm_free, it observes whether it can reuse the original block, its previous block and its next block.
 * 
 * The Perf index of this implementation is affected by the CPU use rate, but it recorded up to 92 points.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

team_t team = {
    "Chaeyeon Park",
    "stu9@sp05.snucse.org"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

/* Return Maximum and Minimum value */
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read or write the value */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given a block ptr bp, compute adress of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute adress of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Basic Macros for Explicit Free List Implementation */
#define PREV_PTR(bp) ((char *)(bp))
#define NEXT_PTR(bp) (((char *)(bp)) + (WSIZE))
#define PRED(bp) (*(char **)(bp))
#define SUCC(bp) (*(char **)(NEXT_PTR(bp)))

void *heap_listp = NULL;
void *free_list = NULL;

/* Helper functions with 'static' attribute */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize);
static void insert_free(void *bp);
static void delete_free(void *bp);
//int mm_check(void);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Initialize free_list with NULL */
    free_list = NULL;

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE))==(void *)-1)
        return -1;

    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    void *bp = free_list;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs */
    asize = ALIGN(size + DSIZE);
    /* Search the free list for a fit */
    while(bp && GET_SIZE(HDRP(bp))<asize)
        bp = PRED(bp);
    
    /* Appropriate memory block found */
    if (bp != NULL){
        bp = place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE))==NULL)
        return NULL;
    bp = place(bp, asize);

    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    //Get the size of the given memory block
    size_t size = GET_SIZE(HDRP(ptr));

    //Update the header and the footer of the given block pointer
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    //Coalesce the memory blocks if the previous block or the next block is freed.
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
     void *nextptr, *prevptr, *newptr;
    size_t nextsize, cursize, prevsize;
    size_t asize;
    
    // If ptr is NULL the call is equivalent to mm_malloc(size)
    if (ptr == NULL)
        return mm_malloc(size);
    // If size is equal to zero, the call is equivalent to mm_free(ptr)
    else if (size == 0){
        mm_free(ptr);
        return NULL;
    }
    // The call to mm_realloc changes the size of the memory block pointed to by ptr to size bytes
    else {
        asize = ALIGN(size + DSIZE);
        cursize = GET_SIZE(HDRP(ptr));

        // If the requested size is less than or equal to the current size of the block
        if (asize <= cursize){
            // If the remaining size of the memory block is too small
            if (cursize-asize < 2*DSIZE){
                PUT(HDRP(ptr), PACK(cursize,1));
                PUT(FTRP(ptr), PACK(cursize,1));
            }
            // Otherwise, split the block for the efficiency of memory use
            else{
                PUT(HDRP(ptr), PACK(asize,1));
                PUT(FTRP(ptr), PACK(asize,1));
                PUT(HDRP(NEXT_BLKP(ptr)), PACK(cursize-asize,0));
                PUT(FTRP(NEXT_BLKP(ptr)), PACK(cursize-asize,0));
                insert_free(NEXT_BLKP(ptr));
            }
            return ptr;
        }
        // If the requested size is greater than or equal to the current size of the block
        nextptr = NEXT_BLKP(ptr);
        prevptr = PREV_BLKP(ptr);
        nextsize = GET_SIZE(HDRP(nextptr));
        prevsize = GET_SIZE(HDRP(prevptr));
        int tot;

        //If the previous block is allocated and the next block is freed
        if (GET_ALLOC(HDRP(prevptr))&&!GET_ALLOC(HDRP(nextptr))){
            tot = nextsize + cursize;

            // If the sum of these two blocks' sizes is greater than or equal to the requested size
            if (tot >= asize){
                delete_free(nextptr);
                //If the remaining size of the memory block is too small
                if (tot - asize < 2*DSIZE){
                    PUT(HDRP(ptr), PACK(tot, 1));
                    PUT(FTRP(ptr), PACK(tot, 1));
                }
                // Otherwise, split the block for the efficiency of the memory use
                else{
                    PUT(HDRP(ptr), PACK(asize, 1));
                    PUT(FTRP(ptr), PACK(asize, 1));
                    PUT(HDRP(NEXT_BLKP(ptr)), PACK(tot-asize,0));
                    PUT(FTRP(NEXT_BLKP(ptr)), PACK(tot-asize,0));
                    insert_free(NEXT_BLKP(ptr));
                }
                return ptr;
            }
       }
       // If the previous block is freed and the next block is allocated
        else if (!GET_ALLOC(HDRP(prevptr))&&GET_ALLOC(HDRP(nextptr))){
            tot = prevsize + cursize;
            // If the sum of these two blocks' sizes is greater than or equal to the requested size
            if (tot >= asize){
                delete_free(prevptr);
                // Copy the payload of the block
                memmove(prevptr, ptr, cursize-DSIZE);
                // If the remaining size of the memory block is too small 
                if (tot - asize < 2*DSIZE){
                    PUT(HDRP(prevptr), PACK(tot, 1));
                    PUT(FTRP(prevptr), PACK(tot, 1));
                }
                // Otherwise, split the block for the efficiency of the memory use
                else{
                    PUT(HDRP(prevptr), PACK(asize, 1));
                    PUT(FTRP(prevptr), PACK(asize, 1));
                    PUT(HDRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    PUT(FTRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    insert_free(NEXT_BLKP(prevptr));
                }
                return prevptr;
            }
        }
        // If both the previous block and the next block is freed
        else if (!GET_ALLOC(HDRP(prevptr))&&!GET_ALLOC(HDRP(nextptr))){
            tot = prevsize + cursize + nextsize;
            // If the sum of these three blocks' sizes is greater than or equal to the requested size
            if (tot >= asize){
                delete_free(nextptr);
                delete_free(prevptr);
                //Copy the payload of the block
                memmove(prevptr, ptr, cursize-DSIZE);
                //
                if (tot - asize < 2*DSIZE){
                    PUT(HDRP(prevptr), PACK(tot, 1));
                    PUT(FTRP(prevptr), PACK(tot, 1));
                }
                //
                else{
                    PUT(HDRP(prevptr), PACK(asize, 1));
                    PUT(FTRP(prevptr), PACK(asize, 1));
                    PUT(HDRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    PUT(FTRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    insert_free(NEXT_BLKP(prevptr));
                }
                return prevptr;
            }
        }
        // Otherwise, we should allocate a new memory block
        newptr = mm_malloc(size);
        memcpy(newptr, ptr, cursize-DSIZE);
        mm_free(ptr);
        return newptr;        
    }
}
/*
* extend_heap - Extending the heap if there is no extra place to allocate
*/
static void *extend_heap(size_t words){
    void *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
* coalesce - Merging the freed blocks
*/
static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // Case 1: The previous block and the next block are allocated
    if (prev_alloc && next_alloc){
        insert_free(bp);
    }
    // Case 2: The previous block is allocated and the next block is freed
    else if (prev_alloc && !next_alloc){
        delete_free(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        
        insert_free(bp);
    }
    // Case 3: The previous block is freed and the next block is allocated
    else if (!prev_alloc && next_alloc){
        delete_free(PREV_BLKP(bp));
        
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        
        insert_free(bp);
    }
    // Case 4: The previous block and the next block is freed
    else{
        delete_free(PREV_BLKP(bp));
        delete_free(NEXT_BLKP(bp));
        
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        
        insert_free(bp);
    }
    return bp;
}

/*
* place - Allocate the memory block
*/
static void *place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    delete_free(bp);

    // If the remaining size is smaller than 2*DSIZE
    if ((csize-asize) < (2*DSIZE)){
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        return bp;
    }
    // If the size of the block that is allocated is bigger than 100 bytes
    else if (asize>100){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 0));
        insert_free(NEXT_BLKP(bp));
        return bp;
    }
    // If the size of the block that is allocated is not bigger than 100 bytes
    else {
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        insert_free(bp);
        return NEXT_BLKP(bp);
    }
}

/*
* insert_free - Insert a freed block into the free_list : Explicit Free List Implementation
*/
static void insert_free(void *bp){
    void *head = free_list;
    void *tail = NULL;
    size_t size = GET_SIZE(HDRP(bp));

    // Freed blocks are sorted according to their size
    while (head && (size > GET_SIZE(HDRP(head)))){
        tail = head;
        head = PRED(head);
    }

    // If there is a block whose size is greater than or equal to that of the block that 'bp' points to
    if (head != NULL){
        // If tail is not NULL, then the block that 'bp' points to should be placed between head and tail
        if (tail != NULL){
            PUT(PREV_PTR(bp), (unsigned int)head);
            PUT(NEXT_PTR(head), (unsigned int)bp);
            PUT(NEXT_PTR(bp), (unsigned int)tail);
            PUT(PREV_PTR(tail), (unsigned int)bp);
        }
        // If tail is NULL, it implies that the block that 'bp' points to is the smallest freed block.
        else {
            PUT(PREV_PTR(bp), (unsigned int)head);
            PUT(NEXT_PTR(head), (unsigned int)bp);
            PUT(NEXT_PTR(bp), (unsigned int)NULL);
            free_list = bp;
        }
    }
    // If there doesn't exist any block whose size is greater than or equal to that of the block that 'bp' points to
    else{
        // If tail is not NULL, it implies that s block that bp points to is the biggest freed block.
        if (tail != NULL){
            PUT(PREV_PTR(bp), (unsigned int)NULL);
            PUT(NEXT_PTR(bp), (unsigned int)tail);
            PUT(PREV_PTR(tail), (unsigned int)bp);
        }
        // If tail is NULL, it implies that there doesn't exist any freed blocks except for the one that 'bp' points to
        else {
            PUT(PREV_PTR(bp), (unsigned int)NULL);
            PUT(NEXT_PTR(bp), (unsigned int)NULL);
            free_list = bp;
        }
    }
}

/*
* delete_free - Delete a freed block from the free_list : Explicit Free List Implementation
*/
static void delete_free(void *bp){
    // If there exists previous freed block
    if (PRED(bp) != NULL){
        // If there exists next freed block 
        if (SUCC(bp) != NULL){
            PUT(NEXT_PTR(PRED(bp)), (unsigned int)SUCC(bp));
            PUT(PREV_PTR(SUCC(bp)), (unsigned int)PRED(bp));
        }
        //If there doesn't exist next freed blcok 
        else{
            PUT(NEXT_PTR(PRED(bp)), (unsigned int)NULL);
            free_list = PRED(bp);
        }
    }
    // If there does't exist previous freed block
    else{
        // If there exists next freed block
        if (SUCC(bp) != NULL)
            PUT(PREV_PTR(SUCC(bp)), (unsigned int)NULL);
        // If there doesn't exist any freed block
        else
            free_list = NULL;
    }
}

/*
* mm_check - Scan the heap and check it for its consistency
* It checkes whether every blcok in the free list is mared as free, whether there are contiguous free blocks that somehow escaped coalescing,
* and whether the pointers in a heap block point to valid heap adresses.
*/
/*
int mm_check(void){

    void *bp = free_list;   
    while (bp){
        if (GET_ALLOC(HDRP(bp))){
            fprintf(stderr, "Every block int he free list is not marked as free.\n");
        }
        else if (PRED(bp) == PREV_BLKP(bp)){
            fprintf(stderr,"There are contiguous free blocks.\n");
        }
        bp = PRED(bp);
    }
    bp = heap_listp;
    while (GET_SIZE(HDRP(bp))){
        if (bp < heap_listp || bp >= mem_sbrk(0)){
            fprintf(stderr,"Pointers in a healp block do not point to valid heap adress\n");
        }
        bp = NEXT_BLKP(bp);
    }
    return 0;
}
*/
