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
#include "GroThread.h"
#include <QPainterPath>
#endif

#include "Micro.h"

#ifdef NOGUI
World::World ( void ) {
#else
World::World ( GroThread *ct ) : calling_thread ( ct ) {
#endif

    prog = NULL;
    chemostat_mode = false;
    next_id = 0;
    step = 0;
    print_rate = -1;
    movie_rate = -1;
    program_initialized = false;
    gro_message = "";
    stop_flag = false;
    tick_count = 0;
    zoom = 1.0;
    barriers = new std::list<Barrier>;

    set_sim_dt ( DEFAULT_SIM_DT );
    set_chip_dt ( DEFAULT_CHIP_DT );

}

World::~World ( void ) {

    std::list<Cell *>::iterator j;

    for ( j=population->begin(); j!=population->end(); j++ ) {
        delete (*j);
    }

    unsigned int k;

    for ( k=0; k<signal_list.size(); k++ )
        delete signal_list[k];

    cpSpaceFree(space);

    delete population;

    // Order matters: cells, signals, and chipmunk space are torn
    // down before prog->destroy() so destroy() cannot safely touch
    // them. Both PythonWorldProgram::destroy and the default no-op
    // honor that contract today; a future world program needing
    // World access during destroy would have to move this earlier.
    if (prog) {
        prog->destroy(this);
        // CCL's gro_Program is owned by GroThread; Python-installed
        // world programs (via set_main) hand ownership to World and
        // get deleted here.
        if (prog->owned_by_world()) {
            delete prog;
            prog = NULL;
        }
    }
    drain_pending_prog_deletions();
}

void World::drain_pending_prog_deletions ( void ) {
    for (MicroProgram * p : pending_prog_deletions) {
        delete p;
    }
    pending_prog_deletions.clear();
}

double World::stats ( const std::string & name ) {

    if ( name == "pop_size" ) return get_pop_size();

    // Throwing rather than silently returning 0 -- a typo in
    // stats("popsize") would otherwise fire game.py's `== 0` rule
    // on every tick. pybind11 surfaces std::string throws as Python
    // RuntimeError; CCL's eval loop will fail loudly the same way.
    throw std::string ( "unknown statistic '" + name + "' in call to stats()" );

}

void World::add_reaction ( const std::vector<int> & reactants,
                            const std::vector<int> & products,
                            float rate ) {

    Reaction r ( rate );

    for ( int idx : reactants ) {
        if ( idx < 0 || idx >= num_signals() )
            throw std::string ( "Reaction refers to a non-existant reactant." );
        r.add_reactant ( idx );
    }
    for ( int idx : products ) {
        if ( idx < 0 || idx >= num_signals() )
            throw std::string ( "Reaction refers to a non-existant product." );
        r.add_product ( idx );
    }

    add_reaction ( r );

}

void World::dispatch_set_param ( Cell * cc, const std::string & name, float val ) {
    if ( cc != NULL ) {
        cc->set_param ( name, val );
        cc->compute_parameter_derivatives();
        return;
    }
    if ( num_signals() > 0 &&
         ( name == "signal_grid_width"
           || name == "signal_grid_height"
           || name == "signal_element_size" ) ) {
        return; // refuse to resize the grid after signals are live
    }
    set_param ( name, val );

}

void Cell::init ( const int * q0, const int * rep0, float frac ) {

    int i;

    for ( i=0; i<MAX_STATE_NUM; i++ ) q[i] = (int) floor(frac*q0[i]);
    for ( i=0; i<MAX_REP_NUM; i++ ) rep[i] = (int) floor(frac*rep0[i]);

}

void World::init_state () {

    // Time
    t = 0.0f;
    max_val = 0.0f;

    // Cells
    population = new std::list<Cell *>; // deleted in ~World

    // Chipmunk stuff
    space = cpSpaceNew(); // freed in ~World
    cpSpaceSetIterations    ( space, ITERATIONS );
    cpSpaceSetDamping       ( space, DAMPING );
    cpSpaceSetCollisionSlop ( space, 0.2 );

    // World default parameters are set by the language stdlib, not
    // here: include/gro.gro for the CCL path, python/gro/__init__.py
    // for the Python path. Each is its language's "gro" library and
    // owns the defaults its programs see.

}

void World::init_chemostat_walls () {

    if ( !chemostat_mode )
        return;

    cpBody *staticBody = cpSpaceGetStaticBody(space);

    int w = get_param("chemostat_width")/2,
            h = get_param("chemostat_height")/2;

    auto add_wall = [&](cpVect a, cpVect b) {
        cpShape *s = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, a, b, 5.0f));
        cpShapeSetElasticity ( s, 1.0f );
        cpShapeSetFriction   ( s, 0.0f );
    };

    add_wall ( cpv(-400,h), cpv(-w,h)  );
    add_wall ( cpv(-w,h),   cpv(-w,-h) );
    add_wall ( cpv(-w,-h),  cpv(w,-h)  );
    add_wall ( cpv(w,-h),   cpv(w,h)   );
    add_wall ( cpv(w,h),    cpv(400,h) );

}

