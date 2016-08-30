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
#define checkheap(lineno) mm_checkheap(lineno)
// #define checkheap(lineno)


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* basic constants and macros */
#define WSIZE       4           /* word and header/footer size (bytes) */
#define DSIZE       8           /* double word size (bytes) */
#define CHUNKSIZE   (1 << 12)   /* extend heap by this amount (bytes) */

#define MAX(x, y)       ((x) > (y)? (x) : (y))

/* pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p)) = (val)

/* mov from p1 to p2 */
#define MOV(p2, p1)     PUT(p2, GET(p1))

/* read the size and allocated bit fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)bp - WSIZE)
#define FTRP(bp)        ((char *)bp + GET_SIZE(HDRP(bp)) - DSIZE) 

/* given block ptr bp, compute address of next and previous block */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* given block ptr bp, compute address of pred and succ pointers */
#define PRED(bp)        ((char *)bp)
#define SUCC(bp)        ((char *)bp + WSIZE)

/* go to block pointed by prev and succ pointers */
#define GOTO_PRED(bp)   GET(PRED(bp))
#define GOTO_SUCC(bp)   GET(SUCC(bp))

/* global variables */
static void *heap_listp;    /* pointer to first block of heap */

/* function prototypes for internal helper functions */
static void checkheap(int lineno);
static int  inside(void *ptr, void *begin, void *end);
static void *extend_heap(size_t words);
static void *coalesce(void *ptr);
static void *find_fit(size_t asize);
static void place(void *ptr, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* create the initial empty list */
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1));  /* prologue header */
    PUT(heap_listp + (2*WSIZE), 0);                 /* prologue pred is always NULL */
    PUT(heap_listp + (3*WSIZE), 0);                 /* initialize as NULL */
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1));  /* prologue footer */
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));        /* epilogue header */
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
    if (size <= 2*DSIZE)
        asize = 3*DSIZE;
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
    int heap_size = mem_heapsize();
    void *begin = mem_heap_lo();
    void *end = mem_heap_hi();

    int block = 1;                      /* current/total block number, initial=prologue */
    int fblock = 0;                     /* accumlated free blocks */
    int ablock = 1;                     /* accumlated allocated blocks */
    int size_accum = (16+1) * WSIZE;    /* accumlated block size, initial=prologue+epilogue */
    void *ptr = heap_listp;             /* implicit list start */
    // void *fptr = heap_listp;            /* free list start */
    int size;
    int alloc;

    // int prologue_valid = 0;     /* prologue is legal, size, address and pointers */
    // int epilogue_valid = 0;     /* epilogue is legal, size and address */
    // int hdr_ftr = 0;            /* header == footer */
    // int coalescing = 0;         /* check if free blocks coalesced immediately */
    // int ptr_valid = 0;          /* point to valid address */ 
    // int ptr_mutual = 0;         /* pointers' relation of ptr, pred, and succ are right */
    // int free_in_list = 0;       /* all free blocks are in free list */
    // int list_all_free = 0;      /* all blocks in free list are free */
    // int size_consis = 0;   /* total blocks = free + alloc, total size = free_size + alloc_size */
    // int block_consis = 0;

    /* check prologue and epilogue of heap */ 
    if (GET(begin) != 0 || GOTO_PRED(heap_listp) != 0)
        printf("prologue corrupted");
    else if (GET_SIZE(HDRP(heap_listp)) != 16 || GET_ALLOC(HDRP(heap_listp)) != 1)
        printf("prologue corrupted");
    else if (GET(HDRP(heap_listp)) != GET(FTRP(heap_listp)))
        printf("prologue corrupted");
    if (GET(((char *)end) - 3) != 1)
        printf("epilogue corrupted");

    /* loop over whole heap, block by block */
    while ((size = GET_SIZE(HDRP(NEXT_BLKP(ptr)))) != 0) {  /* not include epilogue */
        block++;
        size_accum += size;
        ptr = NEXT_BLKP(ptr);
        alloc = GET_ALLOC(HDRP(ptr));
        (alloc == 1) ? ablock++ : fblock++;
        void *pptr = PREV_BLKP(ptr);
        void *nptr = NEXT_BLKP(ptr);
        int palloc = GET_ALLOC(HDRP(pptr));
        int nalloc = GET_ALLOC(HDRP(nptr));
        /* check header and footer */
        if (HDRP(ptr) != FTRP(ptr))
            printf("header not equal to footer at block %d\n", block);
        /* check address inside bound */
        if (!inside(ptr, begin, end))
            printf("pointer of block %d out of bound, which is %p\n", block, ptr);
        /* check coalesce */
        if (!alloc && (!palloc || !nalloc))
            printf("coalesc error of block %d -> | %d | %d | %d |\n", 
                block, palloc, alloc, nalloc);
    }

    /* loop over free list */
    for (ptr = heap_listp; ptr; ptr = (void *)GOTO_SUCC(ptr)) {
        void *pred = (void *)GOTO_PRED(ptr);
        void *succ = (void *)GOTO_SUCC(ptr);
        if (!inside(pred, begin, end))
            printf("pred of free block at address %p out of bound, " 
                "point to address %p\n", ptr, pred);
        if (!inside(succ, begin, end))
            printf("succ of free block at address %p out of bound, " 
                "point to address %p\n", ptr, succ);
    }

    if (size_accum != heap_size)
        printf("heap size mismatch, should be %d bytes, but get %d bytes", 
            heap_size, size_accum);
    if (block != fblock + ablock)
        printf("block number mismatch, totally %d blocks, "
            "but get %d free + %d alloc\n", block, fblock, ablock);

    // while ((nsize = GET_SIZE(HDRP(nptr))) > 0) {
    //     alloc = GET_ALLOC(HDRP(nptr));
    //     printf("block %d has header size %d and alloc %d, at %p\n", block, nsize, alloc, nptr);
    //     block++;
    //     nptr = NEXT_BLKP(nptr);
    // }
    // printf("totally %d blocks\n", block);
}

