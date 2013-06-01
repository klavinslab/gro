/////////////////////////////////////////////////////////////////////////////////////////
//
// gro is protected by the UW OPEN SOURCE LICENSE, which is summaraize here.
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

#ifndef CELL_H
#define CELL_H

#include "chipmunk_private.h"
#include "chipmunk_unsafe.h"
#include "ccl.h"
#include "Defines.h"
#include "Utility.h"

#ifndef NOGUI
#include "Theme.h"
#include "GroPainter.h"
#endif

#include <iostream>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <list>
#include <vector>
#include <string>
#include <map>


class World;
class MicroProgram;


////////////////////////////////////////////////////////////////////////////////
// Cell
//

class Cell {

 public:

  Cell ( World * w );
  ~Cell ( void );

  void init ( const int * q0, const int * rep0, float frac );

  inline void set ( int i, int x ) { q[i] = x; }
  inline int get ( int i ) { return q[i]; }
  inline int * get_q ( void ) { return q; }
  inline int get_rep ( int i ) { return rep[i]; }

  inline void inc ( int i, int x ) { q[i] += x; }
  inline void dec ( int i, int x ) { q[i] -= x; }

  inline void inc ( int i ) { q[i]++; }
  inline void dec ( int i ) { q[i]--; }

  inline void set_rep ( int i, int x ) { rep[i] = x; }

  inline void select ( void ) { selected = true; }
  inline void deselect ( void ) { selected = false; }
  inline bool is_selected ( void ) { return selected; }

  void set_prog ( MicroProgram * p ) { program = p; }

  float get_x ( void ) { return shape->body->p.x; }
  float get_y ( void ) { return shape->body->p.y; }
  float get_theta ( void ) { return shape->body->a; }

  virtual void update ( void ) {}
  virtual Value * eval ( Expr * ) { return NULL; }

#ifndef NOGUI
  virtual void render ( Theme *, GroPainter * ) {}
#endif

  virtual Cell * divide ( void ) { return NULL; }

  cpShape * get_shape ( void ) { return shape; }
  cpBody * get_body ( void ) { return body; }

  void set_id ( int i ) { id = i; }
  int get_id ( void ) { return id; }

  virtual float get_fluorescence ( int i ) { return rep[i]; }

  virtual float get_size ( void ) { return 1; }
  virtual float get_volume ( void ) { return 0.000001; }

  Program * get_gro_program ( void ) { return gro_program; }
  void set_gro_program ( Program * p ) { gro_program = p; }

  // getters and setters for parameters ////////////
  inline void set_param ( std::string str, float val ) { parameters[str] = val; }
  inline float get_param ( std::string str ) { return parameters[str]; }
  inline std::map<std::string,float> get_param_map ( void ) { return parameters; }
  inline void set_param_map ( std::map<std::string,float> p ) { parameters = p; }

  virtual void compute_parameter_derivatives ( void ) {  } // To avoid computing simple functions of parameters
                                                                 // over and over again, this function is called whenever gro
                                                                 // changes a parameter
  // end parameters ///////////////////////////////

  inline void mark_for_death ( void ) { marked = true; }
  inline bool marked_for_death ( void ) { return marked; }

  inline void set_division_indicator ( bool val ) { divided = val; }
  inline void set_daughter_indicator ( bool val ) { daughter = val; }

  inline bool just_divided ( void ) { return divided; }
  inline bool is_daughter ( void ) { return daughter; }

  virtual void force_divide ( void ) {}

 protected:

  cpSpace * space;
  cpShape * shape;
  cpBody * body;

  int q[MAX_STATE_NUM],
    rep[MAX_REP_NUM];

  World * world;
  MicroProgram * program;
  Program * gro_program;
  int id;

  // these ought to be gotten rid of
  float growth_rate,
    division_size_mean,
    division_size_variance;

  bool marked, divided, daughter, selected;

  std::map<std::string,float> parameters;

};

#endif // CELL_H
