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

#ifndef YEAST_H
#define YEAST_H

#include "Micro.h"

class Yeast : public Cell {

 public:
  Yeast ( World * w, float x, float y, float a, float v, bool p );
  void render ( Theme * theme, GroPainter * painter  );
  void update ( void );
  Yeast * divide ( void );
  float radius ( void ) { return 0.62035 * pow ( volume, 0.3333333 ); }
  void inc_volume ( float dv ) { volume += dv; }
  void set_bud ( bool p ) { is_bud = p; }
  float get_volume ( void ) { return volume; }
  inline float rad_to_vol ( float r ) { return 4.18879 * r*r*r; }
  void set_parent_cell ( Yeast * p ) { parent_cell = p; }
  float get_fluorescence ( int i ) { return (float) get_rep(i) / volume; }
  float get_size ( void ) { return volume; }

 private:
  float volume;
  Yeast * bud, * parent_cell;
  bool is_bud;
  cpConstraint * cord;

};

#endif // YEAST_H
