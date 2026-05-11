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

    Value * v;

    v = rec->getField( "background" );

    if ( v && v->get_type() == Value::STRING ) {
      background = v->string_value();
    }

    QColor c ( background.c_str() );

    br = c.red() / 255.0;
    bg = c.green() / 255.0;
    bb = c.blue() / 255.0;

    v = rec->getField( "ecoli_edge" );

    if ( v && v->get_type() == Value::STRING ) {
      ecoli_edge = v->string_value();
    }

    v = rec->getField( "message" );

    if ( v && v->get_type() == Value::STRING ) {
      message = v->string_value();
    }

    v = rec->getField( "chemostat" );

    if ( v && v->get_type() == Value::STRING ) {
      chemostat_edge = v->string_value();
    }

    v = rec->getField( "mouse" );

    if ( v && v->get_type() == Value::STRING ) {
      mouse = v->string_value();
    }

    v = rec->getField( "ecoli_selected" );

    if ( v && v->get_type() == Value::STRING ) {
      ecoli_selected = v->string_value();
    }

    v = rec->getField( "signals" );
    int n, m;

    if ( v && v->get_type() == Value::LIST ) {

        std::list<Value *>::iterator i,j;
        signal_colors.resize(v->list_value()->size());

        for ( n=0, i=v->list_value()->begin(); i!=v->list_value()->end(); n++, i++) {

            Value * col = *i;
            signal_colors[n].resize(3);

            for ( m=0, j=col->list_value()->begin(); m<3 && j!=col->list_value()->end(); m++, j++) {
                signal_colors[n][m] = (*j)->num_value();
            }

        }

    }

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

