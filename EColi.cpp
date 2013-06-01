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

#include "EColi.h"
#include "Programs.h"

#define FMULT 0.125

void EColi::compute_parameter_derivatives ( void ) {

  lambda = sqrt ( 10 * get_param ( "ecoli_growth_rate" ) * get_param ( "ecoli_growth_rate" )  / get_param ( "ecoli_division_size_variance" ) );
  div_vol = get_param ( "ecoli_division_size_mean" ) - get_param ( "ecoli_growth_rate" ) * 10 / lambda;

}

EColi::EColi ( World * w, float x, float y, float a, float v ) : Cell ( w ), volume ( v ) {

  compute_parameter_derivatives();

  float size = DEFAULT_ECOLI_SCALE*get_length();
  MAKE_VERTS;

  body = cpSpaceAddBody(space, cpBodyNew(MASS, cpMomentForPoly(MASS, 8, verts, cpvzero)));

  body->p = cpv ( x, y );
  body->v = cpv ( 0, 0 );
  body->a = a;
        
  shape = cpSpaceAddShape(space, cpPolyShapeNew(body, 8, verts, cpvzero)); // deleted in ~Cell
  shape->e = ELASTIC; shape->u = FRICTION;

  //cpSpaceActivateBody(space,body);

  int i;
  for ( i=0; i<MAX_STATE_NUM; i++ ) q[i] = 0;
  for ( i=0; i<MAX_REP_NUM; i++ ) rep[i] = 0;

  div_count = 0;
  force_div = false;

}

#ifndef NOGUI

void EColi::render ( Theme * theme, GroPainter * painter ) {

  cpPolyShape * poly = (cpPolyShape *) shape;
  int count = poly->numVerts;

  double
    gfp = ( rep[GFP] / volume - world->get_param ( "gfp_saturation_min" ) ) / ( world->get_param ( "gfp_saturation_max" ) - world->get_param ( "gfp_saturation_min" ) ),
    rfp = ( rep[RFP] / volume - world->get_param ( "rfp_saturation_min" ) ) / ( world->get_param ( "rfp_saturation_max" ) - world->get_param ( "rfp_saturation_min" ) ),
    yfp = ( rep[YFP] / volume - world->get_param ( "yfp_saturation_min" ) ) / ( world->get_param ( "yfp_saturation_max" ) - world->get_param ( "yfp_saturation_min" ) ),
    cfp = ( rep[CFP] / volume - world->get_param ( "cfp_saturation_min" ) ) / ( world->get_param ( "cfp_saturation_max" ) - world->get_param ( "cfp_saturation_min" ) );

  theme->apply_ecoli_edge_color ( painter, is_selected() );

  QColor col;

  col.setRgbF( qMin(1.0,rfp + yfp),
               qMin(1.0,gfp + yfp + cfp),
               qMin(1.0,cfp),
               0.75);

  painter->setBrush(col);

  QPainterPath path;
  cpVect v = poly->tVerts[0];
  path.moveTo(v.x,v.y);

  for ( int i=1; i<count; i++ ) {
      v = poly->tVerts[i];
      path.lineTo(v.x,v.y);
  }

  path.closeSubpath();
  painter->drawPath(path);

}
#endif

void EColi::update ( void ) {

  volume += get_param ( "ecoli_growth_rate" ) * rand_exponential ( 1 / world->get_sim_dt() ) * volume;

  if ( volume > div_vol && frand() < lambda * world->get_sim_dt() )
    div_count++;

  float size = DEFAULT_ECOLI_SCALE*get_length();
  MAKE_VERTS;
  cpPolyShapeSetVerts ( shape, 8, verts, cpvzero );

  if ( program != NULL )
    program->update ( world, this );

}

Value * EColi::eval ( Expr * e ) {

  if ( program != NULL )
    program->eval ( world, this, e );

}

EColi * EColi::divide ( void ) {

  if ( div_count >= 10 || force_div ) {

    int r = frand() > 0.5 ? 1 : -1;

    div_count = 0;
    force_div = false;

    float frac = 0.5 + 0.1 * ( frand() - 0.5 );
    float oldvol = volume;
    float oldsize = DEFAULT_ECOLI_SCALE * get_length();

    volume = frac * oldvol;
    float a = shape->body->a;
    float da = 0.25 * (frand()-0.5);

    float size = DEFAULT_ECOLI_SCALE * get_length();    
    MAKE_VERTS;

    cpPolyShapeSetVerts ( shape, 8, verts, cpvzero );
    cpVect oldpos = shape->body->p;
    shape->body->p = oldpos + cpvmult ( cpv ( cos ( a - r*da ),
                                              sin ( a - r*da ) ),  (-r)*0.5*oldsize*(1-frac) );
    shape->body->a = a - r*da;

    float dvol = (1-frac)*oldvol;

    EColi * daughter = new EColi ( world, oldpos.x + r*0.5*oldsize*frac*cos ( a + r*da ), 
                                 oldpos.y + r*0.5*oldsize*frac*sin ( a + r*da ), a+r*da, dvol );
 

    daughter->set_param_map ( get_param_map() );
    daughter->compute_parameter_derivatives();

    if ( gro_program != NULL ) {
      daughter->set_gro_program ( split_gro_program ( gro_program, frac ) );
    }

    daughter->init ( q, rep, 1-frac );

    int i;

    for ( i=0; i<MAX_STATE_NUM; i++ ) q[i] = (int) ceil(frac*q[i]);
    for ( i=0; i<MAX_REP_NUM; i++ ) rep[i] = (int) ceil(frac*rep[i]);

    set_division_indicator(true);
    daughter->set_division_indicator(true);
    daughter->set_daughter_indicator(true);

    return daughter;

  } else return NULL;

}
