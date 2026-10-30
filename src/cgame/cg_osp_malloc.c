/*
 * cg_osp_malloc.c: Doug Lea's malloc, version 2.6.6 (public domain, "A version of malloc/free/realloc
 * written by Doug Lea and released to the public domain", ftp://g.oswego.edu/pub/misc/malloc.c),
 * compiled for the QVM: no mmap, no stdio (malloc_stats prints with Com_Printf), no mallopt /
 * valloc / pvalloc / cfree, and MORECORE = osp_morecore, a sbrk emulation over a static 1 MB
 * arena.
 * The only caller in OSP is cg_osp_customloc.c (malloc); cg_osp_main.c calls malloc_stats.
 */
#include "cg_local.h"

/*
 * Configuration (dlmalloc's names): size_t is int in the QVM, so INTERNAL_SIZE_T is signed;
 * USE_MEMCPY with the memset / memcpy traps; HAVE_MMAP 0 removed the mmap paths and the
 * IS_MMAPPED size bit; DEBUG 0; MORECORE_CLEARS 0; page size 8 KB.
 */
#define Void_t					void
#define INTERNAL_SIZE_T			size_t

#define MALLOC_ZERO( charp, nbytes )												\
do {																				\
	INTERNAL_SIZE_T mzsz = ( nbytes );												\
	if ( mzsz <= 9 * sizeof( mzsz ) ) {												\
		INTERNAL_SIZE_T *mz = (INTERNAL_SIZE_T *)( charp );							\
		if ( mzsz >= 5 * sizeof( mzsz ) ) {	*mz++ = 0;								\
												*mz++ = 0;								\
			if ( mzsz >= 7 * sizeof( mzsz ) ) {	*mz++ = 0;							\
												*mz++ = 0;							\
				if ( mzsz >= 9 * sizeof( mzsz ) ) {	*mz++ = 0;						\
													*mz++ = 0; } } }					\
		*mz++ = 0;																	\
		*mz++ = 0;																	\
		*mz   = 0;																	\
	} else memset( ( charp ), 0, mzsz );											\
} while ( 0 )

#define MALLOC_COPY( dest, src, nbytes )											\
do {																				\
	INTERNAL_SIZE_T mcsz = ( nbytes );												\
	if ( mcsz <= 9 * sizeof( mcsz ) ) {												\
		INTERNAL_SIZE_T *mcsrc = (INTERNAL_SIZE_T *)( src );						\
		INTERNAL_SIZE_T *mcdst = (INTERNAL_SIZE_T *)( dest );						\
		if ( mcsz >= 5 * sizeof( mcsz ) ) {	*mcdst++ = *mcsrc++;					\
												*mcdst++ = *mcsrc++;					\
			if ( mcsz >= 7 * sizeof( mcsz ) ) {	*mcdst++ = *mcsrc++;				\
												*mcdst++ = *mcsrc++;				\
				if ( mcsz >= 9 * sizeof( mcsz ) ) {	*mcdst++ = *mcsrc++;			\
													*mcdst++ = *mcsrc++; } } }		\
		*mcdst++ = *mcsrc++;														\
		*mcdst++ = *mcsrc++;														\
		*mcdst   = *mcsrc  ;														\
	} else memcpy( dest, src, mcsz );												\
} while ( 0 )

#define malloc_getpagesize		8192
#define pagesz					malloc_getpagesize

#define DEFAULT_TRIM_THRESHOLD	( 128 * 1024 )
#define DEFAULT_TOP_PAD			( 0 )

#define MORECORE				osp_morecore
#define MORECORE_FAILURE		-1

// SVID2/XPG mallinfo structure
struct mallinfo {
	int		arena;		// total space allocated from system
	int		ordblks;	// number of non-inuse chunks
	int		smblks;		// unused -- always zero
	int		hblks;		// number of mmapped regions
	int		hblkhd;		// total space in mmapped regions
	int		usmblks;	// unused -- always zero
	int		fsmblks;	// unused -- always zero
	int		uordblks;	// total allocated space
	int		fordblks;	// total non-inuse space
	int		keepcost;	// top-most, releasable (via malloc_trim) space
};

Void_t *malloc( size_t bytes );
void free( Void_t *mem );
Void_t *realloc( Void_t *oldmem, size_t bytes );
Void_t *memalign( size_t alignment, size_t bytes );
Void_t *calloc( size_t n, size_t elem_size );
int malloc_trim( size_t pad );
size_t malloc_usable_size( Void_t *mem );
void malloc_stats( void );
struct mallinfo mallinfo( void );

/*
 * osp_morecore: the arena. 0x100000 + 0x2000 bytes of bss (the file's whole bss); the first
 * call aligns the start to 8 KB.
 */
#define OSP_ARENA_SIZE			0x100000

static char		osp_arena[OSP_ARENA_SIZE + 0x2000];

static struct {
	unsigned long	brk;		// current break
	unsigned long	base;		// 8 KB aligned start of the arena (0 before the first call)
	unsigned long	size;		// usable size
} osp_morecore_state = { 0, 0, 0 };


// chunk representations

struct malloc_chunk {
	INTERNAL_SIZE_T			prev_size;	// Size of previous chunk (if free).
	INTERNAL_SIZE_T			size;		// Size in bytes, including overhead.
	struct malloc_chunk		*fd;		// double links -- used only if free.
	struct malloc_chunk		*bk;
};