int World::add_new_signal ( float diffusion, float degradation ) {

    int w    = get_param("signal_grid_width");
    int h    = get_param("signal_grid_height");
    int numx = w / get_param("signal_element_size");
    int numy = h / get_param("signal_element_size");

    Signal * sig = new Signal (
        cpv(-w/2, -h/2), cpv(w/2, h/2), numx, numy,
        diffusion, degradation );

    add_signal(sig);
    return num_signals() - 1;

}

void World::init () {

    init_state();

    // Program (CCL path only; Python path runs the user file instead
    // of calling prog->init).
    ASSERT ( prog != NULL );

    if ( !program_initialized ) {

        prog->init(this); // if this throws an exception
        // the program will not be initialized
        // in the next line

        program_initialized = true;

    }

    init_chemostat_walls();

}

void World::emit_message ( std::string str, bool clear ) {

#ifndef NOGUI
    calling_thread->emit_message ( str, clear );
#else
    std::cerr << str << "\n";
#endif

}

void World::restart ( void ) {

    std::list<Cell *>::iterator j;

    for ( j=population->begin(); j!=population->end(); j++ ) {
        delete (*j);
    }

    cpSpaceFree(space);

    delete population;

    unsigned int k;
    for ( k=0; k<signal_list.size(); k++ ) {
        signal_list[k]->zero();
    }

    init();

    gro_message = "";

}

static char buf[1024];

#ifndef NOGUI

static void drawString ( GroPainter * painter, int x, int y, const char *str) {

    painter->drawText ( x, y, str );

}

void World::render ( GroPainter * painter ) {

    std::list<Cell *>::iterator j;
    int dec = 0;

    theme.apply_background ( painter );

    painter->scale(zoom,zoom);

    if ( signal_list.size() > 0 ) {

        int i,j;
        unsigned int k;
        float r, g, b;
        QColor col;
        float sum;

        for ( i=0; i<signal_list.front()->get_numx(); i++ ) {

            for ( j=0; j<signal_list.front()->get_numy(); j++ ) {

                r = 0; g = 0; b = 0;

                // determine overall signal level
                sum = 0.0;
                for ( k=0; k<signal_list.size(); k++ )
                    sum += signal_list[k]->get_val(i,j);

                if ( sum > 0.01 ) {

                    for ( k=0; k<signal_list.size(); k++ ) {

                        // accumulate color
                        theme.accumulate_color ( k, signal_list[k]->get_val(i,j), &r, &g, &b );

                    }

                }

                if ( sum > 0.01  ) {

                    col.setRgbF(r/k,g/k,b/k);
                    painter->fillRect(
                                signal_list[0]->get_minp().x+signal_list[0]->get_dw()*i,
                                signal_list[0]->get_minp().y+signal_list[0]->get_dh()*j,
                                (int) ( signal_list[0]->get_dw() ),
                                (int) ( signal_list[0]->get_dh() ),
                                col);

                }

            }

        }

        theme.apply_message_color(painter);

        // draw cross hairs for corners of signal area
        int x = get_param ( "signal_grid_width" ) / 2,
                y = get_param ( "signal_grid_height" ) / 2;

        painter->drawLine ( x-20, y,  x+20, y );
        painter->drawLine ( x, y-20,  x, y+20 );

        painter->drawLine ( -x-20, y,  -x+20, y );
        painter->drawLine ( -x, y-20,  -x, y+20 );

        painter->drawLine ( x-20, -y,  x+20, -y );
        painter->drawLine ( x, -y-20,  x, -y+20 );

        painter->drawLine ( -x-20, -y,  -x+20, -y );
        painter->drawLine ( -x, -y-20,  -x, -y+20 );

    }

    for ( j=population->begin(); j!=population->end(); j++ ) {
        (*j)->render ( &theme, painter );
    }

    theme.apply_chemostat_edge_color(painter);
    painter->setBrush( QBrush() );

    QPainterPath path;

    // Chemostat
    if ( chemostat_mode ) {

        int w = get_param("chemostat_width")/2,
                h = get_param("chemostat_height")/2;

        path.moveTo(-400,h);
        path.lineTo(-w,h);
        path.lineTo(-w,-h);
        path.lineTo(w,-h);
        path.lineTo(w,h);
        path.lineTo(400,h);

        //path.closeSubpath();
        painter->drawPath(path);

    }

    std::list<Barrier>::iterator b;

    for ( b=barriers->begin(); b != barriers->end(); b++ ) {
      path.moveTo((*b).x1,(*b).y1);
      path.lineTo((*b).x2,(*b).y2);
      painter->drawPath(path);
    }

    //  if ( 1 ) {
    //    static char buf[1000];
    //    sprintf ( buf, "%s", gro_message.c_str() );
    //    drawString ( painter, -360, 360-dec, buf );
    //    dec += 16;
    //  }

    painter->reset();

    theme.apply_message_color(painter);

    snprintf ( buf, sizeof(buf), "Cells: %d, Max: %d, t = %.2f min", (int) population->size(), (int) get_param ( "population_max" ), t );
    drawString ( painter, -painter->get_size().width()/2+10, -painter->get_size().height()/2+20, buf );
    dec = 32;

    message_handler.render(painter);

}

