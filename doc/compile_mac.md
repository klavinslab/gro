# Compiling gro on Mac OS v10.9.2 with Qt v5.2.1

## Get Qt

Go to http://qt-project.org/downloads to obtain and install the open source version of Qt via the online installer.

Select Qt 5.3 and Tools.

## Download and install git

Follow the instructions at https://help.github.com/articles/set-up-git. Note that Qt has a nice git interface in Tools/git. You can use this to pull the latest changes and recompile, or simply use git at the command line.

## Get all the code

Make a directory to put code. I'll assume it's called Code. Open a terminal, and cd to the Code directory. Then clone ccl and gro:

    git clone https://github.com/klavinslab/ccl.git
    git clone https://github.com/klavinslab/gro.git

## Compile CCL

Open Qt and choose "Open Project. Navigate to ccl/ccl.pro and open it. Use only the Desktop kit and the choose "Configure Project". Once you have the project open, build it by selecting Build/Build project "ccl". The results of the build should be a new directory 

    build-ccl-Desktop_Qt_5_3_0_clang_64bit-Debug

in the Code directory, filled with object files and the archive libccl.a. This .a file is the ccl library required by gro. So that it is easy
to find later, make a link to the directory:

    ln -s build-ccl-Desktop_Qt_5_3_0_clang_64bit-Debug build-ccl

## Get and compile chipmunk.

Go to https://chipmunk-physics.net/release/Chipmunk-5.x/ and get version 5.3.5.
Note that although newer versions of chipmunk are available, they don't seem to work.

Unpack the chipmunk directory in your Code directory and make sure the directory is called chipmunk.
You will then need a chipmunk.pro file. The one I use is in Code/gro/useful. So from within your Code directory, do

    cp gro/useful/chipmunk.pro chipmunk

Then from within Qt, open the Code/chipmunk/chipmunk.pro and compile chipmunk. The result should be a new build directory in Code with a file called libchipmunk.a in it.
Make an easier link to the build file with:

     ln -s build-chipmunk-Desktop_Qt_5_3_0_clang_64bit-Debug/ build-chipmunk 
 
## Get and compile gro

Open Code/gro/gro.pro in Qt, set it to the active project, build gro and run it. You should be able to open a file in the examples file in the build directory and run it.



    
