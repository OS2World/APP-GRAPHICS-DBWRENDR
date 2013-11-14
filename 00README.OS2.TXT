DBWRENDR for OS/2

This file describes the OS/2 Bound version of DBW-RENDER.  The RAY.EXE
program will run under both MS-DOS and OS/2.

The only differences between this and the original DBWRENDER 1.02 for
the IBM-PC distribution I received is that I modified the RAY.H file. 
I added a "#DEFINE OS2 1" at the beginning of the file, and I added an
#IFDEF OS2 which changes the version message to say version "1.02 (OS2
Bound)".  I also added my name to the version message as the person who
did the OS/2 "conversion".  

I actually didn't change any of the C code, so saying I'm responsible
for "converting" this is really not true.  All I had to do was
re-compile it under OS/2, and run the BIND program.  Thus this is a
straight port from the MS-DOS version.  There are obviously many places
where multithreading and other OS/2 features may be applicable to
DBWRENDR.  I'll leave that to others.

In addition to modifying the RAY.H file, I also created the following
files:

   MAKERAY.OS2  - Make file for Microsoft C 5.1 compiler under OS/2
  00readme.OS2  - this file. 

In the MAKERAY.OS2 program, I assume the BIND program can find the DOSCALLS 
library in C:\OS2\DOSCALLS.LIB.  If this is not correct on your computer you
may have to change this line or run BIND manually.

I only converted the RAY program.  This creates the .TMP files, but does not
actually display anything.  You still have to use the MS-DOS Display program,
or a program that converts DBW .TMP output to .GIF or some other format.

What we really need is a program that converts .TMP files to Presentation
Manager .BMP files, or a Presentation Manager application to display .TMP
files directly.  Again, I'll leave this for others to do.


- Kevin Lowey 

  Internet: LOWEY@SASK.USask.CA
  Fidonet:  Kevin Lowey @ 1:140/43.0
  Uucp:     Lowey@Herald.uucp
  Bitnet:   Lowey@Sask
  Sask-Tel: (306) 966-4857