typedef struct malloc_chunk *mchunkptr;

#define SIZE_SZ					( sizeof( INTERNAL_SIZE_T ) )
#define MALLOC_ALIGNMENT		( SIZE_SZ + SIZE_SZ )
#define MALLOC_ALIGN_MASK		( MALLOC_ALIGNMENT - 1 )
#define MINSIZE					( sizeof( struct malloc_chunk ) )

// conversion from malloc headers to user pointers, and back
#define chunk2mem( p )			( (Void_t *)( (char *)( p ) + 2 * SIZE_SZ ) )
#define mem2chunk( mem )		( (mchunkptr)( (char *)( mem ) - 2 * SIZE_SZ ) )

// pad request bytes into a usable size
#define request2size( req ) \
	( ( (long)( ( req ) + ( SIZE_SZ + MALLOC_ALIGN_MASK ) ) < \
	( long )( MINSIZE + MALLOC_ALIGN_MASK ) ) ? MINSIZE : \
	( ( ( req ) + ( SIZE_SZ + MALLOC_ALIGN_MASK ) ) & ~( MALLOC_ALIGN_MASK ) ) )

// size field is or'ed with PREV_INUSE when previous adjacent chunk in use
#define PREV_INUSE				0x1
#define SIZE_BITS				( PREV_INUSE )

// Ptr to next physical malloc_chunk.
#define next_chunk( p )			( (mchunkptr)( ( (char *)( p ) ) + ( ( p )->size & ~PREV_INUSE ) ) )
// Ptr to previous physical malloc_chunk
#define prev_chunk( p )			( (mchunkptr)( ( (char *)( p ) ) - ( ( p )->prev_size ) ) )
// Treat space at ptr + offset as a chunk
#define chunk_at_offset( p, s )	( (mchunkptr)( ( (char *)( p ) ) + ( s ) ) )

// extract p's inuse bit
#define inuse( p ) \
	( ( ( (mchunkptr)( ( (char *)( p ) ) + ( ( p )->size & ~PREV_INUSE ) ) )->size ) & PREV_INUSE )
// extract inuse bit of previous chunk
#define prev_inuse( p )			( ( p )->size & PREV_INUSE )
// check/set/clear inuse bits in known places
#define inuse_bit_at_offset( p, s ) \
	( ( (mchunkptr)( ( (char *)( p ) ) + ( s ) ) )->size & PREV_INUSE )
#define set_inuse_bit_at_offset( p, s ) \
	( ( (mchunkptr)( ( (char *)( p ) ) + ( s ) ) )->size |= PREV_INUSE )

// Get size, ignoring use bits
#define chunksize( p )			( ( p )->size & ~( SIZE_BITS ) )
// Set size at head, without disturbing its use bit
#define set_head_size( p, s )	( ( p )->size = ( ( ( p )->size & PREV_INUSE ) | ( s ) ) )
// Set size/use ignoring previous bits in header
#define set_head( p, s )		( ( p )->size = ( s ) )
// Set size at footer (only when chunk is not in use)
#define set_foot( p, s )		( ( (mchunkptr)( (char *)( p ) + ( s ) ) )->prev_size = ( s ) )

// bins

#define NAV						128		// number of bins

typedef struct malloc_chunk *mbinptr;

#define bin_at( i )				( (mbinptr)( (char *)&( av_[2 * ( i ) + 2] ) - 2 * SIZE_SZ ) )
#define next_bin( b )			( (mbinptr)( (char *)( b ) + 2 * sizeof( mbinptr ) ) )
#define prev_bin( b )			( (mbinptr)( (char *)( b ) - 2 * sizeof( mbinptr ) ) )

#define top						( bin_at( 0 )->fd )	// The topmost chunk
#define last_remainder			( bin_at( 1 ) )		// remainder from last split
#define initial_top				( (mchunkptr)( bin_at( 0 ) ) )

#define IAV( i )				bin_at( i ), bin_at( i )

