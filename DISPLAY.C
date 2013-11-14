/************************************************************/
/*                                                          */
/*   changes -                                              */
/*   10/31/89  ver 1.02 - allow for new maxcol and maxrow   */
/*             integers at front of temp file.  They are    */
/*             scanned off & ignored, for now               */
/*                                                          */ 
/************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <graph.h>

#include "display.h"

#define VERSION "\nDISPLAY v1.02 (IBM MCGA/VGA)\nCopyright (C) 1989 J. Lowery.  All rights reserved.\n\n"

struct {
     int       curr;
     int       fp;
       } ifil[10];

static int     numfiles = 0;
static int     colorstats[TMPCOLORS][2];
static int     image[MAXCOL];
static char    fname[40],outname[40],str[40];
static int     usedColors;
static int     giffile;

/* 'ofil' must be accessable to gif output functions in another module */
       int     ofil;

/* buffer for a single scan line */
static scanlinetype line;



/* GIF header mode byte definitions */

#define GLOBALCOLORMAP 0x80
#define COLORRES       ((DEPTH-1) << 4)
#define BITSPERPIXEL   (BITS-1)

/* 
 *   the following structures are odd lengths and mix byte- and 
 *   word-length objects, so MUST be packed on 1-byte boundaries, 
 *   or file operations break.
 */

#pragma pack(1)

struct {
     char signature[6];
     int  screenWidth;
     int  screenHeight;

     byte mode;
     byte background;
     byte empty;

     } gifHeader = { "GIF87a", MAXCOL, MAXROW, 
                     GLOBALCOLORMAP | COLORRES | BITSPERPIXEL, 0, 0 };


struct {
     char separator;
     int  imageLeft;
     int  imageTop;
     int  imageWidth;
     int  imageHeight;
     byte mode;
     byte codeSize;      /* this is actually part of the image data stream */

     } imageHeader = { ',', 0, 0, MAXCOL, MAXROW, BITSPERPIXEL, BITS }; 

/* back to normal structure packing */
#pragma pack()

/*   
 *   display write error, wait for operator acknowledge, 
 *   reset video & return.  This is used while video is in 320x200
 *   mode. 
 */
 
int writeError()
{
     close( ofil );           /* close the file   */

     _settextposition( 16, 0 );
     printf("Write error in file %s\n", outname);
     printf("Press any key to exit...");
     while (!kbhit())
          ;
     getch();
     return( TRUE );
}

int gifHdr()      /* create a GIF file from screen */
{
     byte colorValue[3];      /* a single rgb triplet */
     int  i;

     /* write the header information */
     if (write(ofil, (char *)&gifHeader, sizeof(gifHeader)) != sizeof(gifHeader))
          return( writeError() );

     /* write the global color map   */
     for (i=0; i<COLORS; i++)
     {
          /* 
           *   for each of our 256 possible colors, 
           *   map the color's 3 primary intesities (0..15) into 
           *   the gif color map color intensities (0..255).
           *   Note that i==0 defines the MCGA border color.
           */

          if (i < usedColors)
          {
               colorValue[2] = ((long)(colorstats[i][0] & 0x0F00) >> 4); /* blue  */
               colorValue[1] = ((long)(colorstats[i][0] & 0x00F0) );     /* green */
               colorValue[0] = ((long)(colorstats[i][0] & 0x000F) << 4 ); /* red   */
          }
          else
          {
               colorValue[0] = 0;
               colorValue[1] = 0;
               colorValue[2] = 0;
          }

          if (write( ofil, colorValue, sizeof(colorValue)) != sizeof(colorValue))
               return( writeError() );
     }
     /* write the image descriptor */
     if (write(ofil, (char *)&imageHeader, sizeof(imageHeader)) != sizeof(imageHeader))
          return( writeError() );

     return(0);       /* header complete, return ok */ 
}


char terminal[2] = { 0, ';' };     /* image terminator sequence */

int gifClose()
{

     if (gifFlush())
          return(TRUE);
     if (write( ofil, terminal, sizeof( terminal )) != sizeof( terminal ))
          return( writeError() );

     close( ofil );
     return(FALSE);      /* no error */     
}

/* reset video mode, print error message and exit */

void error( msg )
char *msg;
{
     _setvideomode( _DEFAULTMODE );
     printf( "ERROR: %s\n", msg );
     exit( -1 );
}

/* print error message and exit.  Used in 80-column text mode */

