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

#include <QtGui>
#include "GroThread.h"

#define RESIZE                                    \
  middle = QSize(size.width()/2,size.height()/2); \
  delete image;                                   \
  image = new QImage(size, QImage::Format_RGB32);

#define RENDER if ( world ) {             \
  GroPainter painter(size,image);         \
  world->render ( &painter );             \
  emit renderedImage(*image); }

#define CHANGE_STATE(__state__) state = __state__; emit stateChange();

GroThread::GroThread(QObject *parent)
    : QThread(parent),
      state(NO_PROGRAM),
      size(800,800),
      zoom(1.0),
      middle(400,400),
      resized(false),
      world(NULL)
{
    abort = false;
    image = new QImage(size, QImage::Format_RGB32);
}

GroThread::~GroThread()
{

    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();

    delete image;
    //delete world;

}

void GroThread::start_interpreter(void)
{
    QMutexLocker locker(&mutex);

    if ( world != NULL && state == READY ) {

        CHANGE_STATE(RUNNING);

        if ( !isRunning() ) {
            //start ( TimeCriticalPriority ); // this worked for mac, but made things slow on win
            start();
            abort = false;
        } else {
            condition.wakeOne();
        }
    }

}

void GroThread::stop_interpreter(void)
{
    QMutexLocker locker(&mutex);
    abort = true;
}

void GroThread::run()
{

    timer.start();

    forever {

        if ( abort || world->get_stop_flag() ) {
            mutex.lock();
            CHANGE_STATE(READY);
            mutex.unlock();
            RENDER;
            return;
        }     

        try {
            world->update();
        }

        catch ( std::string err ) {
            mutex.lock();
                error_string = err;
                CHANGE_STATE(DEAD);
            mutex.unlock();
            return;
        }

        if (resized) {

            mutex.lock();
            resized = false;
            RESIZE;
            mutex.unlock();

        }

        if ( timer.elapsed() > 33 ) {

            RENDER;
            timer.start();

        }

        fflush(stdout);

    }

}

bool GroThread::parse ( const char * path ) {

    //if ( world )
    //    delete world;

    world = new World(this);
    register_gro_functions();

    current_program = new gro_Program ( path, 0, NULL ); // creates a program object
    world->set_program ( current_program );              // sets the world's program pointer

    try {
        world->init();       // sets up chipmunk and attempts to parse the program
    }

    catch(std::string err) {  // checks for parse errors

        error_string = err;
        delete world;
        world = NULL;
        CHANGE_STATE(NO_PROGRAM);
        return false;

    }

    CHANGE_STATE(READY);      // parsing successful!
    RESIZE;
    RENDER;

    return true;

}

void GroThread::resize ( QSize s ) {

    QMutexLocker locker(&mutex);

    resized = true;
    size = s;

    if ( world && !isRunning() ) {
        RESIZE;
        RENDER;
    }

}

void GroThread::step ( void ) {

    if ( world != NULL && state == READY ) {

        try {
            world->update();
        }

        catch ( std::string err ) {
            mutex.lock();
                error_string = err;
                CHANGE_STATE(DEAD);
            mutex.unlock();
            return;
        }

        RESIZE;
        RENDER;

    }

}

bool GroThread::snapshot ( QString pathname ) {

    return image->save(pathname);

}

void GroThread::moreCells ( void ) {

  if ( has_valid_world() ) {

      world->set_param ( "population_max", world->get_param ( "population_max" ) + 100 );

  }

}

void GroThread::select ( QPoint a, QPoint b ) {

    if ( has_valid_world() ) {
        world->deselect_all_cells();
        world->select_cells(a.x()-middle.width(),a.y()-middle.height(),b.x()-middle.width(),b.y()-middle.height());
        if ( !isRunning() ) {
            RENDER;
        }
    }

}

void GroThread::set_zoom ( float z ) {

    QMutexLocker locker(&mutex);

    resized = true;
    zoom = z;

    if ( world ) world->set_zoom ( z );

    if ( world && !isRunning() ) {
        RESIZE;
        RENDER;
    }

}

void GroThread::dump ( QString filename ) {

    QMutexLocker locker(&mutex);

    if ( has_valid_world() ) {
        FILE * fp = fopen ( filename.toLatin1(), "w" );
        if ( fp ) {
          world->dump(fp);
        }
        fclose ( fp );
    }

}
