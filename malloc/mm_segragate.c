/*
 * mm-segragate.c - segragated fits implementation
 *
 * Block layout is as follow :
 *
 * pro_h : prologue header
 * pro_f : prologue footer 
 * epi_h : epilogue header
 * pred  : pointer to previous free block
 * succ  : pointer to successory free block
 * size_list : fix sized array of 9 size classes, partitioned by power of 2:
 *
 * { 2^4+1 - 2^5 }, { 2^5+1 - 2^6 }, { 2^6+1 - 2^7 }, ... { 2^11+1 - 2^12 }, { 2^12+1 - infinity}
 *
 * |         |                       block 1                               |  ...  |         |    block index
 * |  start  |  pro_h  |  { NULL }, { group_1 }, ... { group_9 } |  pro_f  |  ...  |  epi_h  |    block layout
 * |    0    |   48/1  |                   array of *            |  48/1   |  ...  |   0/1   |    block content
 * |    4    |    4    |                      40                 |   4     |  ...  |    4    |    bytes
 *                     ^
 *                     |
 *                 heap_listp
 *
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
#define ARRAYSIZE   40          /* one (4 bytes) NULL pointer, nine size classes, totally 40 (bytes) */

#define MAX(x, y)       ((x) > (y)? (x) : (y))
#define MIN(x, y)       ((x) > (y)? (y) : (x))

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

/* get group lower bound from group pointer */
#define GET_GROUP_LO(p)    (int)(((p - heap_listp)/WSIZE) + 4)

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
#define GOTO_PRED(bp)   (void *)(GET(PRED(bp)))
#define GOTO_SUCC(bp)   (void *)(GET(SUCC(bp)))

/* global variables */
static void *heap_listp;    /* pointer to first block of heap */
static int  step = 0;       /* step number of trace file */

