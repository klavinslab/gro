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
#include "EColi.h"
#include "Programs.h"

static gro_Program * current_gro_program = NULL;
static Cell * current_cell = NULL;

bool parse_error = false;

Program * split_gro_program ( Program * parent, float frac ) {

  Program * child = parent->copy();
  SymbolTable * symtab = parent->get_symtab();
  SymbolTable * new_symtab = child->get_symtab();

  //printf ( "before: %d %d\n", symtab->get("yfp")->int_value(), new_symtab->get("yfp")->int_value() );

  symtab->divide ( new_symtab, frac );

  //printf ( "after: %d %d\n", symtab->get("yfp")->int_value(), new_symtab->get("yfp")->int_value() );

  return child;

}

Value * new_ecoli ( std::list<Value *> * args, Scope * s ) {

  std::list<Value *>::iterator i = args->begin();
  Value * settings = *i; i++;

  ASSERT ( args->size() == 2 );

  Program * prog = (*i)->program_value()->copy(); // this copy is deleted in ~Cell?
  //printf ( ">> prog = %x in new_ecoli\n", prog );

  World * world = current_gro_program->get_world();

  float x = 0, y = 0, theta = 0, vol;

  if ( settings->get_type() == Value::RECORD ) {

    if ( settings->getField ( "x" ) )
      x = settings->getField ( "x" )->real_value();

    if ( settings->getField ( "y" ) )
      y = settings->getField ( "y" )->real_value();

    if ( settings->getField ( "theta" ) )
      theta = settings->getField ( "theta" )->real_value();

    if ( settings->getField ( "volume" ) )
      vol = settings->getField ( "volume" )->real_value();
    else
      vol = DEFAULT_ECOLI_INIT_SIZE;

  } else {

    fprintf ( stderr, "First argument to 'ecoli' should be a record\n" );
    exit ( -1 );

  }

  EColi * c = new EColi ( world, x, y, theta, vol );

  current_cell = c;
  c->set_gro_program ( prog ); // prog is deleted if/when the cell is deleted in ~Cell
  world->add_cell ( c );
  prog->init_params ( current_gro_program->get_scope() );
  prog->init ( current_gro_program->get_scope() );
  current_cell = NULL;

  return new Value(Value::UNIT);

}


Value * new_signal ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();

  std::list<Value *>::iterator i = args->begin();
  Value * kdi = *i; i++;
  Value * kde = *i;

  int w, h, numx, numy;

  w = world->get_param ( "signal_grid_width" );
  h = world->get_param ( "signal_grid_height" );
  numx = w / world->get_param ( "signal_element_size" );
  numy = h / world->get_param ( "signal_element_size" );

  Signal * sig = new Signal (
     cpv ( -w/2, -h/2 ), cpv ( w/2, h/2 ), numx, numy,
     kdi->num_value(), kde->num_value() );

  world->add_signal ( sig );

  return new Value ( world->num_signals() - 1 );

}

Value * add_reaction ( std::list<Value *> * args, Scope * s ) {

    World * world = current_gro_program->get_world();

    // args are: reactants, products, rate
    std::list<Value *>::iterator i = args->begin();
    Value * R = *i; i++;
    Value * P = *i; i++;
    Value * k = *i;

    Reaction r (k->num_value());

    for ( i=R->list_value()->begin(); i != R->list_value()->end(); i++ ) {
        if ( (*i)->int_value() < 0 || (*i)->int_value() >= world->num_signals() )
            throw std::string ( "Reaction refers to a non-existant reactant." );
        r.add_reactant( (*i)->int_value() );
    }

    for ( i=P->list_value()->begin(); i != P->list_value()->end(); i++ ) {
        if ( (*i)->int_value() < 0 || (*i)->int_value() >= world->num_signals() )
            throw std::string ( "Reaction refers to a non-existant product." );
        r.add_product( (*i)->int_value() );
    }

    world->add_reaction(r);

    return new Value ( Value::UNIT );

}

Value * set_signal ( std::list<Value *> * args, Scope * s ) {

  // n, x, y, v


  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  ASSERT ( args->size() == 4 );

  Value * n = *i; i++;
  Value * x = *i; i++;
  Value * y = *i; i++;
  Value * val = *i; 

  world->set_signal ( n->int_value(), x->num_value(), y->num_value(), val->num_value() );

  return new Value ( Value::UNIT );

}