#endif

void World::add_cell ( Cell * c ) {

    population->push_back ( c );
    c->set_id ( next_id++ );
    cpSpaceStep(space, get_chip_dt());

}

static bool out_of_bounds ( float x, float y ) {

    return x < -400 || x > 400 || y < -400 || y > 400;

}

cpVect World::chemostat_flow ( float, float y, float mag ) {

    if ( y > get_param("chemostat_height")/2 ) return cpv ( mag, 0 );
    else return cpv ( 0, 0 );

}

void World::update ( void ) {

    tick_count.fetch_add(1);

    if ( population->size() < get_param ( "population_max" ) ) {

        if ( prog ) {
            prog->world_update ( this );  // null for Python-loaded worlds (no main yet)
            // If a rule body called set_main(...) and replaced the
            // currently-running prog, the old one was stashed for
            // deferred deletion. Now that world_update has returned
            // and the prog's C++ frame is gone, it's safe to free.
            drain_pending_prog_deletions();
        }
        std::list<Cell *>::iterator j;

        // update each cell
        for ( j=population->begin(); j!=population->end(); j++ ) {

            (*j)->update();

            // check for divisions
            Cell * d = (*j)->divide();
            if ( d != NULL ) add_cell ( d );

        }

        unsigned int i, J, k;

        for ( k=0; k<reaction_list.size(); k++ )
            for ( int i=0; i<signal_list.front()->get_numx(); i++ )
                for ( int J=0; J<signal_list.front()->get_numy(); J++ )
                    reaction_list[k].integrate ( &signal_list, i, J, get_sim_dt() );

        for ( k=0; k<signal_list.size(); k++ )
            signal_list[k]->integrate ( get_sim_dt() );

        for ( j=population->begin(); j!=population->end(); j++ )
            if ( (*j)->marked_for_death() ) {
                Cell * c = (*j);
                j = population->erase ( j );
                delete c;
            }

        // chemostat updates
        if ( chemostat_mode ) {

            for ( j=population->begin(); j!=population->end(); j++ ) {

                cpBody * cb = cpShapeGetBody((*j)->get_shape());
                // Chipmunk 5 did NOT auto-reset body->f between steps, so the
                // original `250*dt` per-step force accumulated over time and
                // grew quadratically. Chipmunk 7 zeros f every step, so we
                // apply 250 directly as a constant force.
                cpBodyApplyForceAtWorldPoint ( cb, chemostat_flow ( (*j)->get_x(), (*j)->get_y(), 3750 ), cpBodyGetPosition(cb) );

                if ( out_of_bounds ( (*j)->get_x(), (*j)->get_y() ) ) {

                    Cell * c = (*j);
                    j = population->erase ( j );
                    delete c;

                }

            }

        }

        for(int i=0; i<3; i++){
            cpSpaceStep(space, get_chip_dt());
        }

        t += get_sim_dt();

        if ( print_rate > 0 && step % print_rate == 0 )
            print();

        step++;

    } else {

        emit_message ( "Population limit reached. Increase the population limit via the Simulation menu, or by setting the parameter \"population_max\" in your gro program." );
        set_stop_flag(true);

    }

}

