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

#include "Programs.h"
#include "EColi.h"

#define Q cell->get_q()

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  OPEN LOOP GFP
//

#define RNA Q[0]
#define PROTEIN Q[1]

void GFP_Program::init ( World * world ) {

  EColi * c = new EColi ( world, 0, 0, 0.78, 2*WIDTH );

  c->set ( 0, R0 );
  c->set ( 1, P0 );
  c->set_rep ( GFP, P0 );

  world->add_cell ( c );

  printf ( "initializing GFP program\n" );

}

void GFP_Program::update ( World *, Cell * cell ) {

  PRODUCE ( RNA, 0.01 );
  CONSUME ( RNA, 0.002*RNA );

  PRODUCE ( PROTEIN, 2.0*RNA );
  CONSUME ( PROTEIN, 0.0002*PROTEIN );

  cell->set_rep ( GFP, PROTEIN );

}

void GFP_Program::destroy ( World * ) {

}

#undef RNA
#undef PROTEIN

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  BISTABLE SWITCH
//

#define X1 cell->get_q()[2]
#define X2 cell->get_q()[3]
#define GREEN cell->get_q()[0]
#define RED cell->get_q()[1]
#define RATE1 0.004+0.5/(0.03*X2*X2+1)
#define RATE2 0.004+0.5/(0.03*X1*X1+1)
#define DEG1 0.0001
#define DEG2 0.0002

void Bistable_Program::init ( World * world ) {

  EColi * c = new EColi ( world, 0, 0, 0.78, 2*WIDTH );
  world->add_cell ( c );

}

void Bistable_Program::update ( World *, Cell * cell ) {

  PRODUCE ( X1, RATE1 );
  PRODUCE ( GREEN, RATE1 );

  PRODUCE ( X2, RATE2 );
  PRODUCE ( RED, RATE2 );

  CONSUME ( X1, DEG1*X1 );
  CONSUME ( GREEN, DEG2*GREEN );

  CONSUME ( X2, DEG1*X2 );
  CONSUME ( RED, DEG2*RED );

  cell->set_rep ( GFP, 2*GREEN );
  cell->set_rep ( RFP, 2*RED );

}

void Bistable_Program::destroy ( World * ) {

}

#undef X1
#undef X2
#undef RED
#undef GREEN
#undef RATE1 
#undef RATE2
#undef DEG1
#undef DEG2

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  BISTABLE SWITCH
//

#define PROTEIN cell->get_q()[0]
#define RNA cell->get_q()[1]

void Sensor_Program::init ( World * world ) {

  EColi * c = new EColi ( world, 0, 0, 0.78, 2*WIDTH );
  world->add_cell ( c );

  Signal * ahl = new Signal ( cpv ( -400, -400 ), cpv ( 400, 400 ), 150, 150, 4, 0 );
  ahl->set ( 100.0f, 100.0f, 1000.0f );

  world->add_signal ( ahl );

}

void Sensor_Program::update ( World * world, Cell * cell ) {

  float a = world->get_signal_value ( cell, 0 );

  PRODUCE ( RNA, 0.005+0.1*a );
  CONSUME ( RNA, 0.002*RNA );

  PRODUCE ( PROTEIN, 0.04*RNA );
  CONSUME ( PROTEIN, 0.0001*PROTEIN );

  cell->set_rep ( GFP, PROTEIN );

}

void Sensor_Program::destroy ( World * ) {

}

#undef RNA
#undef PROTEIN

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  LEADER ELECTION
//

void LeaderElection::init ( World * world ) {

  EColi * c = new EColi ( world, 0, 0, 0.78, 2*WIDTH );
  world->add_cell ( c );
  Signal * ahl = new Signal ( cpv ( -400, -400 ), cpv ( 400, 400 ), 150, 150, 4, 0 );
  world->add_signal ( ahl );

}

void LeaderElection::update ( World * world, Cell * cell ) {

  float a = world->get_signal_value ( cell, 0 );

  int Q0 = cell->get(0), Q1 = cell->get(1);

  if ( Q0 == 0 && Q1 == 0 ) { // Undecided

    if ( a > 0.05 ) 
      cell->set(0,100);
    else if ( frand() < 0.004 ) 
      cell->set(1,100);    

  } else if ( Q0 > 0 && Q1 == 0 ) { // Follower

    cell->set(0,100);
    world->emit_signal(cell,0,0.25);

  }  else if ( Q0 == 0 && Q1 > 0 ) { // Leader

    cell->set(1,100);
    world->emit_signal(cell,0,0.5);

  } else if ( Q0 > 0 && Q1 > 0 ) {

    printf ( "oops\n" );

  }
 
  cell->set_rep ( GFP, Q0>0?500:0 );
  cell->set_rep ( RFP, Q1>0?500:0 );


}

void LeaderElection::destroy ( World * ) {

}
