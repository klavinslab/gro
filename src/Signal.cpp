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

Signal::Signal  ( cpVect p, cpVect q, int nx, int ny, float kdi, float kde ) 
      : minp ( p ), maxp ( q ), numx ( nx ), numy ( ny ), kdiff ( kdi ), kdeg ( kde ) {

  int i, j;

  sig.resize ( nx );
  dsig.resize ( nx );

  for ( i=0; i<nx; i++ ) {
    sig[i].resize(ny);
    dsig[i].resize(ny);
  }

  for ( i=0; i<nx; i++ ) 
    for ( j=0; j<ny; j++ ) {
      sig[i][j] = 0.0;
      dsig[i][j] = 0.0;
    }

  w = q.x - p.x;
  h = q.y - p.y;

  dw = w/nx;
  dh = h/ny;

}

Signal::~Signal ( void ) {

}

//void Signal::render ( GroPainter * painter, int index ) {

//  int i, j;

//  QColor col;

//  for ( i=0; i<numx; i++ ) {
//    for ( j=0; j<numy; j++ ) {
//      double a = sig[i][j];
//      double b = sig[i][j];
//      col.setRgbF( qMin(1.0,a), 0.0, qMin(1.0,b), 0.5 );
//      painter->fillRect((int)minp.x+dw*i,(int)minp.y+dh*j,(int)dw,(int)dh,col);
//    }
//  }

//}

void Signal::integrate ( float dt ) {

  int i, j, k, l;

  for ( i=1; i<numx-1; i++ ) {
    for ( j=1; j<numy-1; j++ ) { 
      dsig[i][j] = - 6*kdiff*sig[i][j] - kdeg*sig[i][j];
      dsig[i][j] += kdiff * ( 
          0.5*sig[i+1][j-1] + sig[i+1][j] + 0.5*sig[i+1][j+1] 
        +     sig[i][j-1]                 +     sig[i][j+1]
        + 0.5*sig[i-1][j-1] + sig[i-1][j] + 0.5*sig[i-1][j+1] );
    }
  }

  for ( i=0; i<numx; i++ ) {
    for ( j=0; j<numy; j++ ) {
      sig[i][j] += dt * dsig[i][j];
      if ( sig[i][j] < 0.0 )
	sig[i][j] = 0.0; // just in case
    }
  }

}

void Signal::zero ( void ) {

  int i, j;

  for ( i=0; i<numx; i++ ) 
    for ( j=0; j<numy; j++ ) 
      sig[i][j] = 0.0;

}

void Signal::set ( int i, int j, float c ) { 

  if ( i >=0 && i<numx && j >=0 && j < numy )
    sig[i][j] = c; 

}

void Signal::set ( float x, float y, float c ) { 

  int i = row(x), j = col(y);

  if ( i >=0 && i<numx && j >=0 && j < numy )
    sig[i][j] = c; 

}

void Signal::inc ( int i, int j, float c ) { 

  if ( i > 1 && i<numx-1 && j > 1 && j < numy-1 )
    sig[i][j] += c; 

}

void Signal::inc ( float x, float y, float c ) { 

  int i = row(x), j = col(y);

  if ( i > 1 && i<numx-1 && j > 1 && j < numy-1 )
    sig[i][j] += c; 

}

void Signal::dec ( int i, int j, float c ) {

  if ( i >=0 && i<numx && j >=0 && j < numy )
    sig[i][j] = max ( 0.0, sig[i][j] - c ); 

}

void Signal::dec ( float x, float y, float c ) {

  int i = row(x), j = col(y);

  if ( i >=0 && i<numx && j >=0 && j < numy )
    sig[i][j] = max ( 0.0, sig[i][j] - c ); 

}

float Signal::get ( int i, int j ) {

    if ( i >=0 && i<numx && j >=0 && j < numy )
        return sig[i][j];
    else
        return 0.0f;

}

float Signal::get ( float x, float y ) {

  int i = row(x), j = col(y);

  if ( i >=0 && i<numx && j >=0 && j < numy )
    return sig[i][j];
  else
    return 0.0f;

}

void Signal::set_rect ( float x1, float y1, float x2, float y2, float c ) {

    int X1 = row(x1), Y1 = col(y1),
        X2 = row(x2), Y2 = col(y2);

    for ( int i = X1; i<=X2; i++ )
        for ( int j = Y1; j<Y2; j++ )
            set(i,j,c);

}
