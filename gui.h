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

#ifndef GUI_H
#define GUI_H

#include <QMainWindow>
#include <QPixmap>
#include <QWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QDir>

#include "GroWidget.h"

namespace Ui {
class Gui;
}

class Gui : public QMainWindow
{
    Q_OBJECT
    
public:

    explicit Gui(int ac, char **av, QWidget *parent = 0);
    void updateActionStates(void);
    ~Gui();

public slots:

    void startStop ( void );
    void open ( void );
    void step ( void );
    void reload ( void );
    void displayMessage ( QString msg, bool clear = false );
    void help(void);
    void about(void);
    void snapshot(void);
    void moreCells(void) { growidget.moreCells(); }
    void zoom_in(void);
    void zoom_out(void);
    void reset_zoom(void);
    void dump(void);

private:

    Ui::Gui *ui;
    QSplitter splitter;
    GroWidget growidget;
    QTextEdit console;
    QDir directory;
    QString fileName;
    float zoom;

};

#endif // GUI_H
