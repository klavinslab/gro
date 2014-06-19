# Compiling gro on Mac OS v10.9.2 with Qt v5.0.3

## Get Qt

Go to http://qt-project.org/downloads to obtain and install the open source version of Qt via the online installer.

Select Gt 5.2 and Tools. 

## Download and install git

Follow the instructions at https://help.github.com/articles/set-up-git. Note that Qt has a nice git interface in Tools/git. You can use this to pull the latest changes and recompile, or simply use git at the command line.

## Get and compile CCL

Make a directory to put code, open a terminal, and cd to that directory. Then clone ccl:

    git clone https://github.com/klavinslab/ccl.git
    
Open Qt and choose "Open Project. Navigate to ccl/ccl.pro and open it. Use only the Desktop kit and the choose "Configure Project". Once you have the project open, build it by selecting Build/Build project "ccl". The results of the build should be a new directory 

    build-ccl-Desktop_Qt_5_3_0_clang_64bit-Debug

in the same directory as ccl, filled with object files and the archive libccl.a. This .a file is the ccl library required by gro.

## Get and compile gro

In your code directory again, clone gro:

    
