/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given a block ptr bp, compute adress of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute adress of net and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PREV_PTR(bp) ((char *)(bp))
#define NEXT_PTR(bp) (((char *)(bp)) + (WSIZE))
#define PRED(bp) (*(char **)(bp))
#define SUCC(bp) (*(char **)(NEXT_PTR(bp)))

/* free list */
void *heap_listp;
void *free_list;

/* Helper functions with 'static' attribute */

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize);
static void insert_node(void *bp, size_t size);
static void delete_node(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
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
    while(bp&& GET_SIZE(HDRP(bp))<asize)
        bp = PRED(bp);
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
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    insert_node(ptr, size);
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
    
    if (ptr == NULL)
        return mm_malloc(size);
    else if (size == 0){
        mm_free(ptr);
        return NULL;
    }
    else {
        asize = ALIGN(size + DSIZE);
        cursize = GET_SIZE(HDRP(ptr));
        if (asize <= cursize){
            PUT(HDRP(ptr), PACK(asize,1));
            PUT(FTRP(ptr), PACK(asize,1));
            if (asize != cursize){
                PUT(HDRP(NEXT_BLKP(ptr)), PACK(cursize-asize,0));
                PUT(FTRP(NEXT_BLKP(ptr)), PACK(cursize-asize,0));
                insert_node(NEXT_BLKP(ptr), cursize-asize);
            }
            return ptr;
        }
        nextptr = NEXT_BLKP(ptr);
        prevptr = PREV_BLKP(ptr);
        nextsize = GET_SIZE(HDRP(nextptr));
        prevsize = GET_SIZE(HDRP(prevptr));
        int tot;
        if (GET_ALLOC(HDRP(prevptr))&&!GET_ALLOC(HDRP(nextptr))){
            tot = nextsize + cursize;
            if (tot >= asize){
                delete_node(nextptr);
                if (tot - asize < 2*DSIZE){
                    PUT(HDRP(ptr), PACK(tot, 1));
                    PUT(FTRP(ptr), PACK(tot, 1));
                }
                else{
                    PUT(HDRP(ptr), PACK(asize, 1));
                    PUT(FTRP(ptr), PACK(asize, 1));
                    PUT(HDRP(NEXT_BLKP(ptr)), PACK(tot-asize,0));
                    PUT(FTRP(NEXT_BLKP(ptr)), PACK(tot-asize,0));
                    insert_node(NEXT_BLKP(ptr), tot-asize);
                }
                return ptr;
            }
       }
        else if (!GET_ALLOC(HDRP(prevptr))&&GET_ALLOC(HDRP(nextptr))){
            tot = prevsize + cursize;
            if (tot >= asize){
                delete_node(prevptr);
                memmove(prevptr, ptr, size);
                if (tot - asize < 2*DSIZE){
                    PUT(HDRP(prevptr), PACK(tot, 1));
                    PUT(FTRP(prevptr), PACK(tot, 1));
                }
                else{
                    PUT(HDRP(prevptr), PACK(asize, 1));
                    PUT(FTRP(prevptr), PACK(asize, 1));
                    PUT(HDRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    PUT(FTRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    insert_node(NEXT_BLKP(prevptr), tot-asize);
                }
                return prevptr;
            }
        }
        else if (!GET_ALLOC(HDRP(prevptr))&&!GET_ALLOC(HDRP(nextptr))){
            tot = prevsize + cursize + nextsize;
            if (tot >= asize){
                delete_node(nextptr);
                delete_node(prevptr);
                memmove(prevptr, ptr, size);
                if (tot - asize < 2*DSIZE){
                    PUT(HDRP(prevptr), PACK(tot, 1));
                    PUT(FTRP(prevptr), PACK(tot, 1));
                }
                else{
                    PUT(HDRP(prevptr), PACK(asize, 1));
                    PUT(FTRP(prevptr), PACK(asize, 1));
                    PUT(HDRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    PUT(FTRP(NEXT_BLKP(prevptr)), PACK(tot-asize,0));
                    insert_node(NEXT_BLKP(prevptr), tot-asize);
                }
                return prevptr;
            }
        }
        newptr = mm_malloc(size);
        memcpy(newptr, ptr, MIN(size, cursize));
        mm_free(ptr);
        return newptr;        
    }
}

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
    insert_node(bp, size);

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc){
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        
        insert_node(bp, size);
    }
    else if (!prev_alloc && next_alloc){
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
        
        insert_node(bp, size);
    }
    else{
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        
        insert_node(bp, size);
    }
    return bp;
}

static void *place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    delete_node(bp);

    if ((csize-asize) >= (2*DSIZE)){
       PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
		insert_node(bp, csize-asize);
        return NEXT_BLKP(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    return bp;
}

static void insert_node(void *bp, size_t size){
    void *head = free_list;
    void *tail = NULL;

    while (head && (size > GET_SIZE(HDRP(head)))){
        tail = head;
        head = PRED(head);
    }

    if (head != NULL){
        if (tail != NULL){
            PUT(PREV_PTR(bp), (unsigned int)head);
            PUT(NEXT_PTR(head), (unsigned int)bp);
            PUT(NEXT_PTR(bp), (unsigned int)tail);
            PUT(PREV_PTR(tail), (unsigned int)bp);
        }
        else {
            PUT(PREV_PTR(bp), (unsigned int)head);
            PUT(NEXT_PTR(head), (unsigned int)bp);
            PUT(NEXT_PTR(bp), (unsigned int)NULL);
            free_list = bp;
        }
    }
    else{
        if (tail != NULL){
            PUT(PREV_PTR(bp), (unsigned int)NULL);
            PUT(NEXT_PTR(bp), (unsigned int)tail);
            PUT(PREV_PTR(tail), (unsigned int)bp);
        }
        else {
            PUT(PREV_PTR(bp), (unsigned int)NULL);
            PUT(NEXT_PTR(bp), (unsigned int)NULL);
            free_list = bp;
        }
    }
}

static void delete_node(void *bp){
    if (PRED(bp) != NULL){
        if (SUCC(bp) != NULL){
            PUT(NEXT_PTR(PRED(bp)), (unsigned int)SUCC(bp));
            PUT(PREV_PTR(SUCC(bp)), (unsigned int)PRED(bp));
        }
        else{
            PUT(NEXT_PTR(PRED(bp)), (unsigned int)NULL);
            free_list = PRED(bp);
        }
    }
    else{
        if (SUCC(bp) != NULL)
            PUT(PREV_PTR(SUCC(bp)), (unsigned int)NULL);
        else
            free_list = NULL;
    }
}

int mm_check(void){

    void *bp;

}