static mbinptr av_[NAV * 2 + 2] = {
	0, 0,
	IAV( 0 ),   IAV( 1 ),   IAV( 2 ),   IAV( 3 ),   IAV( 4 ),   IAV( 5 ),   IAV( 6 ),   IAV( 7 ),
	IAV( 8 ),   IAV( 9 ),   IAV( 10 ),  IAV( 11 ),  IAV( 12 ),  IAV( 13 ),  IAV( 14 ),  IAV( 15 ),
	IAV( 16 ),  IAV( 17 ),  IAV( 18 ),  IAV( 19 ),  IAV( 20 ),  IAV( 21 ),  IAV( 22 ),  IAV( 23 ),
	IAV( 24 ),  IAV( 25 ),  IAV( 26 ),  IAV( 27 ),  IAV( 28 ),  IAV( 29 ),  IAV( 30 ),  IAV( 31 ),
	IAV( 32 ),  IAV( 33 ),  IAV( 34 ),  IAV( 35 ),  IAV( 36 ),  IAV( 37 ),  IAV( 38 ),  IAV( 39 ),
	IAV( 40 ),  IAV( 41 ),  IAV( 42 ),  IAV( 43 ),  IAV( 44 ),  IAV( 45 ),  IAV( 46 ),  IAV( 47 ),
	IAV( 48 ),  IAV( 49 ),  IAV( 50 ),  IAV( 51 ),  IAV( 52 ),  IAV( 53 ),  IAV( 54 ),  IAV( 55 ),
	IAV( 56 ),  IAV( 57 ),  IAV( 58 ),  IAV( 59 ),  IAV( 60 ),  IAV( 61 ),  IAV( 62 ),  IAV( 63 ),
	IAV( 64 ),  IAV( 65 ),  IAV( 66 ),  IAV( 67 ),  IAV( 68 ),  IAV( 69 ),  IAV( 70 ),  IAV( 71 ),
	IAV( 72 ),  IAV( 73 ),  IAV( 74 ),  IAV( 75 ),  IAV( 76 ),  IAV( 77 ),  IAV( 78 ),  IAV( 79 ),
	IAV( 80 ),  IAV( 81 ),  IAV( 82 ),  IAV( 83 ),  IAV( 84 ),  IAV( 85 ),  IAV( 86 ),  IAV( 87 ),
	IAV( 88 ),  IAV( 89 ),  IAV( 90 ),  IAV( 91 ),  IAV( 92 ),  IAV( 93 ),  IAV( 94 ),  IAV( 95 ),
	IAV( 96 ),  IAV( 97 ),  IAV( 98 ),  IAV( 99 ),  IAV( 100 ), IAV( 101 ), IAV( 102 ), IAV( 103 ),
	IAV( 104 ), IAV( 105 ), IAV( 106 ), IAV( 107 ), IAV( 108 ), IAV( 109 ), IAV( 110 ), IAV( 111 ),
	IAV( 112 ), IAV( 113 ), IAV( 114 ), IAV( 115 ), IAV( 116 ), IAV( 117 ), IAV( 118 ), IAV( 119 ),
	IAV( 120 ), IAV( 121 ), IAV( 122 ), IAV( 123 ), IAV( 124 ), IAV( 125 ), IAV( 126 ), IAV( 127 )
};

// field-extraction macros
#define first( b )				( ( b )->fd )
#define last( b )				( ( b )->bk )

// Indexing into bins
#define bin_index( sz ) \
	( ( ( ( (unsigned long)( sz ) ) >> 9 ) ==    0 ) ?       ( ( (unsigned long)( sz ) ) >>  3 ) : \
	  ( ( ( (unsigned long)( sz ) ) >> 9 ) <=    4 ) ?  56 + ( ( (unsigned long)( sz ) ) >>  6 ) : \
	  ( ( ( (unsigned long)( sz ) ) >> 9 ) <=   20 ) ?  91 + ( ( (unsigned long)( sz ) ) >>  9 ) : \
	  ( ( ( (unsigned long)( sz ) ) >> 9 ) <=   84 ) ? 110 + ( ( (unsigned long)( sz ) ) >> 12 ) : \
	  ( ( ( (unsigned long)( sz ) ) >> 9 ) <=  340 ) ? 119 + ( ( (unsigned long)( sz ) ) >> 15 ) : \
	  ( ( ( (unsigned long)( sz ) ) >> 9 ) <= 1364 ) ? 124 + ( ( (unsigned long)( sz ) ) >> 18 ) : \
	                                                   126 )

// bins for chunks < 512 are all spaced 8 bytes apart, and hold identically sized chunks
#define MAX_SMALLBIN			63
#define MAX_SMALLBIN_SIZE		512
#define SMALLBIN_WIDTH			8

#define smallbin_index( sz )	( ( (unsigned long)( sz ) ) >> 3 )

// Requests are `small' if both the corresponding and the next bin are small
#define is_small_request( nb )	( nb < MAX_SMALLBIN_SIZE - SMALLBIN_WIDTH )

#define BINBLOCKWIDTH			4		// bins per block

#define binblocks				( bin_at( 0 )->size )	// bitvector of nonempty blocks

// bin<->block macros
#define idx2binblock( ix )		( (unsigned)1 << ( ix / BINBLOCKWIDTH ) )
#define mark_binblock( ii )		( binblocks |= idx2binblock( ii ) )
#define clear_binblock( ii )	( binblocks &= ~( idx2binblock( ii ) ) )

// Other static bookkeeping data (no n_mmaps_max / mmap_threshold without mmap)

// variables holding tunable values
static unsigned long	trim_threshold = DEFAULT_TRIM_THRESHOLD;
static unsigned long	top_pad = DEFAULT_TOP_PAD;

// The first value returned from sbrk
static char				*sbrk_base = (char *)( -1 );

// The maximum memory obtained from system via sbrk
static unsigned long	max_sbrked_mem = 0;

// The maximum via either sbrk or mmap
static unsigned long	max_total_mem = 0;

// internal working copy of mallinfo
static struct mallinfo	current_mallinfo = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// The total memory obtained from system via sbrk
#define sbrked_mem				( current_mallinfo.arena )

// Tracking mmaps
static unsigned int		n_mmaps = 0;
static unsigned int		max_n_mmaps = 0;
static unsigned long	mmapped_mem = 0;
static unsigned long	max_mmapped_mem = 0;

// Linking chunks in bin lists. Call these only with variables, not arbitrary expressions.

