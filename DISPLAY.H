#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

#define BPP         4                   /* Bits per pixel */
#define MAXROW      200                 /* Rows run 0-199 */
#define MAXCOL      320                 /* Cols run 0-319 */
#define COLORS      256
#define BITS        8                   /* 2^BITS == COLORS */
#define TMPCOLORS  4096                 /* colors in TMP file, 0..4095 */

#define MAXBYTE     40                  /* MAXCOL / 8     */
#define DEPTH       6
#define MAXGRAY     (1 << BPP)
#define PPW         (sizeof( int ) * 8 / BPP)
#define WPSL        (MAXCOL / PPW)
#define ABS(x)      ((x) < 0 ? -(x) : (x))


typedef int              scanlinetype[3][WPSL];
typedef unsigned char    byte;

#define  RED   0         /* color indices into 'line'  */
#define  GREEN 1
#define  BLUE  2

int gifPixel( short );
int gifFlush( void  );