void World::add_signal ( Signal * s ) {

    signal_list.push_back ( s );

}

float World::get_signal_value ( Cell * c, int i ) {

    float s = c->get_size() / 3.0, a = c->get_theta();

    return (
                signal_list[i]->get ( (float) ( c->get_x() ), (float) ( c->get_y() ) ) +
                signal_list[i]->get ( (float) ( c->get_x() + s * cos ( a ) ), (float) ( c->get_y() + s * sin ( a ) ) ) +
                signal_list[i]->get ( (float) ( c->get_x() - s * cos ( a ) ), (float) ( c->get_y() - s * sin ( a ) ) )
                ) / 3.0;

}

void World::emit_signal ( Cell * c, int i, float ds ) {

    signal_list[i]->inc (  c->get_x(), c->get_y(), ds );

}

void World::absorb_signal ( Cell * c, int i, float ds ) {

    signal_list[i]->dec (  c->get_x(), c->get_y(), ds );

}

std::vector< std::vector<float> > * World::get_signal_matrix ( int i ) {

  // Pointer is into the Signal's internal storage -- DO NOT delete
  // it at the call site. The bounds check here means both CCL and
  // Python bindings can forward without re-checking, and an
  // out-of-range CCL call now raises instead of segfaulting in the
  // signal_list[i] indexing.
  if ( i < 0 || i >= num_signals() )
      throw std::string ( "get_signal_matrix: invalid signal handle" );
  return signal_list[i]->get_signal_matrix();

}

void World::print ( void ) {

    std::list<Cell *>::iterator j;
    int i;

    for ( j=population->begin(); j!=population->end(); j++ ) {

        printf ( "%d, %d, ", (*j)->get_id(), step );

        for ( i=0; i<MAX_REP_NUM-1; i++ )
            printf ( "%f, ", (*j)->get_fluorescence ( i ) );

        printf ( "%f\n", (*j)->get_fluorescence ( MAX_REP_NUM-1 ) );

    }

}

bool World::snapshot ( const char * path ) {

#ifndef NOGUI
    return calling_thread->snapshot(path);
#endif

}

#define NUM_BINS 12
static int bins[NUM_BINS+1];
static char histbuf[100];
int asd = 0;

void World::histogram ( float x, float y, float width, float height, int channel ) {

    int max_freq;
    std::list<Cell *>::iterator i;
    int j;
    float val;

    for ( j=0; j<NUM_BINS; j++ )
        bins[j]=0;

    max_val = 0.01;
    for ( i=population->begin(); i!=population->end(); i++ ) {
        val = (*i)->get_rep ( channel ) / (*i)->get_size();
        if ( val > max_val )
            max_val = val;
    }

    for ( i=population->begin(); i!=population->end(); i++ ) {
        val = (*i)->get_rep ( channel ) / (*i)->get_size();
        bins[ (int) ( ( NUM_BINS * val) / max_val )]++;
    }

    max_freq = 1;
    for ( j=0; j<NUM_BINS; j++ ) {
        if ( bins[j] > max_freq )
            max_freq = bins[j];
    }
#if PORTED_TO_QT
    if ( channel == 0 )
        glColor3f ( 0.5f, 0.8f, 0.5f);
    else if ( channel == 1 )
        glColor3f ( 0.8f, 0.5f, 0.5f);
    else if ( channel == 2 )
        glColor3f ( 0.8f, 0.8f, 0.5f);

    glBegin(GL_QUADS);
    for ( j=0; j<NUM_BINS; j++ ) {
        glVertex2f ( x + width * (j+0.1) / NUM_BINS, y  );
        glVertex2f ( x + width * (j+0.1) / NUM_BINS, y + height * bins[j] / max_freq );
        glVertex2f ( x + width * (j+1) / NUM_BINS, y + height * bins[j] / max_freq );
        glVertex2f ( x + width * (j+1) / NUM_BINS, y );
    }
    glEnd();

    glLineWidth(1.0f);

    glBegin(GL_LINE_STRIP);
    glVertex2f ( x, y+height );
    glVertex2f ( x, y );
    glVertex2f ( x+width, y );
    glEnd();

    glBegin(GL_LINES);
    glVertex2f ( x+width, y ); glVertex2f ( x+width, y-4 );
    glVertex2f ( x, y+height ); glVertex2f ( x-4, y+height );
    glEnd();

    sprintf ( histbuf, "Channel %d", channel );
    drawString ( x, y + height + 8, histbuf );

    sprintf ( histbuf, "%d", max_freq );
    drawString ( x-24, y + height-4, histbuf );

    sprintf ( histbuf, "%.2f", max_val );
    drawString ( x+width-8, y - 16, histbuf );
#endif

}

