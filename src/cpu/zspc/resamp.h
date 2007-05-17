/* Simple sample rate increase using linear interpolation */

#ifndef RESAMPLER_H
#define RESAMPLER_H

#include <assert.h>

#ifdef __cplusplus
	extern "C" {
#endif

/* All input and output is in stereo. Individual samples are counted, not pairs,
so all counts should be a multiple of 2. */

/* Changes input/output ratio. Does *not* clear buffer, allowing changes to ratio
without disturbing sound (in case you want to make slight adjustments in real-time). */
void resampler_set_rate( int in_rate, int out_rate );

/* Clears input buffer */
void resampler_clear( void );

/* Number of samples that can be written to input buffer */
static int resampler_max_write( void );

/* Pointer to where new input samples should be written */
static short* resampler_buffer( void );

/* Tells resampler that 'count' samples have been written to input buffer */
static void resampler_write( int count );

/* Resamples and generates at most 'count' output samples and returns number of
samples actually written to '*out' */
int resampler_read( short* out, int count );


#ifndef RESAMPLER_BUF_SIZE
	#define RESAMPLER_BUF_SIZE 4096
#endif

/* Private */
extern short resampler_buf [RESAMPLER_BUF_SIZE + 8];
extern short* resampler_write_pos;

static inline int resampler_max_write( void ) { return resampler_buf + RESAMPLER_BUF_SIZE - resampler_write_pos; }

static inline short* resampler_buffer( void ) { return resampler_write_pos; }

static inline void resampler_write( int count )
{
	assert( count % 2 == 0 ); /* must be even */
	
	resampler_write_pos += count;
	
	/* fails if you overfill buffer */
	assert( resampler_write_pos <= &resampler_buf [RESAMPLER_BUF_SIZE] );
}

#ifdef __cplusplus
	}
#endif

#endif
