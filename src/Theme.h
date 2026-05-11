/////////////////////////////////////////////////////////////////////////////////////////
//
// gro is protected by the UW OPEN SOURCE LICENSE, which is summarized here.
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

#ifndef THEME_H
#define THEME_H

#include "ccl.h"
#include "GroPainter.h"

class Theme {

public:

    Theme ( void );
    ~Theme ( void ) {}

    void set ( Value * rec );

    void apply_background ( GroPainter * painter );
    void apply_ecoli_edge_color ( GroPainter * painter, bool is_selected );
    void apply_message_color ( GroPainter * painter );
    void apply_chemostat_edge_color ( GroPainter * painter );
    void apply_mouse_color ( QPainter * painter );
    void accumulate_color ( int index, float val, float * r, float * g,  float * b );

private:

    std::string background;
    std::string ecoli_edge;
    std::string ecoli_selected;
    std::string chemostat_edge;
    std::string message;
    std::string mouse;

    std::vector< std::vector<float> > signal_colors;

    double br, bg, bb;

};

#endif // THEME_H