void World::scatter ( float x, float y, float width, float height, int channel1, int channel2 ) {

    float max_val = 0.0;
    std::list<Cell *>::iterator i;

    for ( i=population->begin(); i!=population->end(); i++ ) {
        if ( (*i)->get_rep ( channel1 ) / (*i)->get_size() > max_val )
            max_val = (*i)->get_rep ( channel1 ) / (*i)->get_size();
        if ( (*i)->get_rep ( channel2 ) / (*i)->get_size() > max_val )
            max_val = (*i)->get_rep ( channel2 ) / (*i)->get_size();
    }
#if PORTED_TO_QT
    glLineWidth(1.0f);
    glColor3f ( 0.8f, 0.8f, 0.8f);

    glBegin(GL_LINE_STRIP);
    glVertex2f ( x, y+height );
    glVertex2f ( x, y );
    glVertex2f ( x+width, y );
    glEnd();

    glBegin(GL_LINES);
    glVertex2f ( x+width, y ); glVertex2f ( x+width, y-4 );
    glVertex2f ( x, y+height ); glVertex2f ( x-4, y+height );
    glEnd();

    glPointSize ( 2.0 );

    glBegin(GL_POINTS);
    for ( i=population->begin(); i!=population->end(); i++ ) {
        glVertex2f ( x + ( (*i)->get_rep ( channel1 ) / (*i)->get_size() ) * width / max_val,
                     y + ( (*i)->get_rep ( channel2 ) / (*i)->get_size() ) * height / max_val );
    }
    glEnd();

    sprintf ( histbuf, "Channel %d vs. %d", channel1, channel2 );
    drawString ( x, y + height + 8, histbuf );
#endif
}

void World::select_cells ( int x1, int y1, int x2, int y2 ) {

    std::list<Cell *>::iterator i;

    int
            X1 = (1/zoom)*min ( x1, x2 ),
            X2 = (1/zoom)*max ( x1, x2 ),
            Y1 = (1/zoom)*min ( y1, y2 ),
            Y2 = (1/zoom)*max ( y1, y2 );

    for ( i=population->begin(); i!=population->end(); i++ ) {
        if ( X1-5 <= (*i)->get_x() && (*i)->get_x() <= X2+5 && Y1-5 <= (*i)->get_y() && (*i)->get_y() <= Y2+10 ) {
            (*i)->select();
        }
    }

}

void World::deselect_all_cells ( void ) {

    std::list<Cell *>::iterator i;

    for ( i=population->begin(); i!=population->end(); i++ ) {
        (*i)->deselect();
    }

}

void World::dump ( FILE * fp ) {

    std::list<Cell *>::iterator i;

    fprintf ( fp, "id, x, y, theta, volume, gfp, rfp, yfp, cfp\n" );

    for ( i=population->begin(); i!=population->end(); i++ ) {
        fprintf ( fp, "%d, %f, %f, %f, %f, %d, %d, %d, %d\n",
                  (*i)->get_id(), (*i)->get_x(), (*i)->get_y(), (*i)->get_theta(), (*i)->get_volume(),
                  (*i)->get_rep(GFP), (*i)->get_rep(RFP), (*i)->get_rep(YFP), (*i)->get_rep(CFP) );
    }

}

void World::add_barrier ( float x1, float y1, float x2, float y2 ) {

    // Physics side: a static chipmunk segment so cells collide with
    // it. Elastic + frictionless so cells bounce cleanly along walls.
    cpShape * shape = cpSpaceAddShape(
        space,
        cpSegmentShapeNew(cpSpaceGetStaticBody(space),
                          cpv(x1, y1), cpv(x2, y2), 5.0f));
    cpShapeSetElasticity(shape, 1.0f);
    cpShapeSetFriction  (shape, 0.0f);

    // Render side: remember the endpoints so the painter can draw
    // the wall.
    Barrier * b = new Barrier;
    b->x1 = x1;
    b->y1 = y1;
    b->x2 = x2;
    b->y2 = y2;
    barriers->push_back( *b );

}
