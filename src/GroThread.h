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

#ifndef GROTHREAD_H
#define GROTHREAD_H

#include "Micro.h"
#include "Programs.h"

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
 #include <QElapsedTimer>

QT_BEGIN_NAMESPACE
class QImage;
QT_END_NAMESPACE

class GroThread : public QThread
{
    Q_OBJECT

public:

    typedef enum {
        NO_PROGRAM,
        READY,
        RUNNING,
        DEAD
    } STATE;

    GroThread(QObject *parent);
    ~GroThread();

    bool parse ( const char * path );
    void start_interpreter ( void );
    void stop_interpreter ( void );
    void step ( void );
    void resize ( QSize s );
    inline bool has_valid_world ( void ) { return ( world != NULL ); }
    std::string get_error ( void ) { return error_string; }
    inline STATE get_state ( void ) { return state; }
    bool snapshot ( QString pathname );
    void set_zoom ( float z );
    void dump ( QString filename );

#ifndef NOGUI
    inline Theme * theme ( void ) { return has_valid_world() ? world->get_theme() : NULL; }
#endif

    QSize get_middle ( void ) { return middle; }
    void emit_message ( std::string msg, bool clear = false ) { emit message ( msg, clear ); }

#ifndef NOGUI
    void select ( QPoint a, QPoint b );
#endif

    void moreCells ( void );

signals:

    void renderedImage(const QImage &image);
    void stateChange(void);
    void message(std::string,bool);

protected:

    void run();

private:

    // thread stuff
    QMutex mutex;
    QWaitCondition condition;
    QSize resultSize;
    bool abort;
    STATE state;
    QElapsedTimer timer;

    // rendering stuff
    QSize size, middle;
    QImage * image;
    bool resized;
    float zoom;

    // gro stuff
    MicroProgram * current_program;
    World * world;
    std::string error_string;

};

#endif // GROTHREAD_H
