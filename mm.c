/*
 * Name: Yash Shah (201101202)
 * ( This code works best on 32 BIT machine ) 
 *
 * This is explicit free list allocator in which we maintain the 
 * Free block entries in an explicit list so that we can reduce the
 * linear time worst case allocation time as in implicit allocator. We simply look into the 
 * maintained free list pointers & allocate as soon as we get the 
 * block of suitable size.
 * Unlike in Implicit free list, in which we have to traverse through all the blocks & then check 
 * for the free & suitable sized block, here we directly jump through the free blocks & allocate faster.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
	/* Team name */
	"dynamo",
	/* First member's full name */
	"Yash Shah (201101202)",
	/* First member's email address */
	"yash9414@gmail.com",
	"",
	""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
 
/* aligns given pointer toi size of int ( size_t ) */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/********************************************************
		Some Useful Macros
 ********************************************************/

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define OVERHEAD 8
#define PACK(size, alloc)   ((size) | (alloc))
#define GET(p)              (*(size_t *)(p))
#define PUT(p, val)         (*(size_t *)(p) = (val))
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)
#define HDRP(bp)            ((char *)(bp) - WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) -WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define MAX(a,b) ((a>b)?a:b)

/* Some More macros Used in making Free List & traversing through it */

#define NEXT_FREE(bp) *(int *)(bp)
#define PREV_FREE_BLKP(ptr) (*(void **) (ptr))
#define NEXT_FREE_BLKP(ptr) (*(void **) (ptr + WSIZE))
#define SET_PREV_FREE(bp, previous) (*((void **)(bp)) = previous)
#define SET_NEXT_FREE(bp, next) (*((void **)(bp + WSIZE)) = next)

/* helper function prototype */
void *extend_heap(size_t words);
void addFreeBlock(void *bp) ;
void delFreeBlock(void *bp) ;
void *coalesce(void *bp);
void *find_fit(size_t asize);
void place(void *bp, size_t asize);
/*end of helper functions prototype*/

/* Pointers required for maintaining free list */

void *heap_listp;
void* root;

/* 
 * mm_init - initialize the malloc package.
 * this is same as implicit free list init function.
 * Only change is we also initialize root to NULL.
 */

int mm_init(void)
{

	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) 
		return -1;
	PUT(heap_listp, 0);
	PUT(heap_listp+WSIZE, PACK(OVERHEAD, 1));
	PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1));
	PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1));
	heap_listp += DSIZE;
	root=NULL; // Initialize root to NULL ( yet no free block )
	if (extend_heap(CHUNKSIZE/WSIZE) == (void *)-1)
		return -1;
	return 0;
}

/*
 * 	Extends heap to CHUNKSIZE while initialising the heap.
 *	Also called if can not find the suitable sized free block
 *	and thus it will provide one by calling mem_sbrk function.
*/

void *extend_heap(size_t words) {
	char *bp;
	size_t size;
	size = (words % 2) ? (words+1)*WSIZE :
		words*WSIZE;
	if ((int)(bp = mem_sbrk(size)) == -1)
		return NULL;
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
	return coalesce(bp);
}

/* We Add the free block to the explicit list maintained so that we can directly 
 * traverse through it to get free block & check for required size */

void addFreeBlock(void *bp) {
	void *current = root;
	void *temp = current;
	void *previous = NULL;
	while (current != NULL && bp < current) { 
		previous = PREV_FREE_BLKP(current);
		temp = current;
		current = NEXT_FREE_BLKP(current);
	}

	SET_PREV_FREE(bp, previous);
	SET_NEXT_FREE(bp, temp);
	if (previous != NULL) {
		SET_NEXT_FREE(previous, bp);
	} else { 
		root = bp;
	}
	if (temp != NULL) {
		SET_PREV_FREE(temp, bp);
	}
}

/* Removing the link of free block from the list as soon as it is allocated */

void delFreeBlock(void *bp) {

	void *next = (void *) NEXT_FREE_BLKP(bp);
	void *previous = (void *) PREV_FREE_BLKP(bp);
	if (previous == NULL) { 
		root = next;
	} else {
		SET_NEXT_FREE(previous, next);
	}

	if (next != NULL) { 
		SET_PREV_FREE(next, previous);
	}
}

/*	
	Coalesces the Free blocks to reduce fragmentation 
	Here along with coalescing, the key point is to update explicit
	free list entries for free blocks
 */

void *coalesce(void *bp) {
	size_t previous_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
	/* Previous & Next both are allocated. DO NOT COALESCE */
	if (previous_alloc && next_alloc) {
		addFreeBlock(bp);
		return bp;
	}
	/* Previous is allocated, Next is free. */
	else if (previous_alloc && !next_alloc) {
		delFreeBlock(NEXT_BLKP(bp));
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		addFreeBlock(bp);
	}
	/* Previous is free, Next is Allocated */
	else if (!previous_alloc && next_alloc) {
		delFreeBlock(PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
		addFreeBlock(bp);
	}
	/* Both previousious & next are free */
	else {
		delFreeBlock(PREV_BLKP(bp));
		delFreeBlock(NEXT_BLKP(bp));
		size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
		size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
		addFreeBlock(bp);
	}
	return bp; /* return the void pointer ... anyway we are not getting to catch it */
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size)
{
	size_t asize, extendsize; 	 /* adjusted block size */
	char *bp;                  /* amount to extend heap if no fit */
	if (size <= 0) return NULL;
	if (size <= DSIZE)
		asize = DSIZE+OVERHEAD; 
	else
		asize = ALIGN(size+SIZE_T_SIZE); /* We used already given Macros ALIGN & SIZE_T_SIZE */
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}
	extendsize = MAX(asize,CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}

/* This function finds the suitable free block for allocation requirements */

void *find_fit(size_t asize) { 
	void *bp;

	/* This is the key thing of explicit Allocator. traversing only the free block pointers ...
	 * So directly checking till we find the pointer of the required size or greater size
	 * If found pointer of free block having greater size than required, we are anyway going to 
	 * call place(bp) function to split the memory.
	 */

	for (bp = root; bp != NULL; bp = (void *)NEXT_FREE_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){

			return bp;
		}
	}
	return NULL;    /* no fit */
}

/* Actually allocates the size required by malloc. it divides the free block if its size is larger than
 * required size and makes the remaining space as free block 
 */

void place(void *bp, size_t asize) {
	size_t csize = GET_SIZE(HDRP(bp));

	/* if new free block would be at least as big as min block
	   size, split */

	if ((csize - asize) >= (DSIZE + OVERHEAD)) {
		delFreeBlock(bp);
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		addFreeBlock(bp);
	}

	/* do not split */
	else { 
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		delFreeBlock(bp);
	}
}


/*
 * mm_free - Freeing a block does nothing.
 * Point to note is that we call coalesce as soon as we free the block
 * This is what we call Immediate Coalescing ( this avoids fragmentation ).
 */

void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));
	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *ptr, size_t size)
{
	if(ptr==NULL && size!=0){
		void* justMalloc=mm_malloc(size);
		return justMalloc;
	}
	if(size==0){
		mm_free(ptr);
		return NULL;
	}
	void *oldPointer = ptr;
	void *newPointer;
	size_t copySize;

	newPointer = mm_malloc(size);
	if (newPointer == NULL)
		return NULL;
	copySize = *(size_t *)((char *)oldPointer - WSIZE);

	if (size < copySize)
		copySize = size;
	memcpy(newPointer, oldPointer, copySize);
	mm_free(oldPointer);
	return newPointer;
}
