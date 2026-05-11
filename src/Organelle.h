#ifndef ORGANELLE_H
#define ORGANELLE_H

#include "Cell.h"

#ifndef NOGUI
#include "GroPainter.h"
#endif

class Organelle {

public:

    Organelle ( cpSpace * s, float x, float y, float rad );
    ~Organelle();
    void render ( GroPainter * painter, QColor col );
    void update ( float kdt );

private:

    cpSpace * space;
    cpBody * body;
    cpShape * shape;

};

#endif // ORGANELLE_H
