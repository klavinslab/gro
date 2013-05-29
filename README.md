gro
===

The gro bacterial micro-colony growth specification and simulation software.

Compilation Notes
=================

Eric successfully compiles gro on a Mac and on Windows 7. The following description is for the Mac.

To compile you need:

  chipmunk: version 5.3.5 should work: http://chipmunk-physics.net/release/
  Qt Creator: http://qt.digia.com/product/developer-tools/ or just qmake
 
Then, from within Qt, open three separate projects: chipmunk, ccl, and gro.

Then:

  * Compile chipmunk. You need a static library, so you might need to tweak the chipmunk qmake file to give you this.
  * Compile ccl
  * Edit gro.pro and make sure the directories for the ccl and chipmunk libraries are configured for your machine.
  * Compile gro

Linux Notes
===========

Nick got this to work on ubuntu linux and sent me the following notes:

I put together a minimal patch that makes gro compile fine on linux (for me, ubuntu 12.04). Here's a short summary of the changes:

ccl:
    Program.h: if OS is linux, includes strdup (just like windows)
gro:
    gro.pro: if linux, uses same settings as for mac, except:
                         -lchipmunk instead of -lchip
                         no -fast  CXXFLAG
    GroPainter.cpp: tries to include gropainter.h, but only GroPainter.h exists. I think this was a bug that was missed since mac's filesystem is case insensitive.

