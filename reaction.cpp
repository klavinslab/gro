/////////////////////////////////////////////////////////////////////////////////////////
//
// gro is protected by the UW OPEN SOURCE LICENSE, which is summaraized here.
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

Reaction::Reaction( float k ) : rate(k) {
}

void Reaction::add_reactant ( int i ) {

    reactants.push_back ( i );
    printf ( "added reactant %d\n", i );

}

void Reaction::add_product ( int i ) {

    products.push_back ( i );
    printf ( "added product %d\n", i );

}

void Reaction::integrate ( std::vector<Signal *> * signal_list, int i, int j, float dt ) {

    float v = rate;

    for ( int k = 0; k < reactants.size(); k++ ) {
        v *= (*signal_list)[reactants[k]]->get(i,j);
    }

    for ( int k = 0; k < reactants.size(); k++ ) {
        (*signal_list)[reactants[k]]->dec ( i, j, v * dt );
    }

    for ( int k = 0; k < products.size(); k++ ) {
        (*signal_list)[products[k]]->inc ( i, j, v * dt );
    }

}