void usageExit( msg )
char *msg;
{
     if (msg) 
          printf("ERROR: %s\n",msg);

     printf("\nUsage: raydsp [-g outfile] infile [infile [...]] \n");
     printf("\nWhere: -g       Creates GIF file from screen display");
     printf("\n       outfile  GIF output file name.");
     printf("\n       infile   Input DBW_Render .TMP file.\n");
     printf("\nDisplays a DBW_Render output file, and optionally ");
     printf("\nconverts it to Compuserve (tm) GIF format.\n\n");
     exit(-1);
}

read_scanline(pass,row)
int pass,row;
{
     int i,j,x,val,good;

     if (pass < 2)
     {    if ((row % 50) == 0) 
               printf("\nRow: %4d ",row);
          printf(".");
          fflush(stdout);
     }

     good = 0;
     for (i = 0; i < numfiles && good == 0; i++) 
     {
          while (ifil[i].curr < row) 
          {
               if (read(ifil[i].fp,(char *)&ifil[i].curr,sizeof(int))!=sizeof(int)) 
               {
                    ifil[i].curr = 9999; 
                    continue;
               }

               if (read(ifil[i].fp,(char *)line,sizeof(scanlinetype))!=sizeof(scanlinetype))
               {
                    ifil[i].curr = 9999;
                    continue;
               }
               if (ifil[i].curr == row) 
               {
                    good = 1;
                    break;
               }
          }
     }

    /* couldn't find this scan line in any file */

     if (good == 0) 
          return(0);

     x = 0;
     for (i = 0; i < WPSL; i++) 
     {
          for (j = 0; j < PPW; j++) 
          {
          /*--- TMP file has red, green, then blue nibbles.  Palette
                needs them as blue (MSB), green, red (LSB) ----------*/

               val = (  ((line[RED  ][i] >> (BPP * j)) & (MAXGRAY-1))               /* red   */
                     + (((line[GREEN][i] >> (BPP * j)) & (MAXGRAY-1)) <<  BPP)      /* green */
                     + (((line[BLUE ][i] >> (BPP * j)) & (MAXGRAY-1)) << (BPP*2))); /* blue  */

               val %= TMPCOLORS;

               if (pass == 1)      /* histogram count on pass 1 */
               {
                    if (colorstats[val][1] < 32767) 
                         colorstats[val][1]++;
               }
               else 
                    image[x++] = val;
          }
     }
     return(1);
}

/* 
 *   find the darkest color in colorstats[], and make it
 *   colorstats[0], for use as a border color
 */

getBackground()
{
     /* scan the color table for minimal distance to r,g,b = {0,0,0} */

     int i, j;
     short best, dist;
     short t0, t1;

     for ( i=0, best=0, dist=32000; i < usedColors; i++) 
     {
          j = distance( colorstats[i][0], 0 );
          if ( j < dist) 
          {
               best = i;
               dist = j;
          }
     }

     /* colorstats[0] gets darkest color for background */

     t0 = colorstats[ best ][ 0 ];
     t1 = colorstats[ best ][ 1 ];
     colorstats[ best ][ 0 ] = colorstats[ 0 ][ 0 ];  
     colorstats[ best ][ 1 ] = colorstats[ 0 ][ 1 ];
     colorstats[ 0 ][ 0 ] = t0;
     colorstats[ 0 ][ 1 ] = t1;


}