/* function prototypes for internal helper functions */
static void *get_group(void *ptr);
static void *extend_heap(size_t words);
static void *coalesce(void *ptr);
static void *find_fit(size_t asize);
static void place(void *ptr, size_t asize);
static int  inside(void *ptr, void *begin, void *end);
static void checkheap(int lineno);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    int i;
    /* create the initial empty list */
    if ((heap_listp = mem_sbrk(4*WSIZE+ARRAYSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(2*WSIZE+ARRAYSIZE, 1));  /* prologue header */
    for (i = 2; i <= 11; i++) {                      /* size list */
        PUT(heap_listp + (i*WSIZE), 0);
    }
    PUT(heap_listp + (12*WSIZE), PACK(2*WSIZE+ARRAYSIZE, 1));  /* prologue footer */
    PUT(heap_listp + (13*WSIZE), PACK(0, 1));        /* epilogue header */
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
    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    void *oldptr = ptr;
    size_t old_size;
    void *newptr;
    size_t copySize;
    
    /* alloc block with enough size */    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    /* copy contents inside old block to new */
    old_size = GET_SIZE(HDRP(ptr)) - WSIZE;  /* don't copy header */
    copySize = (size_t)MIN(size, old_size);
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


/* internal helper routines */

/*
 * get_group - return (WSIZE before) start address of the group, where ptr belongs to 
 */
void *get_group(void *ptr)
{
    int i;
    size_t size = GET_SIZE(HDRP(ptr));
    for (i = 4; i <= 12; i++) {
        if (i == 12)
            return (char *)heap_listp + (8*WSIZE);      /* last size group { 2^12+1 - infinity } */
        else if (size >= (1 << i) + 1 && size <= (1 << (i+1)))
            return (char *)heap_listp + (i-4)*WSIZE;
    }
    return NULL;
}

void *get_group_by_size(size_t size)
{
    int i;
    for (i = 4; i <= 12; i++) {
        if (i == 12)
            return  (char *)heap_listp + (8*WSIZE);  /* last size group { 2^12+1 - infinity } */
        else if (size >= (1 << i) + 1 && size <= (1 << (i+1)))
            return (char *)heap_listp + (i-4)*WSIZE;
    }
    return NULL;
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
    PUT(HDRP(ptr), PACK(size, 0));          /* free block header */
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

        void *group = get_group(ptr);                   /* get block's group start address */
        /* set pointers for newly freed block */
        PUT(PRED(ptr), (size_t)group);         /* start <- pred */
        MOV(SUCC(ptr), SUCC(group));           /* succ -> old first, won't bother if only 1 free block */
        /* set pred pointer to new block for old first free block */
        if (GOTO_SUCC(group) != 0)             /* when first extend happened in mm_init */
            PUT(PRED(GOTO_SUCC(group)), (size_t)ptr);
        /* set succ to newly freed block for start block */
        PUT(SUCC(group), (size_t)ptr);
        return  ptr;
    }

    else if (prev_alloc && !next_alloc) {   /* | alloc | ptr | free | */

        void *nptr = NEXT_BLKP(ptr);
        size += GET_SIZE(HDRP(nptr));
        /* set header and footer */
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
        void *cgroup = get_group(ptr);                  /* block's group after coalescing */

        /* set pointers for pred block and succ block of next block */
        if (GOTO_SUCC(nptr) != 0) { 
            MOV(SUCC(GOTO_PRED(nptr)), SUCC(nptr));         /* n_pred -> n_succ */
            MOV(PRED(GOTO_SUCC(nptr)), PRED(nptr));     /* n_pred <- n_succ */
        }
        else 
            PUT(SUCC(GOTO_PRED(nptr)), 0);
        /* set pointers for newly freed block */
        PUT(PRED(ptr), (size_t)cgroup);                 /* cgroup <- ptr */
        if (GOTO_SUCC(cgroup) != 0)
            MOV(SUCC(ptr), SUCC(cgroup));                   /* succ -> old first */
        else
            PUT(SUCC(ptr), 0);
        if (GOTO_SUCC(cgroup) != 0) 
            /* set pred pointer to new block for old first free block */
            PUT(PRED(GOTO_SUCC(cgroup)), (size_t)ptr);  /* old free -> ptr */
        /* set succ to newly freed block for start block */
        PUT(SUCC(cgroup), (size_t)ptr);                 /* cgroup -> ptr */

    }

    else if (!prev_alloc && next_alloc) {   /* | free | ptr | alloc | */

        void *pptr = PREV_BLKP(ptr);
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        /* set header and footer */
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(pptr), PACK(size, 0));
        void *cgroup = get_group(pptr);                 /* block's group after coalescing */

        /* set pointers for pred block and succ block of previous block */
        if (GOTO_SUCC(pptr) != 0) { 
            MOV(SUCC(GOTO_PRED(pptr)), SUCC(pptr));     /* p_pred -> p_succ */
            MOV(PRED(GOTO_SUCC(pptr)), PRED(pptr));     /* p_pred <- p_succ */
        }
        else 
            PUT(SUCC(GOTO_PRED(pptr)), 0);

        /* set pointers for newly freed block */
        PUT(PRED(pptr), (size_t)cgroup);
        if (GOTO_SUCC(cgroup) != 0)
            MOV(SUCC(pptr), SUCC(cgroup));
        else
            PUT(SUCC(pptr), 0);
        if (GOTO_SUCC(cgroup) != 0) 
            /* set pred pointer to new block for old first free block */
            PUT(PRED(GOTO_SUCC(cgroup)), (size_t)pptr);
        /* set succ to newly freed block for start block */
        PUT(SUCC(cgroup), (size_t)pptr);

        ptr = pptr;
    }

    else {                                  /* | free | ptr | free | */

        void *pptr = PREV_BLKP(ptr);
        void *nptr = NEXT_BLKP(ptr);
        size += GET_SIZE(HDRP(pptr)) + GET_SIZE(FTRP(nptr));
        /* set header and footer */
        PUT(HDRP(pptr), PACK(size, 0));
        PUT(FTRP(nptr), PACK(size, 0));
        void *cgroup = get_group(pptr);

        /* set pointers for pred block and succ block of previous block */
        if (GOTO_SUCC(pptr) != 0) {
            MOV(SUCC(GOTO_PRED(pptr)), SUCC(pptr));     /* p_pred -> p_succ */
            MOV(PRED(GOTO_SUCC(pptr)), PRED(pptr));     /* p_pred <- p_succ */
        }
        else
            PUT(SUCC(GOTO_PRED(pptr)), 0);
        /* set pointers for pred block and succ block of next block */
        if (GOTO_SUCC(nptr) != 0) {
            MOV(SUCC(GOTO_PRED(nptr)), SUCC(nptr));     /* n_pred -> n_succ */
            MOV(PRED(GOTO_SUCC(nptr)), PRED(nptr));     /* n_pred <- n_succ */
        }
        else 
            PUT(SUCC(GOTO_PRED(nptr)), 0);

        /* set pointers for newly freed block */
        PUT(PRED(pptr), (size_t)cgroup);
        if (GOTO_SUCC(pptr) != 0)
            MOV(SUCC(pptr), SUCC(cgroup));
        else
            PUT(SUCC(pptr), 0);
        if (GOTO_SUCC(cgroup) != 0) 
            /* set pred pointer to new block for old first free block */
            PUT(PRED(GOTO_SUCC(cgroup)), (size_t)pptr);
        /* set succ to newly freed block for start block */
        PUT(SUCC(cgroup), (size_t)pptr);

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
    void *group = get_group_by_size(asize);
    void *nptr;
    void *ngroup;
    /* loop over all group classes */
    for (ngroup = group; ngroup <= (heap_listp+(8*WSIZE)); ngroup += WSIZE) {
        /* loop over all blocks inside a group */
        for (nptr = GOTO_SUCC(ngroup); nptr; nptr = GOTO_SUCC(nptr)) {
            if (GET_SIZE(HDRP(nptr)) >= asize)
                return nptr;
        }
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
        /* set pointers for pred block and succ block of ptr */
        MOV(SUCC(GOTO_PRED(ptr)), SUCC(ptr));             /* pred -> succ */
        if (GOTO_SUCC(ptr) != 0) {                        /* do this only if there are more than 1 free blocks */
            MOV(PRED(GOTO_SUCC(ptr)), PRED(ptr));         /* pred <- succ */
        }
    }

    else {                      /* segmente to 2 blocks */

        /* set header and footer */
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(diff, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(diff, 0));

        /* set pointers for pred block and succ block of ptr */
        if (GOTO_SUCC(ptr) != 0) {                        /* do this only if there are more than 1 free blocks */
            MOV(SUCC(GOTO_PRED(ptr)), SUCC(ptr));         /* pred -> succ */
            MOV(PRED(GOTO_SUCC(ptr)), PRED(ptr));         /* pred <- succ */
        }
        else 
            PUT(SUCC(GOTO_PRED(ptr)), 0);

        void *nptr = NEXT_BLKP(ptr);
        void *cgroup = get_group_by_size(diff);                   /* group of newly segmented free block */

        /* set pointers for newly segmented free block */
        PUT(PRED(nptr), (size_t)cgroup);                  /* start <- nptr */
        if (GOTO_SUCC(cgroup) != 0) {
            MOV(SUCC(nptr), SUCC(cgroup));                /* nptr -> old first */
            /* set pred pointer to new block for old first free block */
            PUT(PRED(GOTO_SUCC(cgroup)), (size_t)nptr);   /* nptr < old first */
        }
        else 
            PUT(SUCC(nptr), 0);
        /* set succ to newly freed block for start block */
        PUT(SUCC(cgroup), (size_t)nptr);
    } 
}

/* check if address of ptr inside heap bound */
int inside(void *ptr, void *begin, void *end)
{
    int left, right;

    if (ptr == 0)       /* point to NULL, either block 0 or last free block */
        return 1;
    left = ptr >= begin;
    right = ptr <= end;
    return left & right;
}

/*
 * mm_checkheap - check the heap of correctness.
 */
void mm_checkheap(int lineno)
{

    printf("\n");
    step++;             /* step inside trace file */
    printf("step %d, at trace file row %d\n", step, step+4);

    int heap_size = mem_heapsize();
    void *begin = mem_heap_lo();
    void *end = mem_heap_hi();

    int block = 1;                                /* current/total block number, initial=prologue */
    int fblock = 0;                               /* accumlated free blocks */
    int ablock = 1;                               /* accumlated allocated blocks */
    int size_accum = (4*WSIZE)+ARRAYSIZE;         /* accumlated block size, initial=prologue+epilogue */
    void *ptr = heap_listp;                       /* implicit list start, also first group array address */
    void *group;                                  /* group list of current block */
    void *ngroup;      
    int size;                                     /* size of current block */
    int alloc;                                    /* alloc of current block */

    /* print basic info */
    printf("checkheap called from line : %d\n", lineno);
    printf("current heap size is %d bytes\n", mem_heapsize());
    /* print group lists start */
    for (ngroup = heap_listp; ngroup <= (heap_listp+(8*WSIZE)); ngroup += WSIZE) 
        printf("group > 2^%d+1 at %p, first block at %p\n", GET_GROUP_LO(ngroup), 
            ngroup, GOTO_SUCC(ngroup));
    /* block 1 */
    printf("\nblock 1 (heap_listp) is at address %p, next at %p\n", 
        heap_listp, NEXT_BLKP(heap_listp));


    /* check prologue and epilogue of heap */ 
    if (GET(begin) != 0){
        printf("heap start corrupted\n");
        exit(0);
    }
    else if (GET_SIZE(HDRP(heap_listp)) != 48 || GET_ALLOC(HDRP(heap_listp)) != 1) {
        printf("prologue corrupted\n");
        exit(0);
    }
    else if (GET(HDRP(heap_listp)) != GET(FTRP(heap_listp))) {
        printf("prologue corrupted\n");
        exit(0);
    }
    if (GET(((char *)end) - 3) != 1) {
        printf("epilogue corrupted\n");
        exit(0);
    }

    /* loop over whole heap, block by block */
    while ((size = GET_SIZE(HDRP(NEXT_BLKP(ptr)))) != 0) {  /* not include epilogue */
        ptr = NEXT_BLKP(ptr);
        group = get_group(ptr);
        size_accum += size;
        block++;
        alloc = GET_ALLOC(HDRP(ptr));
        (alloc == 1) ? ablock++ : fblock++;
        void *pptr = PREV_BLKP(ptr);
        void *nptr = NEXT_BLKP(ptr);
        int palloc = GET_ALLOC(HDRP(pptr));
        int nalloc = GET_ALLOC(HDRP(nptr));
        /* print per block info */
        printf("block %d has pred at : %p, self at %p, succ at %p, group > 2^%d+1, "
            "header size: %d, alloc %d\n", block, GOTO_PRED(ptr), ptr,
            GOTO_SUCC(ptr), GET_GROUP_LO(group), size, alloc);

        /* check header and footer */
        if (GET(HDRP(ptr)) != GET(FTRP(ptr))) {
            printf("header not equal to footer at block %d, "
                "header: size %d, alloc %d, footer: size %d, alloc %d\n", 
                block, size, alloc, GET_SIZE(FTRP(ptr)), GET_ALLOC(FTRP(ptr)));
            exit(0);
        }
        /* check size bigger than 24 bytes */
        if (size < 24) {
            printf("block %d with size %d smaller than 24 (bytes), previous block "
                "size %d, next block size %d\n", block, size, 
                GET_SIZE(pptr), GET_SIZE(nptr));
        }
        /* check address inside bound */
        if (!inside(ptr, begin, end)) {
            printf("pointer of block %d out of bound, which is %u, "
                "begin address: %ud, end address: %u\n", 
                block, (size_t)ptr, (size_t)begin, (size_t)end);
            exit(0);
        }
        /* check coalesce */
        if (!alloc && (!palloc || !nalloc)) {
            printf("coalesc error of block %d -> | %d | %d | %d |\n", 
                block, palloc, alloc, nalloc);
            exit(0);
        }
        /* check whether free block is inside free list , 
         * pass this check means all free blocks are inside right free list
         */
        if (!alloc) {
            int inside_list = 0;
            void *fptr;
            for (fptr = get_group(ptr); fptr; fptr = GOTO_SUCC(fptr)) {
                if (ptr == fptr) {
                    inside_list = 1;
                    break;
                }
            }
            if (!inside_list) {       
               printf("free block %d at %p not in group > 2^%d+1, size %d, "
                "alloc %d, next block at %p, size %d, alloc %d\n", 
                block, ptr, GET_GROUP_LO(group), size, alloc, NEXT_BLKP(ptr), 
                GET_SIZE(NEXT_BLKP(ptr)), GET_ALLOC(NEXT_BLKP(ptr)));
               exit(0); 
           }
        }
    }

    /* loop over all group classes */
    for (group = heap_listp; group <= (heap_listp+(8*WSIZE)); group += WSIZE) {
        /* loop over all blocks inside a group */
        int block_cnt = 0;
        for (ptr = GOTO_SUCC(group); ptr; ptr = GOTO_SUCC(ptr)) {

            block_cnt++;
            size = GET_SIZE(HDRP(ptr));
            alloc = GET_ALLOC(HDRP(ptr));
            void *pred = GOTO_PRED(ptr);
            void *succ = GOTO_SUCC(ptr);

            /* check free or not */
            if (alloc) {
                printf("block_cnt %d at %p in group > 2^%d+1 is not free, "
                    "size %d, alloc %d, next block at %p, size %d, alloc %d\n", 
                    block_cnt, ptr, GET_GROUP_LO(group), size, alloc, NEXT_BLKP(ptr), 
                    GET_SIZE(HDRP(NEXT_BLKP(ptr))), 
                    GET_ALLOC(HDRP(NEXT_BLKP(ptr))));
                /* pred block info */
                printf("has pred at %p in group > 2^%d+1, size %d, alloc %d, " 
                    "succ %p\n", pred, GET_GROUP_LO(get_group(pred)), GET_SIZE(HDRP(pred)), 
                    GET_ALLOC(HDRP(pred)), succ);
                exit(0);
            }
            /* check if pred and succ point to valid address */
            if (!inside(pred, begin, end)) {
                printf("pred of free block_cnt %d at address %p (size %d, alloc %d) "
                    "out of bound, point to address %p\n", block_cnt, ptr, size, alloc, pred);
                exit(0);
            }
            if (!inside(succ, begin, end)) {
                printf("succ of free block_cnt %d at address %p (size %d, alloc %d) "
                    "out of bound, point to address %p\n", block_cnt, ptr, size, alloc, succ);
                exit(0);
            }
            /* check consistence between pred, ptr, succ */
            if (succ == 0 && GOTO_SUCC(pred) != ptr) {
                printf("pointers not consistant of last free block at %p in group > 2^%d+1, "
                    "succ of previous free block is %p\n", ptr, 
                    GET_GROUP_LO(group), GOTO_SUCC(pred));
                exit(0);
            }
            else if (succ != 0) {
                if (GOTO_PRED(succ) != ptr || GOTO_SUCC(pred) != ptr) {
                    printf("block pointers not consistant at %p in group > 2^%d+1\n", 
                        ptr, GET_GROUP_LO(group));
                    exit(0);
                }
            }
        }
    }

    if (size_accum != heap_size) {
        printf("heap size mismatch, should be %d bytes, but get %d bytes\n", 
            heap_size, size_accum);
        exit(0);
    }
    if (block != fblock + ablock) {
        printf("block number mismatch, totally %d blocks, "
            "but get %d free + %d alloc\n", block, fblock, ablock);
        exit(0);
    }

    printf("totally %d blocks\n", block);
}