Value * set_signal_rect ( std::list<Value *> * args, Scope * s ) {

  // n, x1, y1, x2, y2, v

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  ASSERT ( args->size() == 6 );

  Value * n = *i; i++;

  Value * x1 = *i; i++;
  Value * y1 = *i; i++;
  Value * x2 = *i; i++;
  Value * y2 = *i; i++;

  Value * val = *i;

  world->set_signal_rect ( n->int_value(), x1->num_value(), y1->num_value(), x2->num_value(), y2->num_value(), val->num_value() );

  return new Value ( Value::UNIT );

}

Value * get_signal ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  Value * n = *i;

  if ( current_cell != NULL ) {
    ASSERT ( n->int_value() < world->num_signals() );
    return new Value ( (double) world->get_signal_value ( current_cell, n->int_value() ) );
  } else {
    printf ( "Warning: Tried to get signal value from outside a cell program. No action taken\n" );
    return new Value ( 0.0 );
  }

}

Value * emit_signal ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  Value * n = *i; i++;
  Value * ds = *i;

  if ( current_cell != NULL ) 
    world->emit_signal ( current_cell, n->int_value(), ds->num_value() );
  else
    printf ( "Warning: Tried to emit signal from outside a cell program. No action taken\n" );

  return new Value ( Value::UNIT );

}

Value * absorb_signal ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  Value * n = *i; i++;
  Value * ds = *i;

  if ( current_cell != NULL ) 
    world->absorb_signal ( current_cell, n->int_value(), ds->num_value() );
  else
    printf ( "Warning: Tried to absorb signal from outside a cell program. No action taken\n" );

  return new Value ( Value::UNIT );

}

Value * die ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  if ( current_cell != NULL ) 
    current_cell->mark_for_death();
  else
    printf ( "Warning: Called die() from outside a cell program. No action taken\n" );

  return new Value ( Value::UNIT );

}

Value * reset ( std::list<Value *> * args, Scope * s ) {

  current_gro_program->get_world()->restart();
  return new Value ( Value::UNIT );

}

Value * stop ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  world->set_stop_flag(true);
  return new Value ( Value::UNIT );

}

Value * start ( std::list<Value *> * args, Scope * s ) {

  //run_simulation ( true );
  return new Value ( Value::UNIT );

}

Value * set_param ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  Value * name = *i; i++;
  Value * val = *i;

  if ( current_cell == NULL ) { // This is a global parameter //////////////////////////////////

      if ( ( name->string_value() != "signal_grid_width"
          && name->string_value() != "signal_grid_height"
          && name->string_value() != "signal_element_size" ) || world->num_signals() == 0 ) {

          world->set_param ( name->string_value(), val->num_value() );

      }

  } else { /////////////////////// This is a cell-specific parameter ///////////////////////////

    current_cell->set_param ( name->string_value(), val->num_value() );
    current_cell->compute_parameter_derivatives();

  }

  //if ( name->string_value() == "throttle" ) set_throttle ( val->num_value() != 0.0 );

  return new Value ( Value::UNIT );

}

Value * world_stats ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  Value * name = *i;

  if ( name->string_value() == "pop_size" ) {

    return new Value ( world->get_pop_size() );
    
  } else printf ( "unknown statistic %s in call to 'stat'\n", name->string_value().c_str() );

  return new Value ( 0 );

}

Value * message ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  Value * n = *i;
  i++;
  Value * mes = *i;

  world->message ( n->int_value(), mes->string_value() );

  return new Value ( Value::UNIT );

}

Value * clear_messages ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  Value * n = *i;

  world->clear_messages ( n->int_value() );

  return new Value ( Value::UNIT );

}

Value * snapshot ( std::list<Value *> * args, Scope * s ) {

  if ( true /* used to check for whether the gui was instantiated */ ) {
    World * world = current_gro_program->get_world();
    std::list<Value *>::iterator i = args->begin();
    Value * name = *i;
    world->snapshot ( name->string_value().c_str() );
  }

  return new Value ( 0 );

}

Value * force_divide ( std::list<Value *> * args, Scope * s ) {

  if ( current_cell != NULL ) {

    current_cell->force_divide();

  } else fprintf ( stderr, "Warning: divide() called from outside a cell\n" );

  return new Value ( Value::UNIT );

}