// Place chunk p of size s in its bin, in size order, putting it ahead of others of same size.
#define frontlink( P, S, IDX, BK, FD )												\
{																					\
	if ( S < MAX_SMALLBIN_SIZE )													\
	{																				\
		IDX = smallbin_index( S );													\
		mark_binblock( IDX );														\
		BK = bin_at( IDX );															\
		FD = BK->fd;																\
		P->bk = BK;																	\
		P->fd = FD;																	\
		FD->bk = BK->fd = P;														\
	}																				\
	else																			\
	{																				\
		IDX = bin_index( S );														\
		BK = bin_at( IDX );															\
		FD = BK->fd;																\
		if ( FD == BK ) mark_binblock( IDX );										\
		else																		\
		{																			\
			while ( FD != BK && S < chunksize( FD ) ) FD = FD->fd;					\
			BK = FD->bk;															\
		}																			\
		P->bk = BK;																	\
		P->fd = FD;																	\
		FD->bk = BK->fd = P;														\
	}																				\
}

// take a chunk off a list
#define unlink( P, BK, FD )															\
{																					\
	BK = P->bk;																		\
	FD = P->fd;																		\
	FD->bk = BK;																	\
	BK->fd = FD;																	\
}																					\

// Place p as the last remainder
#define link_last_remainder( P )													\
{																					\
	last_remainder->fd = last_remainder->bk = P;									\
	P->fd = P->bk = last_remainder;													\
}

// Clear the last_remainder bin
#define clear_last_remainder \
	( last_remainder->fd = last_remainder->bk = last_remainder )


// osp_morecore: sbrk emulation over osp_arena; returns the old break, (void *)-1 on failure.
static Void_t *osp_morecore( int increment ) {
	unsigned long	brk;

	if ( increment > 0 ) {
		if ( !osp_morecore_state.base ) {
			osp_morecore_state.size = ( OSP_ARENA_SIZE >= ( ( increment + 0x1fff ) & ~0x1fff ) ) ? OSP_ARENA_SIZE : ( ( increment + 0x1fff ) & ~0x1fff );
			if ( osp_morecore_state.size > OSP_ARENA_SIZE ) {
				return (Void_t *)-1;
			}
			osp_morecore_state.brk = osp_morecore_state.base = ( (unsigned long)osp_arena + 0x1fff ) & ~0x1fff;
		} else if ( ( ( osp_morecore_state.brk + increment + 0x1fff ) & ~0x1fff ) > osp_morecore_state.base + osp_morecore_state.size ) {
			return (Void_t *)-1;
		}
		brk = osp_morecore_state.brk;
		osp_morecore_state.brk = brk + increment;
		return (Void_t *)brk;
	}

	if ( increment < 0 ) {
		unsigned long	newbrk;

		newbrk = ( osp_morecore_state.brk + increment + 0x1fff ) & ~0x1fff;
		if ( newbrk >= osp_morecore_state.base ) {
			osp_morecore_state.brk += increment;
			return (Void_t *)osp_morecore_state.brk;
		}
		osp_morecore_state.brk = osp_morecore_state.base;
		return (Void_t *)-1;
	}

	return (Void_t *)osp_morecore_state.brk;
}

// malloc_extend_top: extend the top-most chunk by obtaining memory from the system
static void malloc_extend_top( INTERNAL_SIZE_T nb ) {
	char			*brk;				// return value from sbrk
	INTERNAL_SIZE_T	front_misalign;		// unusable bytes at front of sbrked space
	INTERNAL_SIZE_T	correction;			// bytes for 2nd sbrk call
	char			*new_brk;			// return of 2nd sbrk call
	INTERNAL_SIZE_T	top_size;			// new size of top chunk

	mchunkptr		old_top = top;		// Record state of old top
	INTERNAL_SIZE_T	old_top_size = chunksize( old_top );
	char			*old_end = (char *)( chunk_at_offset( old_top, old_top_size ) );

	// Pad request with top_pad plus minimal overhead
	INTERNAL_SIZE_T	sbrk_size = nb + top_pad + MINSIZE;

	// If not the first time through, round to preserve page boundary
	if ( sbrk_base != (char *)( -1 ) )
		sbrk_size = ( sbrk_size + ( pagesz - 1 ) ) & ~( pagesz - 1 );

	brk = (char *)( MORECORE( sbrk_size ) );

	// Fail if sbrk failed or if a foreign sbrk call killed our space
	if ( brk == (char *)( MORECORE_FAILURE ) ||
		( brk < old_end && old_top != initial_top ) )
		return;

	sbrked_mem += sbrk_size;

	if ( brk == old_end ) {	// can just add bytes to current top
		top_size = sbrk_size + old_top_size;
		set_head( top, top_size | PREV_INUSE );
	} else {
		if ( sbrk_base == (char *)( -1 ) )	// First time through. Record base
			sbrk_base = brk;
		else	// Someone else called sbrk(). Count those bytes as sbrked_mem.
			sbrked_mem += brk - (char *)old_end;

		// Guarantee alignment of first new chunk made from this space
		front_misalign = (unsigned long)chunk2mem( brk ) & MALLOC_ALIGN_MASK;
		if ( front_misalign > 0 ) {
			correction = ( MALLOC_ALIGNMENT ) - front_misalign;
			brk += correction;
		} else
			correction = 0;

		// Guarantee the next brk will be at a page boundary
		correction += ( ( ( (unsigned long)( brk + sbrk_size ) ) + ( pagesz - 1 ) ) &
			~( pagesz - 1 ) ) - ( (unsigned long)( brk + sbrk_size ) );

		// Allocate correction
		new_brk = (char *)( MORECORE( correction ) );
		if ( new_brk == (char *)( MORECORE_FAILURE ) ) return;

		sbrked_mem += correction;

		top = (mchunkptr)brk;
		top_size = new_brk - brk + correction;
		set_head( top, top_size | PREV_INUSE );

		if ( old_top != initial_top ) {
			// There must have been an intervening foreign sbrk call.
			// A double fencepost is necessary to prevent consolidation

			// If not enough space to do this, then user did something very wrong
			if ( old_top_size < MINSIZE ) {
				set_head( top, PREV_INUSE );	// will force null return from malloc
				return;
			}

			// Also keep size a multiple of MALLOC_ALIGNMENT
			old_top_size = ( old_top_size - 3 * SIZE_SZ ) & ~MALLOC_ALIGN_MASK;
			set_head_size( old_top, old_top_size );
			chunk_at_offset( old_top, old_top_size )->size =
				SIZE_SZ | PREV_INUSE;
			chunk_at_offset( old_top, old_top_size + SIZE_SZ )->size =
				SIZE_SZ | PREV_INUSE;
			// If possible, release the rest.
			if ( old_top_size >= MINSIZE )
				free( chunk2mem( old_top ) );
		}
	}

	if ( (unsigned long)sbrked_mem > (unsigned long)max_sbrked_mem )
		max_sbrked_mem = sbrked_mem;
	if ( (unsigned long)sbrked_mem > (unsigned long)max_total_mem )	// without mmapped_mem
		max_total_mem = sbrked_mem;
}

