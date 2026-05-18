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

#ifndef _DEFINES_H_
#define _DEFINES_H_

#define MAX_STATE_NUM 10
#define DAMPING 0.001f
#define ITERATIONS 100
#define SLEEP_TICKS 16
#define ELASTIC 0.1f
#define FRICTION 0.0f
#define WIDTH 10.0f
#define THRESHOLD 40.0f
#define MASS 1.0f

// set in world
#define DEFAULT_CHIP_DT 0.005        // chipmunk time units
#define DEFAULT_SIM_DT 0.02              // minutes

// set in ecoli
#define DEFAULT_ECOLI_GROWTH_RATE 0.0346574   // reactions / min
#define DEFAULT_ECOLI_INIT_SIZE 1.57      // fl
#define DEFAULT_ECOLI_DIV_SIZE_MEAN 3.14  // fl
#define DEFAULT_ECOLI_DIV_SIZE_VAR 0.005  // um
#define DEFAULT_ECOLI_DIAMETER 1.0        // um
#define DEFAULT_ECOLI_SCALE 10.0          // pixels / um


#define MAKE_VERTS cpVect verts[] = {   \
    cpv ( -size/2+WIDTH/3, -WIDTH/2 ),  \
    cpv ( -size/2,         -WIDTH/6 ),  \
    cpv ( -size/2,          WIDTH/6 ),  \
    cpv ( -size/2+WIDTH/3,  WIDTH/2),   \
    cpv ( size/2-WIDTH/3,   WIDTH/2 ),  \
    cpv ( size/2,           WIDTH/6 ),  \
    cpv ( size/2,          -WIDTH/6 ),  \
    cpv ( size/2-WIDTH/3,  -WIDTH/2 )   \
  };

static inline cpFloat frand(void) { return (cpFloat)rand()/(cpFloat)RAND_MAX; }

#define DBG {                                             \
    printf ( "Debug statement: reached %s, line %d\n",    \
                    __FILE__, __LINE__);                  \
    fflush ( stdout );                                    \
}

#define ASSERT(_cond_) {                                                          \
  if ( ! ( _cond_ ) ) {                                                           \
    fprintf (stderr, "Internal error: assertion '%s' failed at %s, line %d\n",    \
                   #_cond_ , __FILE__, __LINE__);                                 \
    exit ( -1 );                                                                  \
  }                                                                               \
}

#define ASSERT_MSG(_cond_,_msg_...) {                                             \
  if ( ! ( _cond_ ) ) {                                                           \
    fprintf ( stderr, "Internal error: assertion '%s' failed at %s, line %d\n",   \
                   #_cond_ , __FILE__, __LINE__);                                 \
    fprintf ( stderr, _msg_ );                                                    \
    exit ( -1 );                                                                  \
  }                                                                               \
}

#define PRODUCE(_target_,_rate_) if ( frand() < _rate_ ) _target_++;
#define CONSUME(_target_,_rate_) if ( frand() < _rate_ ) _target_--;

#define BILLION  1000000000L
#define MILLION  1000000L
#define THOUSAND 1000L

////////////////////////////////////////////////////////////////////////////////////////////////
//
// COLORS
//

#define MAX_REP_NUM 4
#define GFP 0
#define RFP 1
#define YFP 2
#define CFP 3

#define BACKGROUND_COLOR 0.0f, 0.0f, 0.0f, 0.0f

#define SATURATION_MAX 50
#define SATURATION_MIN 0

////////////////////////////////////////////////////////////////////////////////////////////////
//
// CHEMOSTAT SHAPE
//

#define CSXL -150.0f
#define CSXR 150.0f
#define CSYB -150.0f
#define CSYT 150.0f

#define CS1 -400.0f, CSYB
#define CS2 CSXL, CSYB
#define CS3 CSXL, CSYT
#define CS4 CSXR, CSYT
#define CS5 CSXR, CSYB
#define CS6 400.0f, CSYB

#endif
