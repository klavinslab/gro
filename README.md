gro
===

The gro bacterial micro-colony growth specification and simulation software.

Documentation
===

http://depts.washington.edu/soslab/gro/docview.html.

Compiling
===

To get the latest stable build, go to http://depts.washington.edu/soslab/gro 

To compile under linux or mac, see the instructions at https://github.com/klavinslab/gro/tree/master/doc
    
Apple Paths
===

If you are on a Mac you may have discovered an issue with gro not starting properly because of an old directory structure issue. David Soloviechik uses this AppleScript to solve that problem

    tell application "Finder"
    	set current_path to container of (path to me) as alias
    end tell
    set dir to (POSIX path of current_path)
    do shell script "cd \"" & dir & "\" && gro.app/Contents/MacOS/gro > /dev/null 2>&1 &"

To use this script, put it in a file called `run_gro.scpt` within the gro directory and then at the command line run

    > osascript run_gro.scpt
    
    
