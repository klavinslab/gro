#include "Yeast.h"
#include "Programs.h"

Yeast::Yeast ( World * w, float x, float y, float a, float v, bool b )
: Cell ( w ), volume ( v ), bud ( NULL ), is_bud ( b ) {

  float r = radius();
  cpBody *body = cpSpaceAddBody(space, cpBodyNew(r, cpMomentForCircle(10*v, 0.0f, r, cpvzero)));
  parent_cell = NULL;

  body->p = cpv ( x, y );
  body->v = cpv ( 0, 0 );
  body->a = a;

  shape = cpSpaceAddShape(space, cpCircleShapeNew(body, radius(), cpvzero));
  shape->e = ELASTIC; shape->u = FRICTION; 

  int i;
  for ( i=0; i<MAX_STATE_NUM; i++ ) q[i] = 0;
  for ( i=0; i<MAX_REP_NUM; i++ ) rep[i] = 0;

}

#define YFMULT 500

void Yeast::render ( Theme * theme, GroPainter * painter  ) {

  float r = radius();
  float vol;

  if ( is_bud == false && bud == NULL ) { // normal cell

    vol = volume;

  } else if ( is_bud == false && bud != NULL ) { // cell is budding

    float vol = volume + bud->get_volume();

  } else if ( is_bud ) { // cell is a bud

    float vol = volume + parent_cell->get_volume();

  }

  double
    gfp = 500*( rep[GFP] / vol - world->get_param ( "gfp_saturation_min" ) ) / ( world->get_param ( "gfp_saturation_max" ) - world->get_param ( "gfp_saturation_min" ) ),
    rfp = 500*( rep[RFP] / vol - world->get_param ( "rfp_saturation_min" ) ) / ( world->get_param ( "rfp_saturation_max" ) - world->get_param ( "rfp_saturation_min" ) ),
    yfp = 500*( rep[YFP] / vol - world->get_param ( "yfp_saturation_min" ) ) / ( world->get_param ( "yfp_saturation_max" ) - world->get_param ( "yfp_saturation_min" ) ),
    cfp = 500*( rep[CFP] / vol - world->get_param ( "cfp_saturation_min" ) ) / ( world->get_param ( "cfp_saturation_max" ) - world->get_param ( "cfp_saturation_min" ) );

  theme->apply_ecoli_edge_color ( painter, is_selected() );

  QColor col;

  col.setRgbF( qMax(0.0,qMin(1.0,rfp + yfp)),
               qMax(0.0,qMin(1.0,gfp + yfp + cfp)),
               qMax(0.0,qMin(1.0,cfp)),
               0.75);

  painter->setBrush(col);
  QPointF center (get_x(),get_y());

  painter->drawEllipse(center,r,r);

  if ( bud ) 
    bud->render ( theme,painter );

  return;

}

void Yeast::update ( void ) {

  if ( !is_bud ) {

    if ( !bud ) {
      inc_volume ( 600*get_param("yeast_growth_rate") );
    } else {
      bud->inc_volume ( 600*get_param("yeast_growth_rate") );
    }

    if ( volume > 750*get_param("yeast_division_size_mean") && bud == NULL && frand() < 0.01 ) {
      float theta = 6.28*frand(), r = radius();
      bud = new Yeast ( world, get_x()+(r+1)*sin(theta), get_y()+(r+1)*cos(theta), 0.78, rad_to_vol ( 1.0 ), true );
      bud->set_parent_cell ( this );
      cord = cpDampedSpringNew ( get_shape()->body, bud->get_shape()->body, cpv(0,0), cpv(0,0), r+0.1, 10, 10 );
      cpSpaceAddConstraint ( space, cord );
    }

    if ( program != NULL )
      program->update ( world, this );

  }

  cpCircleShapeSetRadius ( shape, radius() );
  if ( bud ) cpCircleShapeSetRadius ( bud->get_shape(), bud->radius() );

}

Yeast * Yeast::divide ( void ) {

  if ( is_bud == false && bud != NULL ) {

    if ( bud->get_volume() > 5000.0 && frand() > 0.01 ) {

      int i;

      float frac = volume / ( volume + bud->get_volume() );

      for ( i=0; i<MAX_STATE_NUM; i++ ) {
    	float temp = q[i];
        set ( i, (int) floor ( frac*temp ) );
        bud->set ( i, (int) ceil ( (1-frac)*temp ) );
      }

      for ( i=0; i<MAX_REP_NUM; i++ ) {
    	float temp = rep[i];
        set_rep ( i, (int) floor ( frac*temp ) );
        bud->set_rep ( i, (int) ceil ( (1-frac)*temp ) );
      }

      if ( gro_program != NULL ) 
        bud->set_gro_program ( split_gro_program ( gro_program, frac ) );

      bud->set_bud ( false );
      world->add_cell ( bud );
      bud = NULL;
      cpSpaceRemoveConstraint ( space, cord );
      cpConstraintFree ( cord );
      cord = NULL;

    }

  }

  return NULL;

}
