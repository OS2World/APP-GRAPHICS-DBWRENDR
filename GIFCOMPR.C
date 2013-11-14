/*
 *
 *  GIFENCOD.C  - GIF Image compression routines
 *
 *  Lempel-Ziv compression based on 'compress'.  
 *  Original GIF modifications by David Rowley 
 *   (mgardi@watdcsu.waterloo.edu)
 *  Converted for use in RAYDSP by John Lowery.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#include "display.h"

extern int ofil;         /* GIF output file handle */

/*
 * General DEFINEs
 */

#define CODEBITS     12
#define HSIZE      5003            /* 80% occupancy */

/*
 *
 * GIF Image compression - modified 'compress'
 *
 * Based on: compress.c - File compression ala IEEE Computer, June 1984.
 *
 * By Authors:  Spencer W. Thomas       (decvax!harpo!utah-cs!utah-gr!thomas)
 *              Jim McKie               (decvax!mcvax!jim)
 *              Steve Davies            (decvax!vax135!petsd!peora!srd)
 *              Ken Turkowski           (decvax!decwrl!turtlevax!ken)
 *              James A. Woods          (decvax!ihnp4!ames!jaw)
 *              Joe Orost               (decvax!vax135!petsd!joe)
 *
 */

static int n_bits;                      /* number of bits/code */
static short maxcode;                   /* maximum code, given n_bits */
static short maxmaxcode = 1<<CODEBITS;  /* should NEVER generate this code */

#define MAXCODE(n_bits)   (((short) 1 << (n_bits)) - 1)

static long htab[HSIZE];
static unsigned short codetab[HSIZE];

#define HashTabOf(i)       htab[i]
#define CodeTabOf(i)    codetab[i]

static short free_ent = 0;                  /* first unused entry */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */

static int  clear_flg = 0;
static int  offset;

/*
 * Algorithm:  use open addressing double hashing (no chaining) on the 
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

static int g_init_bits;
static int ClearCode;
static int EOFCode;
static disk_error;
static first_pass = TRUE;
static short ent;
static int hshift;


static void output( short );
static void cl_block( void );
static void cl_hash( void );
static void char_init( void );
static void char_out( int );
static void flush_char( void );

int gifPixel( c ) 
short c;
{
     register short i;
     register short disp;
     register long  fcode;


     if (first_pass)
     {
          disk_error = FALSE;

          /* Set up g_init_bits - initial number of bits */

          g_init_bits = BITS + 1;
    
          /*
           * Set up the necessary values
           */
          offset = 0;
          clear_flg = 0;
          maxcode = MAXCODE(n_bits = g_init_bits);

          ClearCode = ( 1 << BITS );
          EOFCode = ClearCode + 1;
          free_ent = ClearCode + 2;
    
          char_init();
        
          ent = c;
    
          hshift = 0;
          for ( fcode = HSIZE;  fcode < 65536L; fcode *= 2L )
               hshift++;
          hshift = 8 - hshift;      /* set hash code range bound */
    
          cl_hash();                /* clear hash table */
    
          /*
           *   output the initial clear code
           */
          output( (short)ClearCode );
          first_pass = FALSE;
     }
     else
     {
          
          fcode = (long) (((long) c << CODEBITS) + ent);
          i = (((short)c << hshift) ^ ent);    /* xor hashing */

          if ( HashTabOf (i) == fcode ) 
          {
               ent = CodeTabOf (i);
               return(disk_error);
          } 
          else if ( (long)HashTabOf (i) < 0L )      /* empty slot */
          {
               goto nomatch;
          }

          disp = HSIZE - i;           /* secondary hash (after G. Knott) */

          if ( i == 0 )
               disp = 1;

probe:
          if ( (i -= disp) < 0 )
               i += HSIZE;

          if ( HashTabOf (i) == fcode ) 
          {
               ent = CodeTabOf (i);
               return(disk_error);
          }
          if ( (long)HashTabOf (i) > 0L ) 
               goto probe;

nomatch:
          output ( (short) ent );
          ent = c;
          if ( free_ent < maxmaxcode ) 
          {
               CodeTabOf (i) = free_ent++; /* code -> hashtable */
               HashTabOf (i) = fcode;
          } 
          else
		  {
		       cl_block();
		  }
     }
    return(disk_error);
}