/* check if address of ptr inside heap bound */
int inside(void *ptr, void *begin, void *end)
{
    int left = (unsigned int *)ptr >= (unsigned int *)begin;
    int right = (unsigned int *)ptr <= (unsigned int *)end;
    return !(left + right);
}

/* internal helper routines */

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
    PUT(HDRP(ptr), PACK(size, 0));          /* free block header */
    PUT(PRED(ptr), 0);                      /* initialize pred as NULL */
    PUT(SUCC(ptr), 0);                      /* initialize succ as NULL */
    PUT(FTRP(ptr), PACK(size, 0));          /* free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));  /* new epilogue header */

    /* coalesce if the previous block was free */
    return coalesce(ptr);
}

/*
 * boundary-tags coalescing helper function
 */
void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));              /* current block's size */

    if (prev_alloc && next_alloc) {         /* | alloc | ptr | alloc | */
        /* set pointers for newly freed block */
        PUT(PRED(ptr), (size_t)heap_listp);         /* start <- pred */
        MOV(SUCC(ptr), SUCC(heap_listp));           /* succ -> old first, won't bother if only 1 free block */
        /* set pred pointer to new block for old first free block */
        if (GOTO_SUCC(heap_listp) != 0)             /* when first extend happened in mm_init */
            PUT(PRED(GOTO_SUCC(heap_listp)), (size_t)ptr);
        /* set succ to newly freed block for start block */
        PUT(SUCC(heap_listp), (size_t)ptr);
        return  ptr;
    }

    else if (prev_alloc && !next_alloc) {   /* | alloc | ptr | free | */
        void *nptr = NEXT_BLKP(ptr);
        size += GET_SIZE(HDRP(nptr));

        /* set pointers for pred block and succ block of next block */
        MOV(SUCC(GOTO_PRED(nptr)), SUCC(nptr));     /* n_pred -> n_succ */
        MOV(PRED(GOTO_SUCC(nptr)), PRED(nptr));     /* n_pred <- n_succ */
        /* set pointers for newly freed block */
        PUT(PRED(ptr), (size_t)heap_listp);
        MOV(SUCC(ptr), SUCC(heap_listp));
        /* set pred pointer to new block for old first free block */
        PUT(PRED(GOTO_SUCC(heap_listp)), (size_t)ptr);
        /* set succ to newly freed block for start block */
        PUT(SUCC(heap_listp), (size_t)ptr);

        /* set header and footer */
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {   /* | free | ptr | alloc | */
        void *pptr = PREV_BLKP(ptr);
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));

        /* set pointers for pred block and succ block of prev block */
        MOV(SUCC(GOTO_PRED(pptr)), SUCC(pptr));     /* p_pred -> p_succ */
        MOV(PRED(GOTO_SUCC(pptr)), PRED(pptr));     /* p_pred <- p_succ */
        /* set pointers for prev free block, which is larger now */
        PUT(PRED(pptr), (size_t)heap_listp);
        MOV(SUCC(pptr), SUCC(heap_listp));
        /* set pred pointer to new block for old first free block */
        PUT(PRED(GOTO_SUCC(heap_listp)), (size_t)pptr);
        /* set succ to newly freed block for start block */
        PUT(SUCC(heap_listp), (size_t)pptr);

        /* set header and footer */
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(pptr), PACK(size, 0));
        ptr = pptr;
    }

    else {                                  /* | free | ptr | free | */
        void *pptr = PREV_BLKP(ptr);
        void *nptr = NEXT_BLKP(ptr);
        size += GET_SIZE(HDRP(pptr)) + GET_SIZE(FTRP(nptr));

        /* set pointers for pred block and succ block of prev block */
        MOV(SUCC(GOTO_PRED(pptr)), SUCC(pptr));     /* p_pred -> p_succ */
        MOV(PRED(GOTO_SUCC(pptr)), PRED(pptr));     /* p_pred <- p_succ */
        /* set pointers for pred block and succ block of next block */
        MOV(SUCC(GOTO_PRED(nptr)), SUCC(nptr));     /* n_pred -> n_succ */
        MOV(PRED(GOTO_SUCC(nptr)), PRED(nptr));     /* n_pred <- n_succ */
        /* set pointers for prev free block, which is larger now */
        PUT(PRED(pptr), (size_t)heap_listp);
        MOV(SUCC(pptr), SUCC(heap_listp));
        /* set pred pointer to new block for old first free block */
        PUT(PRED(GOTO_SUCC(heap_listp)), (size_t)pptr);
        /* set succ to newly freed block for start block */
        PUT(SUCC(heap_listp), (size_t)pptr);

        /* set header and footer */
        PUT(HDRP(pptr), PACK(size, 0));
        PUT(FTRP(nptr), PACK(size, 0));
        ptr = pptr;
    }

    return ptr;
}