// malloc: best-fit allocation from the bins, the last remainder or the top chunk
Void_t *malloc( size_t bytes ) {
	mchunkptr		victim;				// inspected/selected chunk
	INTERNAL_SIZE_T	victim_size;		// its size
	int				idx;				// index for bin traversal
	mbinptr			bin;				// associated bin
	mchunkptr		remainder;			// remainder from a split
	long			remainder_size;		// its size
	int				remainder_index;	// its bin index
	unsigned long	block;				// block traverser bit
	int				startidx;			// first bin of a traversed block
	mchunkptr		fwd;				// misc temp for linking
	mchunkptr		bck;				// misc temp for linking
	mbinptr			q;					// misc temp

	INTERNAL_SIZE_T	nb;

	if ( (long)bytes < 0 ) return 0;

	nb = request2size( bytes );	// padded request size;

	// Check for exact match in a bin

	if ( is_small_request( nb ) ) {	// Faster version for small requests
		idx = smallbin_index( nb );

		// No traversal or size check necessary for small bins.

		q = bin_at( idx );
		victim = last( q );

		// Also scan the next one, since it would have a remainder < MINSIZE
		if ( victim == q ) {
			q = next_bin( q );
			victim = last( q );
		}
		if ( victim != q ) {
			victim_size = chunksize( victim );
			unlink( victim, bck, fwd );
			set_inuse_bit_at_offset( victim, victim_size );
			return chunk2mem( victim );
		}

		idx += 2;	// Set for bin scan below. We've already scanned 2 bins.
	} else {
		idx = bin_index( nb );
		bin = bin_at( idx );

		for ( victim = last( bin ); victim != bin; victim = victim->bk ) {
			victim_size = chunksize( victim );
			remainder_size = victim_size - nb;

			if ( remainder_size >= (long)MINSIZE ) {	// too big
				--idx;	// adjust to rescan below after checking last remainder
				break;
			} else if ( remainder_size >= 0 ) {	// exact fit
				unlink( victim, bck, fwd );
				set_inuse_bit_at_offset( victim, victim_size );
				return chunk2mem( victim );
			}
		}

		++idx;
	}

	// Try to use the last split-off remainder

	if ( ( victim = last_remainder->fd ) != last_remainder ) {
		victim_size = chunksize( victim );
		remainder_size = victim_size - nb;

		if ( remainder_size >= (long)MINSIZE ) {	// re-split
			remainder = chunk_at_offset( victim, nb );
			set_head( victim, nb | PREV_INUSE );
			link_last_remainder( remainder );
			set_head( remainder, remainder_size | PREV_INUSE );
			set_foot( remainder, remainder_size );
			return chunk2mem( victim );
		}

		clear_last_remainder;

		if ( remainder_size >= 0 ) {	// exhaust
			set_inuse_bit_at_offset( victim, victim_size );
			return chunk2mem( victim );
		}

		// Else place in bin

		frontlink( victim, victim_size, remainder_index, bck, fwd );
	}

	// If there are any possibly nonempty big-enough blocks,
	// search for best fitting chunk by scanning bins in blockwidth units.

	if ( ( block = idx2binblock( idx ) ) <= binblocks ) {
		// Get to the first marked block

		if ( ( block & binblocks ) == 0 ) {
			// force to an even block boundary
			idx = ( idx & ~( BINBLOCKWIDTH - 1 ) ) + BINBLOCKWIDTH;
			block <<= 1;
			while ( ( block & binblocks ) == 0 ) {
				idx += BINBLOCKWIDTH;
				block <<= 1;
			}
		}

		// For each possibly nonempty block ...
		for ( ;; ) {
			startidx = idx;	// (track incomplete blocks)
			q = bin = bin_at( idx );

			// For each bin in this block ...
			do {
				// Find and use first big enough chunk ...

				for ( victim = last( bin ); victim != bin; victim = victim->bk ) {
					victim_size = chunksize( victim );
					remainder_size = victim_size - nb;

					if ( remainder_size >= (long)MINSIZE ) {	// split
						remainder = chunk_at_offset( victim, nb );
						set_head( victim, nb | PREV_INUSE );
						unlink( victim, bck, fwd );
						link_last_remainder( remainder );
						set_head( remainder, remainder_size | PREV_INUSE );
						set_foot( remainder, remainder_size );
						return chunk2mem( victim );
					} else if ( remainder_size >= 0 ) {	// take
						set_inuse_bit_at_offset( victim, victim_size );
						unlink( victim, bck, fwd );
						return chunk2mem( victim );
					}
				}

				bin = next_bin( bin );
			} while ( ( ++idx & ( BINBLOCKWIDTH - 1 ) ) != 0 );

			// Clear out the block bit.

			do {	// Possibly backtrack to try to clear a partial block
				if ( ( startidx & ( BINBLOCKWIDTH - 1 ) ) == 0 ) {
					binblocks &= ~block;
					break;
				}
				--startidx;
				q = prev_bin( q );
			} while ( first( q ) == q );

			// Get to the next possibly nonempty block

			if ( ( block <<= 1 ) <= binblocks && ( block != 0 ) ) {
				while ( ( block & binblocks ) == 0 ) {
					idx += BINBLOCKWIDTH;
					block <<= 1;
				}
			} else
				break;
		}
	}

	// Try to use top chunk

	// Require that there be a remainder, ensuring top always exists
	if ( ( remainder_size = chunksize( top ) - nb ) < (long)MINSIZE ) {
		// Try to extend
		malloc_extend_top( nb );
		if ( ( remainder_size = chunksize( top ) - nb ) < (long)MINSIZE )
			return 0;	// propagate failure
	}

	victim = top;
	set_head( victim, nb | PREV_INUSE );
	top = chunk_at_offset( victim, nb );
	set_head( top, remainder_size | PREV_INUSE );
	return chunk2mem( victim );
}

