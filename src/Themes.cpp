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

#include "Micro.h"

void World::set_theme ( Value * v ) {

    theme.set ( v );

}

void Theme::set_colors ( const std::string & bg_,
                         const std::string & edge_,
                         const std::string & selected_,
                         const std::string & chemostat_,
                         const std::string & message_,
                         const std::string & mouse_,
                         const std::vector< std::vector<float> > & signal_palette ) {

    background     = bg_;
    ecoli_edge     = edge_;
    ecoli_selected = selected_;
    chemostat_edge = chemostat_;
    message        = message_;
    mouse          = mouse_;

    QColor c ( background.c_str() );
    br = c.red()   / 255.0;
    bg = c.green() / 255.0;
    bb = c.blue()  / 255.0;

    signal_colors = signal_palette;

}

Theme::Theme ( void ) {

    // default values
    background = "#000000";
    ecoli_edge = "#ffffff";
    ecoli_selected = "#ff000";
    mouse = "#333333";
    message = "#000000";
    chemostat_edge = "#000000";

    signal_colors.resize(2);
    signal_colors[0].resize(3);
    signal_colors[1].resize(3);

    signal_colors[0][0] = 1;
    signal_colors[0][1] = 0;
    signal_colors[0][2] = 1;

    signal_colors[1][0] = 0;
    signal_colors[1][1] = 1;
    signal_colors[1][2] = 1;

}

void Theme::set ( Value * rec ) {

    // Extract field-by-field from the CCL record, falling back to
    // the current value when a field is absent. Then forward to
    // set_colors so the actual member-assignment + br/bg/bb parse
    // lives in one place.
    auto str_or = [&](const char * key, const std::string & fallback) {
      Value * v = rec->getField(key);
      return (v && v->get_type() == Value::STRING) ? v->string_value()
                                                   : fallback;
    };

    std::vector<std::vector<float>> palette = signal_colors;
    Value * sig = rec->getField("signals");
    if (sig && sig->get_type() == Value::LIST) {
        palette.clear();
        palette.resize(sig->list_value()->size());
        int n = 0;
        for (auto i = sig->list_value()->begin();
             i != sig->list_value()->end(); ++n, ++i) {
            palette[n].resize(3);
            int m = 0;
            for (auto j = (*i)->list_value()->begin();
                 m < 3 && j != (*i)->list_value()->end(); ++m, ++j) {
                palette[n][m] = (*j)->num_value();
            }
        }
    }

    set_colors(
        str_or("background",     background),
        str_or("ecoli_edge",     ecoli_edge),
        str_or("ecoli_selected", ecoli_selected),
        str_or("chemostat",      chemostat_edge),
        str_or("message",        message),
        str_or("mouse",          mouse),
        palette);

}

void Theme::apply_background ( GroPainter * painter ) {

    painter->setBackground(QBrush(QColor(background.c_str())));
    painter->clear();

}

void Theme::apply_ecoli_edge_color ( GroPainter * painter, bool is_selected ) {

    if ( is_selected )
        painter->setPen ( QColor ( ecoli_selected.c_str() ) );
    else
        painter->setPen ( QColor ( ecoli_edge.c_str() ) );

}

void Theme::apply_message_color ( GroPainter * painter ) {

    painter->setPen ( QColor ( message.c_str() ) );

}

void Theme::apply_chemostat_edge_color ( GroPainter * painter ) {

    painter->setPen ( QPen ( QBrush ( QColor ( chemostat_edge.c_str() ) ), 8, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin ) );

}

void Theme::apply_mouse_color ( QPainter *painter ) {

    painter->setPen ( QPen(QBrush(QColor( mouse.c_str() ),Qt::SolidPattern), 1, Qt::DashDotLine ) );

}

void Theme::accumulate_color ( int index, float val, float * r, float * g,  float * b ) {

    double tval = min ( 1.0, max ( 0.0, val ) );
    int k = index%signal_colors.size();

    *r += br*(1-tval)+tval*signal_colors[k][0];
    *g += bg*(1-tval)+tval*signal_colors[k][1];
    *b += bb*(1-tval)+tval*signal_colors[k][2];

}