/*
 * find_fit - search free list for suitable free block
 */
void *find_fit(size_t asize)
{
    /* first fit search */
    void *nptr = heap_listp;

    for (; GET_SIZE(HDRP(nptr)) > 0; nptr = (void *)GOTO_SUCC(nptr)) {
        if (GET_SIZE(HDRP(nptr)) >= asize)
            return nptr;
    }
    return NULL;    /* no fit */
}

/*
 * place - place requested block at the beginning of free block,
 * split only when size of remainder equal or exceed the minimum bock size
 */
void place(void *ptr, size_t asize)
{
    size_t size = GET_SIZE(HDRP(ptr));
    size_t diff = size - asize;

    if (diff < (3*DSIZE)) {     /* use whole block if left size is less than minimum size */
        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));
    }

    else {                      /* segmente to 2 blocks */
        /* set header and footer */
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(diff, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(diff, 0));

        void *nptr = NEXT_BLKP(ptr);

        /* set pointers for pred block and succ block of ptr */
        if (GOTO_SUCC(ptr) != 0) {                       /* do this only if there are more than 1 free blocks */
            MOV(SUCC(GOTO_PRED(ptr)), SUCC(ptr));   /* pred -> succ */
            MOV(PRED(GOTO_SUCC(ptr)), PRED(ptr));   /* pred <- succ */
        }
        /* set pointers for newly segmented block */
        PUT(PRED(nptr), (size_t)heap_listp);        /* start <- nptr */
        if (GOTO_SUCC(ptr) != 0) 
            MOV(SUCC(nptr), SUCC(heap_listp));      /* nptr -> old first */
        else 
            PUT(SUCC(nptr), 0);                     /* nptr -> NULL */
        /* set pred pointer to new block for old first free block */
        PUT(PRED(GOTO_SUCC(heap_listp)), (size_t)nptr); /* won't bother even if only 1 free block */
        /* set succ to newly freed block for start block */
        PUT(SUCC(heap_listp), (size_t)nptr);
    } 
}