// free: consolidate the chunk with its free neighbours, into top (trimming) or a bin
void free( Void_t *mem ) {
	mchunkptr		p;			// chunk corresponding to mem
	INTERNAL_SIZE_T	hd;			// its head field
	INTERNAL_SIZE_T	sz;			// its size
	int				idx;		// its bin index
	mchunkptr		next;		// next contiguous chunk
	INTERNAL_SIZE_T	nextsz;		// its size
	INTERNAL_SIZE_T	prevsz;		// size of previous contiguous chunk
	mchunkptr		bck;		// misc temp for linking
	mchunkptr		fwd;		// misc temp for linking
	int				islr;		// track whether merging with last_remainder

	if ( mem == 0 )	// free(0) has no effect
		return;

	p = mem2chunk( mem );
	hd = p->size;

	sz = hd & ~PREV_INUSE;
	next = chunk_at_offset( p, sz );
	nextsz = chunksize( next );

	if ( next == top ) {	// merge with top
		sz += nextsz;

		if ( !( hd & PREV_INUSE ) ) {	// consolidate backward
			prevsz = p->prev_size;
			p = chunk_at_offset( p, -( (long)prevsz ) );
			sz += prevsz;
			unlink( p, bck, fwd );
		}

		set_head( p, sz | PREV_INUSE );
		top = p;
		if ( (unsigned long)( sz ) >= (unsigned long)trim_threshold )
			malloc_trim( top_pad );
		return;
	}

	set_head( next, nextsz );	// clear inuse bit

	islr = 0;

	if ( !( hd & PREV_INUSE ) ) {	// consolidate backward
		prevsz = p->prev_size;
		p = chunk_at_offset( p, -( (long)prevsz ) );
		sz += prevsz;

		if ( p->fd == last_remainder )	// keep as last_remainder
			islr = 1;
		else
			unlink( p, bck, fwd );
	}

	if ( !( inuse_bit_at_offset( next, nextsz ) ) ) {	// consolidate forward
		sz += nextsz;

		if ( !islr && next->fd == last_remainder ) {	// re-insert last_remainder
			islr = 1;
			link_last_remainder( p );
		} else
			unlink( next, bck, fwd );
	}

	set_head( p, sz | PREV_INUSE );
	set_foot( p, sz );
	if ( !islr )
		frontlink( p, sz, idx, bck, fwd );
}

