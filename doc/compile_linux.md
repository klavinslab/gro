Linux Notes
-----------

Nick got this to work on ubuntu linux and sent me the following notes:

I put together a minimal patch that makes gro compile fine on linux (for me, ubuntu 12.04). Here's a short summary of the changes:

ccl:
    Program.h: if OS is linux, includes strdup (just like windows)
gro:
    gro.pro: if linux, uses same settings as for mac, except:
                         -lchipmunk instead of -lchip
                         no -fast  CXXFLAG
    GroPainter.cpp: tries to include gropainter.h, but only GroPainter.h exists. I think this was a bug that was missed since mac's filesystem is case insensitive.