main(argc,argv)
int        argc;
char        **argv;
{
     short  i, j, k;
     short  xRes, yRes;
     short  gap;
     short  t0, t1, prgb, crgb, cpix, ppix, maxdis;
     char   *s;
     char   errmsg[80];


     printf(VERSION);

     fname[0] = '\0';

     for (i=1; i<argc; i++) 
     {
          if (*argv[i] == '-')
          {
               argv[i]++;

               if (toupper(*argv[i]) == 'G')
               {    
                    /* 
                     *   the output filename may be part of the current
                     *   argv, or may be the next, depending on exactly
                     *   how the operator typed it.  
                     */

                    giffile = TRUE;
                    argv[i]++;
                    if (*argv[i] == '\0')    /* name isn't here   */
                         i++;                /* try the next argv */

                    if (i<argc)
                    {
                         /* 
                          *   get the output filename, if present,
                          *   leaving room for possible concatination
                          *   of a filetype
                          */
                         strncpy( outname, argv[i], sizeof(outname)-5 );

                         /* if no filetype, default to .GIF */
                         if ( strchr( outname, '.' ) == NULL )
                              strcat( outname, ".GIF" );
                    }
                    else
                         outname[0] = '\0';  /* no output filename */
               }
               else
               {
                    sprintf( errmsg, "unrecognized command line option: -%c",*argv[i]);
                    usageExit( errmsg );
               }
          }
          else
          {          
               if (numfiles > 9) 
                    usageExit("too many input files supplied (9 max.)");

               strcpy( str, argv[i] );  /* filename into our temp buf */

               if ( strchr( str, '.' ) == NULL )  /* if no filetype */
                    strcat( str, ".TMP" );        /* ..provide one  */

               ifil[numfiles].fp = open(str,O_RDONLY | O_BINARY,0);
               if (ifil[numfiles].fp == -1)
               {    
                    printf("ERROR: Can't find input file '%s'.",str);
                    continue;
               }
               else
               {    /* 
                     *   read and check the row, col spec 
                     *   if not 320x200, bail out.
                     */
                    if ( read(ifil[numfiles].fp, (char *)&xRes, sizeof( short )) 
                              != sizeof( short ) ||
                         read(ifil[numfiles].fp, (char *)&yRes, sizeof( short )) 
                              != sizeof( short )  )
                    {
                         printf("ERROR: Can't read file '%s' - ignored.\n",str);
                         continue;
                    }

                    if ( xRes != 320 || yRes != 200 )
                    {
                         printf("ERROR: %s is not a 320x200 MCGA data file - ignored\n",str);
                         continue;
                    }
               }

               ifil[numfiles].curr = -9999;
               numfiles++;
          }
     }

     if (numfiles == 0)
          usageExit( "no valid input file(s) specified." );

     if ( giffile && outname[0] == '\0' )
          usageExit( "no GIF output filename specified." );

     if (giffile)
     {
          /* check to see if output file exists */
          ofil = open(outname, O_RDONLY);
          close( ofil );

          if (ofil != -1)          /* yes, warn them */
          {
               printf("Output file \"%s\" exists.  Replace it (y/N)? ",outname);
               if (toupper(getch()) != 'Y')
                    exit( -1 );       /* no further message, just exit */
          }
     
          /* now, open it for real... */
          ofil = open(outname, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 
                                  S_IREAD | S_IWRITE );
          if (ofil == -1) 
               error( "can't create output file." );

          printf("\n");
     }

     printf("Building a color histogram:\n");

     for (i = 0; i < TMPCOLORS; i++) 
     {
          colorstats[i][0] = i;
          colorstats[i][1] = 0;
     }

     /* read through the input file list, building the color table */
     for (i = 0; i < MAXROW; i++ )
          read_scanline(1,i);

     printf("\n");

     /* reset the input file list */
     for (i = 0; i < numfiles; i++) 
     {
          lseek(ifil[i].fp,(long)(2*sizeof(int)),0);
          ifil[i].curr = -9999;
     }

     /*
      *   we now sort the color table in descending order 
      *   of quantity of pixels of each color
      */

     for (gap = TMPCOLORS/2; gap > 0; gap /=2) 
     {
          for (i = gap; i < TMPCOLORS; i++) 
          {
               for (j = i-gap; 
                     j >= 0 && colorstats[j][1] < colorstats[j+gap][1]; 
                      j -= gap) 
               {
                    t0                  = colorstats[j][0];
                    t1                  = colorstats[j][1];
                    colorstats[j][0]    = colorstats[j+gap][0];
                    colorstats[j][1]    = colorstats[j+gap][1];
                    colorstats[j+gap][0]= t0;
                    colorstats[j+gap][1]= t1;
               }
          }
     }

     /* count colors with a population > 0 */
     for (usedColors = 0 ; 
               usedColors < TMPCOLORS && colorstats[usedColors][1] > 0; 
                    usedColors++) 
          ;

/* debug ----------------
     printf("post-sort - Colors:population \n\t");
     for (i = 0; i < usedColors; i++) 
     {
          printf("%03x:%05d ",colorstats[i][0],colorstats[i][1]);
          if ((i % 7) == 7) 
               printf("\n\t");
     }
     printf("Press any key to continue...");
     while (!kbhit())
          ;
     getch();
-------------- debug */

     if (usedColors == 0)
     {
          error( "\nInput file error, no color data." );
     }
     else if (usedColors < COLORS)
     {
          printf("\n%d colors used\n",usedColors);
     }
     else
     {
          printf("\nMapping %d colors into %d\n", usedColors, COLORS);

          for (maxdis = 2; maxdis < (TMPCOLORS/4); maxdis *= 2) 
          {
               for (i=usedColors/2; i < usedColors; i++) 
               {
                    for (j = 0; j < i; j++) 
                    {
                         if (distance(colorstats[i][0],colorstats[j][0]) < maxdis) 
                         {
                              /* delete colorstat[i][] */
                              for (k=i+1; k<usedColors; k++) 
                              {
                                   colorstats[k-1][0] = colorstats[k][0];
                                   colorstats[k-1][1] = colorstats[k][1];
                              }
                              --usedColors;
                              --i;
                              break;
                         }
                    }
                    if (usedColors <= COLORS) break;
               }
               if (usedColors <= COLORS) break;
          }
     }

     /* set up to display the image */

     _setvideomode( _MRES256COLOR );    /* 320 x 200 x 256 color */

     /* now set up the palette      */

     getBackground();    /* force darkest color to colorstats[0] */

     for (i = 0; i < usedColors; i++) 
     {    _remappalette( i, ((long)(colorstats[i][0] & 0x0F00) << 10) |
                            ((long)(colorstats[i][0] & 0x00F0) << 6 ) |
                            ((long)(colorstats[i][0] & 0x000F) << 2 ) );
     }

     /* if a GIF file was requested, write the header here */

     if ( giffile && gifHdr())        /* GIF file is no good  */
     {
          exit( -1 );
     }

    /* now figure out the pixels to display */

    cpix    = 0;
    crgb    = colorstats[0][1];

    for (i = 0; i < MAXROW; i++) 
    {
     /* check if there was an attempt to abort */

          if (kbhit() && getch() == 0x1B)    /* ESC? */
               error( "ABORT at operator request."); 
               
          if (read_scanline(2,i)) 
          {
               for (j = 0 ; j < MAXCOL; j++) 
               {
                    prgb = crgb;
                    crgb = image[j];
                    ppix = cpix;

                    if (j == 0)
                         cpix = getcolor(ppix,&crgb,-1);
                    else
                         cpix = getcolor(ppix,&crgb,prgb);

                /* display the computed pixel */

                    _setcolor( cpix );
                    _setpixel( j, i );

                    if (giffile)             /* gif file? */
                         if (gifPixel( cpix ))
                         {
                              exit( -1 );
                         }     
               }
          }
          else if (giffile)
          {    
               /* no line 'i' - write a dummy to the GIF output file */

               cpix = 0;      /* background */

               for (j = 0 ; j < MAXCOL; j++) 
                    if (gifPixel( cpix ))
                    {
                         exit( -1 );
                    }     
          }
     }
     printf("\n");

     if (giffile)             /* gif file is complete */
          gifClose();         /* wrap it up           */

     /* wait here until we get a Return key hit */
     
     putch( 0x07 );      /* go "beep"   */
     while (1)
          if (kbhit() && getch() == 0x0D)
               break;

     /* complete, return display to normal text mode */

     _setvideomode( _DEFAULTMODE );
}