// realloc: grow in place (into top, the next or the previous chunk) or allocate, copy and free
Void_t *realloc( Void_t *oldmem, size_t bytes ) {
	INTERNAL_SIZE_T	nb;				// padded request size

	mchunkptr		oldp;			// chunk corresponding to oldmem
	INTERNAL_SIZE_T	oldsize;		// its size

	mchunkptr		newp;			// chunk to return
	INTERNAL_SIZE_T	newsize;		// its size
	Void_t			*newmem;		// corresponding user mem

	mchunkptr		next;			// next contiguous chunk after oldp
	INTERNAL_SIZE_T	nextsize;		// its size

	mchunkptr		prev;			// previous contiguous chunk before oldp
	INTERNAL_SIZE_T	prevsize;		// its size

	mchunkptr		remainder;		// holds split off extra space from newp
	INTERNAL_SIZE_T	remainder_size;	// its size

	mchunkptr		bck;			// misc temp for linking
	mchunkptr		fwd;			// misc temp for linking

	if ( (long)bytes < 0 ) return 0;

	// realloc of null is supposed to be same as malloc
	if ( oldmem == 0 ) return malloc( bytes );

	newp = oldp = mem2chunk( oldmem );
	newsize = oldsize = chunksize( oldp );

	nb = request2size( bytes );

	if ( (long)( oldsize ) < (long)( nb ) ) {
		// Try expanding forward

		next = chunk_at_offset( oldp, oldsize );
		if ( next == top || !inuse( next ) ) {
			nextsize = chunksize( next );

			// Forward into top only if a remainder
			if ( next == top ) {
				if ( (long)( nextsize + newsize ) >= (long)( nb + MINSIZE ) ) {
					newsize += nextsize;
					top = chunk_at_offset( oldp, nb );
					set_head( top, ( newsize - nb ) | PREV_INUSE );
					set_head_size( oldp, nb );
					return chunk2mem( oldp );
				}
			}

			// Forward into next chunk
			else if ( ( (long)( nextsize + newsize ) >= (long)( nb ) ) ) {
				unlink( next, bck, fwd );
				newsize += nextsize;
				goto split;
			}
		} else {
			next = 0;
			nextsize = 0;
		}

		// Try shifting backwards.

		if ( !prev_inuse( oldp ) ) {
			prev = prev_chunk( oldp );
			prevsize = chunksize( prev );

			// try forward + backward first to save a later consolidation

			if ( next != 0 ) {
				// into top
				if ( next == top ) {
					if ( (long)( nextsize + prevsize + newsize ) >= (long)( nb + MINSIZE ) ) {
						unlink( prev, bck, fwd );
						newp = prev;
						newsize += prevsize + nextsize;
						newmem = chunk2mem( newp );
						MALLOC_COPY( newmem, oldmem, oldsize - SIZE_SZ );
						top = chunk_at_offset( newp, nb );
						set_head( top, ( newsize - nb ) | PREV_INUSE );
						set_head_size( newp, nb );
						return newmem;
					}
				}

				// into next chunk
				else if ( ( (long)( nextsize + prevsize + newsize ) >= (long)( nb ) ) ) {
					unlink( next, bck, fwd );
					unlink( prev, bck, fwd );
					newp = prev;
					newsize += nextsize + prevsize;
					newmem = chunk2mem( newp );
					MALLOC_COPY( newmem, oldmem, oldsize - SIZE_SZ );
					goto split;
				}
			}

			// backward only
			if ( prev != 0 && (long)( prevsize + newsize ) >= (long)nb ) {
				unlink( prev, bck, fwd );
				newp = prev;
				newsize += prevsize;
				newmem = chunk2mem( newp );
				MALLOC_COPY( newmem, oldmem, oldsize - SIZE_SZ );
				goto split;
			}
		}

		// Must allocate

		newmem = malloc( bytes );

		if ( newmem == 0 )	// propagate failure
			return 0;

		// Avoid copy if newp is next chunk after oldp.
		// (This can only happen when new chunk is sbrk'ed.)

		if ( ( newp = mem2chunk( newmem ) ) == next_chunk( oldp ) ) {
			newsize += chunksize( newp );
			newp = oldp;
			goto split;
		}

		// Otherwise copy, free, and exit
		MALLOC_COPY( newmem, oldmem, oldsize - SIZE_SZ );
		free( oldmem );
		return newmem;
	}

split:	// split off extra room in old or expanded chunk

	if ( newsize - nb >= MINSIZE ) {	// split off remainder
		remainder = chunk_at_offset( newp, nb );
		remainder_size = newsize - nb;
		set_head_size( newp, nb );
		set_head( remainder, remainder_size | PREV_INUSE );
		set_inuse_bit_at_offset( remainder, remainder_size );
		free( chunk2mem( remainder ) );	// let free() deal with it
	} else {
		set_head_size( newp, newsize );
		set_inuse_bit_at_offset( newp, newsize );
	}

	return chunk2mem( newp );
}

