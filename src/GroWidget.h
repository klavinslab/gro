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

#ifndef GROWIDGET_H
#define GROWIDGET_H

#include <QPixmap>
#include <QWidget>

#include "GroThread.h"

class GroWidget : public QWidget
{
    Q_OBJECT

public:

    explicit GroWidget(int ac, char ** av, QWidget *parent = 0);
    ~GroWidget();

    void startStop(void);
    void step(void) { grothread.step(); }
    void open ( QString path );
    void showCenteredMessage ( QString msg );
    void snapshot ( QString path );
    void moreCells ( void );
    void zoom ( float z ) { grothread.set_zoom ( z ); }

    QSize sizeHint ( void ) { return QSize(800,800); }
    QSize minimumSizeHint ( void )  { return QSize(100,100); }
    GroThread::STATE getThreadState ( void ) { return grothread.get_state(); }
    void dump ( QString filename );

signals:

    void messageFromGro ( QString msg, bool clear = false );

private slots:

    void updatePixmap(const QImage &image);
    void handleStateChange ( void );
    void passMessage(std::string msg, bool clear = false);

protected:

    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent * );
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);

private:

    GroThread grothread;
    QPixmap pixmap;
    QString status_string;
    QPoint down, up, current;
    bool is_down, error_flag;
    QImage error_image;
};

#endif // GROWIDGET_H
