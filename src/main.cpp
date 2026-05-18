/////////////////////////////////////////////////////////////////////////////////////////
//
// gro is protected by the UW OPEN SOURCE LICENSE, which is summaraized here.
// Please see the file LICENSE.txt for the complete license.
//
// THE SOFTWARE (AS DEFINED BELOW) AND HARDWARE DESIGNS (AS DEFINED BELOW) IS PROVIDED
// UNDER THE TERMS OF THIS OPEN SOURCE LICENSE (“LICENSE”).  THE SOFTWARE IS PROTECTED
// BY COPYRIGHT AND/OR OTHER APPLICABLE LAW.  ANY USE OF THIS SOFTWARE OTHER THAN AS
// AUTHORIZED UNDER THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
//
// BY EXERCISING ANY RIGHTS TO THE SOFTWARE AND/OR HARDWARE PROVIDED HERE, YOU ACCEPT AND
// AGREE TO BE BOUND BY THE TERMS OF THIS LICENSE.  TO THE EXTENT THIS LICENSE MAY BE
// CONSIDERED A CONTRACT, THE UNIVERSITY OF WASHINGTON (“UW”) GRANTS YOU THE RIGHTS
// CONTAINED HERE IN CONSIDERATION OF YOUR ACCEPTANCE OF SUCH TERMS AND CONDITIONS.
//
// TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION
//
//

#ifndef NOGUI

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "gui.h"

extern void qt_set_sequence_auto_mnemonic(bool b);

// Signal handler that writes a backtrace to ~/Library/Logs/gro/
// crash-<pid>-<unix-time>.log on SIGSEGV/SIGBUS/SIGABRT/SIGILL/
// SIGFPE, then re-raises with the default handler so macOS still
// produces its normal DiagnosticReport. The log is the easy-to-find
// version for users who can't navigate to ~/Library/Logs/
// DiagnosticReports/. Only async-signal-safe calls (open/write/
// backtrace/backtrace_symbols_fd) here.
static void gro_crash_handler(int sig)
{
    char path[256];
    snprintf(path, sizeof(path),
             "%s/Library/Logs/gro/crash-%ld-%ld.log",
             getenv("HOME") ? getenv("HOME") : "/tmp",
             (long)getpid(), (long)time(nullptr));

    // Create the directory if it doesn't exist. mkdir is async-signal-
    // safe on macOS.
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/Library/Logs/gro",
             getenv("HOME") ? getenv("HOME") : "/tmp");
    mkdir(dir, 0755);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char * hdr = "gro crashed. Signal ";
        write(fd, hdr, strlen(hdr));
        const char * name =
            sig == SIGSEGV ? "SIGSEGV (segfault)" :
            sig == SIGBUS  ? "SIGBUS (bus error)" :
            sig == SIGABRT ? "SIGABRT (abort)"    :
            sig == SIGILL  ? "SIGILL (illegal instruction)" :
            sig == SIGFPE  ? "SIGFPE (arithmetic)" :
            "unknown";
        write(fd, name, strlen(name));
        write(fd, "\n\nC++ backtrace (read top-to-bottom; "
                  "atos -o gro.app/Contents/MacOS/gro <addr> for "
                  "symbolicated frames):\n\n", 113);

        void * frames[64];
        int n = backtrace(frames, 64);
        backtrace_symbols_fd(frames, n, fd);

        write(fd, "\n", 1);
        close(fd);

        // Surface the path to stderr so a user running from a
        // terminal sees it immediately.
        const char * msg1 = "\ngro: crash log written to ";
        write(STDERR_FILENO, msg1, strlen(msg1));
        write(STDERR_FILENO, path, strlen(path));
        write(STDERR_FILENO, "\n", 1);
    }

    // Re-raise with default handler so macOS still produces its
    // DiagnosticReport and the process actually dies.
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_crash_handler(void)
{
    struct sigaction sa;
    sa.sa_handler = gro_crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;  // run once per signal
    for (int sig : {SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE}) {
        sigaction(sig, &sa, nullptr);
    }
}

int main(int argc, char *argv[])
{

    install_crash_handler();

    qt_set_sequence_auto_mnemonic(true);

    QApplication a(argc, argv);
    // QSettings uses these to pick its on-disk location. macOS
    // resolves to ~/Library/Preferences/edu.washington.klavinslab.gro.plist;
    // used today by Gui::open to remember the last-browsed directory
    // across launches.
    QCoreApplication::setOrganizationName  ("KlavinsLab");
    QCoreApplication::setOrganizationDomain("klavinslab.washington.edu");
    QCoreApplication::setApplicationName   ("gro");
    // include/, examples/, and python/ are siblings of gro.app at
    // install time. In the dev build they live two more levels up
    // (build/gro.app's grandparent = the project root). Rather than
    // hard-coding either path, walk up from MacOS/ until we find a
    // directory containing all three. Production install dirs and
    // the source tree both satisfy the test, so the same binary
    // works in both contexts without extra symlinks.
    {
        QDir d = QDir(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 6; i++) {
            if (d.exists("include") && d.exists("examples") && d.exists("python")) {
                QDir::setCurrent(d.absolutePath());
                break;
            }
            if (!d.cdUp()) break;
        }
    }
    Q_INIT_RESOURCE(icons);
    Gui w(argc,argv);
    w.show();

    return a.exec();

}

#else

#include <stdio.h>
#include <iostream>
#include "Micro.h"
#include "Programs.h"

int main (int argc, char *argv[])
{

    World * world;
    MicroProgram * current_program;

    world = new World();
    register_gro_functions();

    if ( argc > 1 ) {

        current_program = new gro_Program ( argv[1], argc, argv );
        world->set_program ( current_program );

        try {
            world->init();
        }

        catch ( std::string err ) {
            std::cout << "Error: " << err << "\n";
            return -1;
        }

        while ( 1 ) {

            if ( world->get_stop_flag() ) {
               return 0;
            }

            try {
                world->update();
            }

            catch ( std::string err ) {
                std::cerr << "Error: " << err << "\n";
                return -1;
            }

        }

    }

    return 0;

}

#endif
