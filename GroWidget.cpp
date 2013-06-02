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
#include <string>

#include "GroWidget.h"

GroWidget::GroWidget(int ac, char ** av,QWidget *parent) :
    QWidget(parent),
    grothread(this),
    status_string ( tr("Click \"Open\" to open a program. Then click \"Start\"." ) ),
    is_down ( false ),
    error_flag ( false ),
    error_image ( ":/images/error.png" )
{

    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<std::string>("std::string");

    connect(&grothread, SIGNAL(renderedImage(QImage)), this, SLOT(updatePixmap(QImage)));
    connect(&grothread, SIGNAL(stateChange()),this,SLOT(handleStateChange()));
    connect(&grothread, SIGNAL(message(std::string,bool)), this, SLOT(passMessage(std::string,bool)));

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

}

GroWidget::~GroWidget()
{

}

void GroWidget::updatePixmap(const QImage &image) {

    pixmap = QPixmap::fromImage(image);

    if ( pixmap.isNull() ) {
        printf ( "pixmap is null\n" ); fflush ( stdout );
    }

    update();

}

void GroWidget::handleStateChange ( void ) {

    switch ( grothread.get_state() ) {

    case GroThread::NO_PROGRAM:
        emit messageFromGro ( QString() );
        break;

    case  GroThread::READY:
        emit messageFromGro ( QString() );
        break;

    case GroThread::DEAD:
        emit messageFromGro ( QString() );
        break;

    case GroThread::RUNNING:
        emit messageFromGro ( QString() );
        break;

    }

}

void GroWidget::resizeEvent(QResizeEvent *) {

    grothread.resize(size());

}

void GroWidget::paintEvent(QPaintEvent *) {

    QPainter painter(this);

    //painter.scale(1.5,1.5);

    if ( !painter.isActive() ) return;

    if ( error_flag ) {

        int w = width(),
            h = height(),
            iw = error_image.width(),
            ih = error_image.height();
        painter.fillRect(rect(), Qt::white);
        painter.drawImage ( (w-iw)/2, (h-ih)/2, error_image );

    } else if (pixmap.isNull()) {

        painter.fillRect(rect(), Qt::white);
        painter.setPen(Qt::black);

        painter.drawText (
                rect(),
                Qt::AlignCenter,
                status_string
              );

    } else {

        painter.drawPixmap(0,0, pixmap);

    }

    if ( is_down && grothread.has_valid_world() ) {

        grothread.theme()->apply_mouse_color(&painter);
        painter.setBrush ( QBrush() );
        painter.drawRect ( QRect(down,current) );

    }

}

void GroWidget::startStop ( void ) {

    if ( grothread.get_state() == GroThread::READY ) {
        grothread.start_interpreter();
    } else if ( grothread.get_state() == GroThread::RUNNING ) {
        grothread.stop_interpreter();
    }

}

void GroWidget::open ( QString path ) {

    emit messageFromGro ( "", true );

    if ( !grothread.parse(path.toLatin1()) ) {

        emit messageFromGro ( tr("") + grothread.get_error().c_str() + "<br>"
                            + "Please edit the program to fix the error and try opening it again.", true );

        error_flag = true;
        repaint();

    } else {

        //emit messageFromGro ( "Successfully parsed " + path
         //                     + "<br>Click \"Start\" to run it.", false );
        error_flag = false;

    }

}

void GroWidget::dump ( QString filename ) {

  if ( grothread.get_state() == GroThread::READY ) {

      grothread.dump(filename);

  }

}

void GroWidget::moreCells ( void ) {

    grothread.moreCells();

    if ( grothread.get_state() == GroThread::READY )
        grothread.start_interpreter();

}

void GroWidget::passMessage ( std::string msg, bool clear ) {

    emit messageFromGro ( msg.c_str(), clear );

}

void GroWidget::snapshot ( QString path ) {

    if ( grothread.has_valid_world() ) {
        grothread.snapshot(path);
    }

}

void GroWidget::mousePressEvent ( QMouseEvent * e ) {

    down = e->pos();
    up = down;
    current = down;
    is_down = true;

}

void GroWidget::mouseReleaseEvent ( QMouseEvent * e ) {

    up = e->pos();
    is_down = false;
    grothread.select ( up, down );
    repaint();

}

void GroWidget::mouseMoveEvent ( QMouseEvent * e ) {

    current = e->pos();
    repaint();

}