Value * chemostat ( std::list<Value *> * args, Scope * s ) {

    World * world = current_gro_program->get_world();
    std::list<Value *>::iterator i = args->begin();

    Value * val = *i;

    world->set_chemostat_mode(val->bool_value());

    return new Value ( Value::UNIT );

}

Value * World::map_to_cells ( Expr * e ) {

  std::list<Value *> * L = new std::list<Value *>;
  std::list<Cell *>::iterator j;

  for ( j=population->begin(); j!=population->end(); j++ ) {
    L->push_back ( (*j)->eval ( e ) );
  }

  return new Value ( L );

}

Value * map_to_cells (  std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();

  std::list<Value *>::iterator i = args->begin();
  Expr * expression = (*i)->func_body();

  while ( expression->get_type() == Expr::CONSTANT ) {
    expression = expression->get_val()->func_body();
  }

  return world->map_to_cells ( expression );

}

Value * run ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  float dvel = (*i)->num_value();

  if ( current_cell != NULL ) {

    float a = current_cell->get_theta();
    cpBody * body = current_cell->get_body();
    cpVect v = cpBodyGetVel ( body );
    cpFloat adot = cpBodyGetAngVel ( body );

    cpBodySetTorque ( body, -adot ); // damp angular rotation

    cpBodyApplyForce ( // apply force
      current_cell->get_shape()->body, 
      cpv ( 
        ( dvel*cos(a) - v.x ) * world->get_sim_dt(),
        ( dvel*sin(a) - v.y ) * world->get_sim_dt()
      ), 
      cpv ( 0, 0 ) );

  } else

    printf ( "Warning: Tried to emit signal from outside a cell program. No action taken\n" );

  return new Value ( Value::UNIT );

}

Value * tumble ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  float vel = (*i)->num_value(); 

  if ( current_cell != NULL ) {

    float a = current_cell->get_theta();
    cpBody * body = current_cell->get_body();
    cpVect v = cpBodyGetVel ( body );
    cpFloat adot = cpBodyGetAngVel ( body );

    cpBodySetTorque ( body, vel - adot ); // apply torque

    cpBodyApplyForce ( // damp translation
      current_cell->get_shape()->body, 
      cpv ( 
        - v.x * world->get_sim_dt(),
        - v.y * world->get_sim_dt()
      ), 
      cpv ( 0, 0 ) );

  } else

    printf ( "Warning: Tried to emit signal from outside a cell program. No action taken\n" );

  return new Value ( Value::UNIT );

}