int gifFlush()
{
     output( (short) EOFCode );
     return (disk_error);
}

/*
 * output()
 *
 * Output the given code.
 * Inputs:
 *      code:   A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *              that n_bits =< (long)wordsize - 1.
 * Outputs:
 *      Outputs code to the file.
 * Assumptions:
 *      Chars are 8 bits long.
 * Algorithm:
 *      Maintain a CODEBITS character long buffer (so that 8 codes will
 * fit in it exactly).  When the buffer fills up empty it and start over.
 */

static unsigned long cur_accum = 0;
static int  cur_bits = 0;

static unsigned long masks[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
                                         0x001F, 0x003F, 0x007F, 0x00FF,
                                         0x01FF, 0x03FF, 0x07FF, 0x0FFF,
                                         0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

static void output( code )
short  code;
{
     cur_accum &= masks[ cur_bits ];

     if( cur_bits > 0 )
    	  cur_accum |= ((long)code << cur_bits);
     else
          cur_accum = code;

     cur_bits += n_bits;

     while( cur_bits >= 8 ) 
     {
    	  char_out( (unsigned int)(cur_accum & 0xff) );
          cur_accum >>= 8;
	      cur_bits -= 8;
     }

     /*
      * If the next entry is going to be too big for the code size,
      * then increase it, if possible.
      */
     if ( free_ent > maxcode || clear_flg ) 
     {
          if( clear_flg ) 
          {
               maxcode = MAXCODE (n_bits = g_init_bits);
               clear_flg = 0;
          } 
          else 
          {
               n_bits++;
               if ( n_bits == CODEBITS )
                    maxcode = maxmaxcode;
               else
                    maxcode = MAXCODE(n_bits);
          }
     }
	
     if( code == EOFCode ) 
     {
          /*
           * At EOF, write the rest of the buffer.
           */

          while( cur_bits > 0 ) 
          {
    	       char_out( (unsigned int)(cur_accum & 0xff) );
	           cur_accum >>= 8;
	           cur_bits -= 8;
          }
	      flush_char();

          if (disk_error)
               writeError();
     }
}

/*
 * Clear out the hash table
 */

static void cl_block ()        /* table clear for block compress */
{

     cl_hash();
     free_ent = ClearCode + 2;
     clear_flg = 1;

     output( (short)ClearCode );  /* tell the decoder what we're doing */
}

static void cl_hash()          /* reset code table */
{
     memset( htab, (char) 0xFF, sizeof( htab));
}


/******************************************************************************
 *
 * GIF Specific routines
 *
 ******************************************************************************/

/*
 * Number of characters so far in this 'packet'
 */

static int a_count;

/*
 * Set up the 'byte output' routine
 */
static void char_init()
{
     a_count = 0;
}

/*
 * Define the storage for the packet accumulator
 */

static char accum[ 256 ];

/*
 * Add a character to the end of the current packet, and if it is 254
 * characters, flush the packet to disk.
 */
static void char_out( c )
int c;
{
     accum[ a_count++ ] = c;
	 if( a_count >= 254 ) 
		  flush_char();
}

/*
 * Flush the packet to disk, and reset the accumulator
 */

static void flush_char()
{
     if( a_count > 0 ) 
     {
          /* write the low byte only of a_count */
		  if ( write( ofil, (char *)&a_count, 1 ) != 1 ||
		       write( ofil, (char *)accum, a_count ) != a_count )
		  {
		       disk_error = TRUE;
		  }    
		  a_count = 0;
	 }
}	

/* The End */