/* get the next encoding for a pixel */

int getcolor(ppix,crgb,prgb)
int ppix, *crgb, prgb;
{
     short i,j,val,cr,cg,cb,pr,pg,pb,nr,ng,nb,best,dist,nrgb;

     /* if same color, then return same as previous pixel */

     if (*crgb == prgb) 
          return((int)ppix);

     /* set up for comparisons */

     cr = *crgb & (MAXGRAY-1);
     cg = (*crgb >> BPP) & (MAXGRAY-1);
     cb = (*crgb >> (BPP*2)) & (MAXGRAY-1);

     /* look for an exact match in the color table (or minimal distance) */

     for (i=0; i < usedColors; i++) 
     {
          if (colorstats[i][0] == *crgb) 
               return (i);
          if (i == 0) 
          {
               best = 0;
               dist = distance(colorstats[i][0],*crgb);
          }
          else if ((j=distance(colorstats[i][0],*crgb)) < dist) 
          {
               best = i;
               dist = j;
          }
     }

     /* do a best absolute */

     *crgb = colorstats[best][0];
     return(best);
}


int distance(argb, brgb) 
int argb, brgb;
{
     short b,g,r;

     /* set up for comparisons */

     r = argb & (MAXGRAY-1);
     g = (argb >> BPP) & (MAXGRAY-1);
     b = (argb >> (BPP*2)) & (MAXGRAY-1);

     r -= brgb & (MAXGRAY-1);
     g -= (brgb >> BPP) & (MAXGRAY-1);
     b -= (brgb >> (BPP*2)) & (MAXGRAY-1);

     return((int)(r*r + g*g + b*b));
}