Value * zoom ( std::list<Value *> * args, Scope * s ) {

  World * world = current_gro_program->get_world();
  std::list<Value *>::iterator i = args->begin();

  float z = (*i)->num_value();

  if ( z > 0 ) {

      world->set_zoom ( z );

  }

  return new Value ( Value::UNIT );

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Gro Class Methods
// 
// 

gro_Program::gro_Program ( const char * path, int ac, char ** av ) : MicroProgram(), pathname(path), scope(NULL), argc(ac), argv(av) {

  current_gro_program = this;

}

void gro_Program::add_method ( Value::TYPE t, int num_args, const char * name, EXTERNAL_CCLI_FUNCTION f ) {

  ASSERT ( scope != NULL );

  int i;
  TypeExpr * R = new TypeExpr ( t );
  std::list<TypeExpr *> * Ta = new std::list<TypeExpr *>;
  for ( i=0; i<num_args; i++ )
    Ta->push_back ( new TypeExpr ( true ) ); 
  scope->add ( name, new Value ( f, R, Ta ) ); // Ta and its elements should be deleted when scope is deleted

}

bool init_ccl = false;

void gro_Program::init ( World * w ) {

  world = w;

  scope = new Scope; // deleted in ::destroy

  scope->push ( new SymbolTable ( 100 ) );

  // Initialize Globals : this is in the top scope.
  scope->add ( "dt", new Value ( world->get_sim_dt() ) );
  scope->add ( "just_divided", new Value ( false ) );
  scope->add ( "daughter", new Value ( false ) );
  scope->add ( "selected", new Value ( false ) );
  scope->add ( "volume", new Value ( 0.0000000001 ) );
  scope->add ( "id", new Value ( 0 ) ); // these values should be deleted when scope is deleted

  // command line arguments
  std::list<Value *> * args = new std::list<Value *>;

  int i;
  for ( i=0; i<argc; i++ ) {
      args->push_back ( new Value ( argv[i] ) );
  }

  scope->add ( "ARGC", new Value ( (int) args->size() ) );
  scope->add ( "ARGV", new Value ( args ) );

  parse_error = false;

  current_cell = NULL;

  int parse_ok;

  try {

      parse_ok = readOrganismProgram ( scope, pathname.c_str() );

  }

  catch ( std::string err ) {

      parse_error = true;
      throw err;

  }

  if ( parse_ok < 0 ) {
      parse_error = true;
      throw std::string ( "un-ported parse error" );
  }

  world_update_program = scope->get_program ( "main" );

  if ( world_update_program ) {
    world_update_program->init_params ( scope );
    world_update_program->init ( scope );
  } 

}

Value * gro_Program::eval ( World * world, Cell * cell, Expr * e ) {

  current_cell = cell;

  Program * gro = cell->get_gro_program();

  ASSERT ( gro != NULL );

  SymbolTable * symtab = gro->get_symtab();

  if ( cell->just_divided() ) {

    scope->assign ( "just_divided", new Value ( true ) );    
    cell->set_division_indicator ( false ); 

    if ( cell->is_daughter() ) {
      scope->assign ( "daughter", new Value ( true ) ); 
      cell->set_daughter_indicator ( false ); 
    } else {
      scope->assign ( "daughter", new Value ( false ) ); 
    }

  } else {

    scope->assign ( "just_divided", new Value ( false ) );    
    scope->assign ( "daughter", new Value ( false ) ); 

  }

  scope->assign ( "dt", new Value ( world->get_sim_dt() ) ); 
  scope->assign ( "volume", new Value ( cell->get_volume() ) ); 
  scope->assign ( "selected", new Value ( cell->is_selected() ) );
  scope->assign ( "id", new Value ( cell->get_id() ) );

  scope->push ( gro->get_symtab() );
  Value * v = e->eval ( scope );
  scope->pop();

  return v;

}

void gro_Program::update ( World * world, Cell * cell ) {

  if ( !parse_error ) {

  current_cell = cell;

  Program * gro = cell->get_gro_program();

  ASSERT ( gro != NULL );

  SymbolTable * symtab = gro->get_symtab();

  if ( cell->just_divided() ) {

    scope->assign ( "just_divided", new Value ( true ) );    
    cell->set_division_indicator ( false ); 

    if ( cell->is_daughter() ) {
      scope->assign ( "daughter", new Value ( true ) ); 
      cell->set_daughter_indicator ( false ); 
    } else {
      scope->assign ( "daughter", new Value ( false ) ); 
    }

  } else {

    scope->assign ( "just_divided", new Value ( false ) );    
    scope->assign ( "daughter", new Value ( false ) ); 

  }

  scope->assign ( "dt", new Value ( world->get_sim_dt() ) ); 
  scope->assign ( "volume", new Value ( cell->get_volume() ) ); 
  scope->assign ( "selected", new Value ( cell->is_selected() ) );
  scope->assign ( "id", new Value ( cell->get_id() ) );

  gro->step(scope);

  Value * v = symtab->get ( "gfp" );
  if ( v != NULL )
    cell->set_rep ( GFP, v->real_value() );

  v = symtab->get ( "rfp" );
  if ( v != NULL )
    cell->set_rep ( RFP, v->real_value() );

  v = symtab->get ( "yfp" );
  if ( v != NULL )
    cell->set_rep ( YFP, v->real_value() );

  v = symtab->get ( "cfp" );
  if ( v != NULL )
    cell->set_rep ( CFP, v->real_value() );

  current_cell = NULL;

  }

}

void gro_Program::world_update ( World * world ) {

  if ( !parse_error ) {

  if ( world_update_program ) 
    world_update_program->step ( scope );

  }

}

void gro_Program::destroy ( World * world ) {

  if ( scope != NULL ) {
    delete scope; // this would be the case if this isn't the first time init was called
  }

}

std::string gro_Program::name ( void ) const {

  return pathname;

}

Value * set_theme ( std::list<Value *> * args, Scope * s ) {

#ifndef NOGUI

  ASSERT ( args->size() == 1 );

  std::list<Value *>::iterator i = args->begin();
  current_gro_program->get_world()->set_theme ( *i );

#endif

  return new Value ( Value::UNIT );

}

Value * gro_print ( std::list<Value *> * args, Scope * s ) {

  std::list<Value *>::iterator i;
  std::stringstream strm;

  for ( i=args->begin(); i!=args->end(); i++ ) {
      if ( (*i)->get_type() == Value::STRING )
          strm << (*i)->string_value();
      else
        strm << (*i)->tostring();
  }

#ifndef NOGUI
  current_gro_program->get_world()->emit_message(strm.str());
#else
  std::cout << strm.str();
#endif

  return new Value(Value::UNIT);

}

Value * gro_clear ( std::list<Value *> * args, Scope * s ) {

  std::string str = "";
  current_gro_program->get_world()->emit_message(str,true);
  return new Value(Value::UNIT);

}

Value * gro_fopen ( std::list<Value *> * args, Scope * s ) {

    World * world = current_gro_program->get_world();
    std::list<Value *>::iterator i = args->begin();
    Value * name = *i; i++;
    Value * mod = *i;

    FILE * f = fopen ( name->string_value().c_str(), mod->string_value().c_str() );

    if ( f ) {

        world->fileio_list.push_back(f);
        return new Value ( (int) world->fileio_list.size() - 1 );

    } else {

        return new Value(-1);

    }

}

Value * gro_fprint ( std::list<Value *> * args, Scope * s ) {

    World * world = current_gro_program->get_world();
    std::list<Value *>::iterator i = args->begin();
    int index = (*i)->int_value(); i++;

    if ( 0 <= index && index < world->fileio_list.size() ) {

        std::stringstream strm;

        for ( ; i!=args->end(); i++ ) {
            if ( (*i)->get_type() == Value::STRING )
                strm << (*i)->string_value();
            else
                strm << (*i)->tostring();
        }

        fprintf ( world->fileio_list[index], strm.str().c_str() );
        fflush ( world->fileio_list[index] );
    }

    return new Value ( Value::UNIT );

}

Value * gro_dump ( std::list<Value *> * args, Scope * s ) {

    World * world = current_gro_program->get_world();
    std::list<Value *>::iterator i = args->begin();
    int index = (*i)->int_value(); i++;

    if ( 0 <= index && index < world->fileio_list.size() ) {

        world->dump(world->fileio_list[index]);
        fflush ( world->fileio_list[index] );

    }

    return new Value ( Value::UNIT );

}

Value * gro_time ( std::list<Value *> * args, Scope * s ) {

    World * world = current_gro_program->get_world();

    return new Value ( world->get_time() );

}

void register_gro_functions ( void ) {

  // Parameters
  register_ccl_function ( "set", set_param );
  register_ccl_function ( "zoom", zoom );

  // Cell types
  register_ccl_function ( "ecoli", new_ecoli );

  // Signals
  register_ccl_function ( "signal",        new_signal );
  register_ccl_function ( "set_signal",    set_signal );
  register_ccl_function ( "set_signal_rect", set_signal_rect );
  register_ccl_function ( "get_signal",    get_signal );
  register_ccl_function ( "emit_signal",   emit_signal );
  register_ccl_function ( "absorb_signal", absorb_signal );
  register_ccl_function ( "reaction",      add_reaction );

  // World control
  register_ccl_function ( "reset",          reset );
  register_ccl_function ( "stop",           stop );
  register_ccl_function ( "start",          start );
  register_ccl_function ( "stats",          world_stats );
  register_ccl_function ( "snapshot",       snapshot );
  register_ccl_function ( "message",        message );
  register_ccl_function ( "clear_messages", clear_messages );
  register_ccl_function ( "set_theme",      set_theme );
  register_ccl_function ( "dump",           gro_dump );
  register_ccl_function ( "time",           gro_time );

  // Misc
  register_ccl_function ( "die", die );
  register_ccl_function ( "divide", force_divide );
  register_ccl_function ( "map_to_cells", map_to_cells );
  register_ccl_function ( "run", run );
  register_ccl_function ( "tumble", tumble );
  register_ccl_function ( "print", gro_print );
  register_ccl_function ( "clear", gro_clear );
  register_ccl_function ( "chemostat", chemostat );

  // File I/O
  register_ccl_function ( "fopen", gro_fopen );
  register_ccl_function ( "fprint", gro_fprint );

}
