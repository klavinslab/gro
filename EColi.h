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

#ifndef _ECOLI_H_
#define _ECOLI_H_

#include "Micro.h"

class EColi : public Cell {

 public:
  EColi ( World * w, float x, float y, float a, float v );

#ifndef NOGUI
  void render ( Theme * theme, GroPainter * painter );
#endif

  void update ( void );
  Value * eval ( Expr * e );
  EColi * divide ( void );

  void compute_parameter_derivatives ( void );

  float get_fluorescence ( int i ) { return (float) get_rep(i) / volume; }
  float get_length ( void ) { return volume / ( 0.25 * PI * DEFAULT_ECOLI_DIAMETER * DEFAULT_ECOLI_DIAMETER ); }

  float get_volume ( void ) { 
    return volume;
  }

  void force_divide ( void ) { force_div = true; }
  
 private:
  float volume, lambda, div_vol;
  int div_count;
  bool force_div;

};

#endif
