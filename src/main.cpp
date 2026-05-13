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
#include <unistd.h>
#include "gui.h"

extern void qt_set_sequence_auto_mnemonic(bool b);

int main(int argc, char *argv[])
{

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
