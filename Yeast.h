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
