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
#include "gui.h"

extern void qt_set_sequence_auto_mnemonic(bool b);

int main(int argc, char *argv[])
{

    cpInitChipmunk();
    cp_collision_slop = 0.2f;

    qt_set_sequence_auto_mnemonic(true);

    QApplication a(argc, argv);
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

    cpInitChipmunk();
    cp_collision_slop = 0.2f;

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
