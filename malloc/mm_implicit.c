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
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* trun on or off heap checker */
// #define checkheap(lineno) mm_checkheap(lineno)
#define checkheap(lineno)


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* basic constants and macros */
#define WSIZE       4           /* word and header/footer size (bytes) */
#define DSIZE       8           /* double word size (bytes) */
#define CHUNKSIZE   (1 << 12)   /* extend heap by this amount (bytes) */

#define MAX(x, y)   ((x) > (y)? (x) : (y))

/* pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p)) = (val)

/* read the size and allocated bit fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* given block ptr bp, compute address of its header and footer */
#define HDRP(bp)     ((char *)bp - WSIZE)
#define FTRP(bp)     ((char *)bp + GET_SIZE(HDRP(bp)) - DSIZE) 

/* given block ptr bp, compute address of next and previous block */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* global variables */
static void *heap_listp;    /* pointer to first block of heap */

/* function prototypes for internal helper functions */
static void checkheap(int lineno);
static void *coalesce(void *ptr);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *ptr, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* create the initial empty list */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /* epilogue header */
    heap_listp += (2*WSIZE);

    /* extend the empty heap with a free block of CHUNKSIZE bytes */
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
    size_t asize;       /* adjusted block size */
    size_t extendsize;  /* amount to extend heap if not fit */
    char *ptr;

    /* ignore spurious requests */
    if (size == 0)
        return NULL;

    /* adjust block size to include overhead and alignment reuqirements */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else 
        asize = ALIGN(size + SIZE_T_SIZE);

    /* search the free list for a fit */
    if ((ptr = find_fit(asize)) != NULL) {
        place(ptr, asize);
        checkheap(__LINE__);
        return ptr;
    }

    /* no fit found, get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((ptr = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(ptr, asize);
    checkheap(__LINE__);
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == 0)
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    checkheap(__LINE__);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * mm_checkheap - check the heap of correctness.
 */
void mm_checkheap(int lineno)
{
    printf("\n");
    printf("checkheap called from line : %d\n", lineno);
    printf("current heap size is %d bytes\n", mem_heapsize());
    printf("block 0 is at address 0x%p\n", heap_listp);

    /* traverse through all blocks in heap */
    int block = 0;
    void *nptr = NEXT_BLKP(heap_listp);
    int nsize;
    int alloc;

    while ((nsize = GET_SIZE(HDRP(nptr))) > 0) {
        alloc = GET_ALLOC(HDRP(nptr));
        printf("block %d has header size %d and alloc %d\n", block, nsize, alloc);
        block++;
        nptr = NEXT_BLKP(nptr);
    }
    printf("totally %d blocks\n", block);
}

/* internal helper routines */

/*
 * boundary-tags coalescing helper function
 */
void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (prev_alloc && next_alloc) {         /* case 1 */
        return  ptr;
    }

    else if (prev_alloc && !next_alloc) {   /* case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {   /* case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }

    else {                                  /* case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + 
            GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    return ptr;
}

/*
 * extentd_heap - extends the heap with a new free block
 */
void *extend_heap(size_t words)
{
    char *ptr;
    size_t size;

    /* allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(ptr = mem_sbrk(size)) == -1)
        return NULL;

    /* initialize free block header/footer and the epilogue header */
    PUT(HDRP(ptr), PACK(size, 0));           /* free block header */
    PUT(FTRP(ptr), PACK(size, 0));           /* free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));   /* new epilogue header */
    // PUT(FTRP(bp) + WSIZE, PACK(0, 1));   /* this also works */

    /* coalesce if the previous block was free */
    return coalesce(ptr);
}

/*
 * find_fit - search free list for suitable free block
 */
void *find_fit(size_t asize)
{
    /* first fit search */
    void *nptr = NEXT_BLKP(heap_listp);
    size_t nsize;

    while((nsize = GET_SIZE(HDRP(nptr))) > 0) {
        if (!GET_ALLOC(HDRP(nptr)) && nsize >= asize)
            return nptr;
        else
            nptr = NEXT_BLKP(nptr);
    }
    return NULL;    /* no fit */
}

/*
 * place - place requested block at the beginning of free block,
 * split only when size of remainder equal or exceed the minimum bock size
 */
void place(void *ptr, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(ptr));
    size_t diff = csize - asize;

    if (diff < (2*DSIZE)) {     /* use whole block if left size smaller than minimum size */
        PUT(HDRP(ptr), PACK(csize, 1));
        PUT(FTRP(ptr), PACK(csize, 1));
    }

    else {                      /* segmente to 2 blocks */
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(diff, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(diff, 0));
    } 
}