// memalign: allocate with worst case padding, give back the leader and the spare end
Void_t *memalign( size_t alignment, size_t bytes ) {
	INTERNAL_SIZE_T	nb;				// padded  request size
	char			*m;				// memory returned by malloc call
	mchunkptr		p;				// corresponding chunk
	char			*brk;			// alignment point within p
	mchunkptr		newp;			// chunk to return
	INTERNAL_SIZE_T	newsize;		// its size
	INTERNAL_SIZE_T	leadsize;		// leading space befor alignment point
	mchunkptr		remainder;		// spare room at end to split off
	long			remainder_size;	// its size

	if ( (long)bytes < 0 ) return 0;

	// If need less alignment than we give anyway, just relay to malloc

	if ( alignment <= MALLOC_ALIGNMENT ) return malloc( bytes );

	// Otherwise, ensure that it is at least a minimum chunk size

	if ( alignment < MINSIZE ) alignment = MINSIZE;

	// Call malloc with worst case padding to hit alignment.

	nb = request2size( bytes );
	m = (char *)( malloc( nb + alignment + MINSIZE ) );

	if ( m == 0 ) return 0;	// propagate failure

	p = mem2chunk( m );

	if ( ( ( (unsigned long)( m ) ) % alignment ) != 0 ) {	// misaligned
		// Find an aligned spot inside chunk.
		brk = (char *)mem2chunk( ( (unsigned long)( m + alignment - 1 ) ) & -( (signed)alignment ) );
		if ( (long)( brk - (char *)( p ) ) < MINSIZE ) brk = brk + alignment;

		newp = (mchunkptr)brk;
		leadsize = brk - (char *)( p );
		newsize = chunksize( p ) - leadsize;

		// give back leader, use the rest

		set_head( newp, newsize | PREV_INUSE );
		set_inuse_bit_at_offset( newp, newsize );
		set_head_size( p, leadsize );
		free( chunk2mem( p ) );
		p = newp;
	}

	// Also give back spare room at the end

	remainder_size = chunksize( p ) - nb;

	if ( remainder_size >= (long)MINSIZE ) {
		remainder = chunk_at_offset( p, nb );
		set_head( remainder, remainder_size | PREV_INUSE );
		set_head_size( p, nb );
		free( chunk2mem( remainder ) );
	}

	return chunk2mem( p );
}

// calloc: malloc + clear
Void_t *calloc( size_t n, size_t elem_size ) {
	mchunkptr		p;
	INTERNAL_SIZE_T	csz;

	INTERNAL_SIZE_T	sz = n * elem_size;

	Void_t			*mem;

	if ( (long)n < 0 ) return 0;

	mem = malloc( sz );

	if ( mem == 0 )
		return 0;
	else {
		p = mem2chunk( mem );

		csz = chunksize( p );

		MALLOC_ZERO( mem, csz - SIZE_SZ );
		return mem;
	}
}

// malloc_trim: give memory at the top of the arena back to osp_morecore
int malloc_trim( size_t pad ) {
	long	top_size;		// Amount of top-most memory
	long	extra;			// Amount to release
	char	*current_brk;	// address returned by pre-check sbrk call
	char	*new_brk;		// address returned by negative sbrk call

	top_size = chunksize( top );
	extra = ( ( top_size - pad - MINSIZE + ( pagesz - 1 ) ) / pagesz - 1 ) * pagesz;

	if ( extra < (long)pagesz )	// Not enough memory to release
		return 0;
	else {
		// Test to make sure no one else called sbrk
		current_brk = (char *)( MORECORE( 0 ) );
		if ( current_brk != (char *)( top ) + top_size )
			return 0;	// Apparently we don't own memory; must fail
		else {
			new_brk = (char *)( MORECORE( -extra ) );

			if ( new_brk == (char *)( MORECORE_FAILURE ) ) {	// sbrk failed?
				// Try to figure out what we have
				current_brk = (char *)( MORECORE( 0 ) );
				top_size = current_brk - (char *)top;
				if ( top_size >= (long)MINSIZE ) {	// if not, we are very very dead!
					sbrked_mem = current_brk - sbrk_base;
					set_head( top, top_size | PREV_INUSE );
				}
				return 0;
			} else {
				// Success. Adjust top accordingly.
				set_head( top, ( top_size - extra ) | PREV_INUSE );
				sbrked_mem -= extra;
				return 1;
			}
		}
	}
}

// malloc_usable_size: the number of usable bytes of an allocated chunk
size_t malloc_usable_size( Void_t *mem ) {
	mchunkptr	p;

	if ( mem == 0 )
		return 0;
	else {
		p = mem2chunk( mem );
		if ( !inuse( p ) ) return 0;
		return chunksize( p ) - SIZE_SZ;
	}
}

// malloc_update_mallinfo: recompute current_mallinfo from the bins
static void malloc_update_mallinfo( void ) {
	int				i;
	mbinptr			b;
	mchunkptr		p;

	INTERNAL_SIZE_T	avail = chunksize( top );
	int				navail = ( (long)( avail ) >= (long)MINSIZE ) ? 1 : 0;

	for ( i = 1; i < NAV; ++i ) {
		b = bin_at( i );
		for ( p = last( b ); p != b; p = p->bk ) {
			avail += chunksize( p );
			navail++;
		}
	}

	current_mallinfo.ordblks = navail;
	current_mallinfo.uordblks = sbrked_mem - avail;
	current_mallinfo.fordblks = avail;
	current_mallinfo.hblks = n_mmaps;
	current_mallinfo.hblkhd = mmapped_mem;
	current_mallinfo.keepcost = chunksize( top );
}

// malloc_stats: print the maximum and current arena bytes and the bytes in use
void malloc_stats( void ) {
	malloc_update_mallinfo();
	Com_Printf( "max system bytes = %10d\n",
		(unsigned int)( max_total_mem ) );
	Com_Printf( "system bytes     = %10d\n",
		(unsigned int)( sbrked_mem + mmapped_mem ) );
	Com_Printf( "in use bytes     = %10d\n",
		(unsigned int)( current_mallinfo.uordblks + mmapped_mem ) );
}

// mallinfo: a copy of the updated current_mallinfo
struct mallinfo mallinfo( void ) {
	malloc_update_mallinfo();
	return current_mallinfo;
}
