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

#ifndef _PROGRAM_H_
#define _PROGRAM_H_

#include "Micro.h"
#include "ccl.h"

class GFP_Program : public MicroProgram {

 public:
  GFP_Program ( int r, int p ) : MicroProgram(), R0 ( r ), P0 (p ) {}
  void init ( World * world );
  void update ( World * world, Cell * cell );
  void destroy ( World * world );
  std::string name ( void ) const { return "Open Loop GFP"; }

 private:
  int R0, // initial amount of RNA
      P0; // initial amount of Protein

};

class Bistable_Program : public MicroProgram {

 public:

   Bistable_Program ( void ) : MicroProgram() {}
   void init ( World * world );
   void update ( World * world, Cell * cell );
   void destroy ( World * world );
   std::string name ( void ) const { return "Bistable Switch"; }

 private:

};

class Sensor_Program : public MicroProgram {

 public:

   Sensor_Program ( void ) : MicroProgram() {}
   void init ( World * world );
   void update ( World * world, Cell * cell );
   void destroy ( World * world );
   std::string name ( void ) const { return "Signal Sensor"; }

 private:

};

class LeaderElection : public MicroProgram {

 public:

   LeaderElection ( void ) : MicroProgram() {}
   void init ( World * world );
   void update ( World * world, Cell * cell );
   void destroy ( World * world );
   std::string name ( void ) const { return "Leader Election"; }

 private:

};

class TIR1_AuxIAA : public MicroProgram {

 public:

   TIR1_AuxIAA ( void ) : MicroProgram() {}
   void init ( World * world );
   void update ( World * world, Cell * cell );
   void world_update ( World * world );
   void destroy ( World * world );
   std::string name ( void ) const { return "TIR1 - Aux/IAA"; }

 private:

};

Program * split_gro_program ( Program * parent, float frac );

class gro_Program : public MicroProgram {

 public:

   gro_Program ( const char * path, int ac, char ** av );
   void init ( World * world );
   void update ( World * world, Cell * cell );
   Value * eval ( World * world, Cell * cell, Expr * e );
   void world_update ( World * world );
   void destroy ( World * world );
   World * get_world ( void ) { return world; }
   Scope * get_scope ( void ) { return scope; }
   void add_method (  Value::TYPE t, int num_args, const char * name, EXTERNAL_CCLI_FUNCTION f );

   std::string name ( void ) const;

 private:

   std::string pathname;
   Scope * scope;
   Program * world_update_program;
   World * world;
   int argc;
   char ** argv;

};

#endif
