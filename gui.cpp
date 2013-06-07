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

#include <QtGui>
#include <QSizePolicy>
#include <QFileDialog>
#include "gui.h"
#include "ui_gui.h"
#include <unistd.h>

#define GetCurrentDir getcwd

char cCurrentPath[FILENAME_MAX];

Gui::Gui(int ac, char **av, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Gui),
    growidget(ac,av)
{

    ui->setupUi(this);

    console.setReadOnly(true);

    splitter.setOrientation(Qt::Vertical);
    splitter.addWidget(&growidget);
    splitter.addWidget(&console);
    splitter.setSizes(QList<int>() << 775 << 150);

    ui->centralWidget->layout()->addWidget(&splitter);
    //ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    connect ( &growidget, SIGNAL(messageFromGro(QString,bool)),
              this,       SLOT  (displayMessage(QString,bool)));

    about();

    zoom = 1.0;

}

Gui::~Gui()
{
    delete ui;
}

void Gui::startStop ( void ) {

    growidget.startStop();

}

void Gui::help ( void ) {

    system ( "open http://depts.washington.edu/soslab/gro/docview.html" );
}

void Gui::about ( void ) {

    console.clear();
    console.insertHtml ( "<h2>gro: The Cell Programming Language</h2>Version beta.5<br />" );
    console.insertHtml ( "Programmed by Eric Klavins, University of Washington, Seattle, WA, USA<br />" );
    console.insertHtml ( "Copyright &copy; 2011-2012, University of Washington (GNU V. 2)<br />" );
    console.insertHtml ( "See <a href=\"http://depts.washington.edu/soslab/gro/\">http://depts.washington.edu/soslab/gro</a> for more information.<br />" );

    // set directory for C++ functions like fopen
    // NOTE: Here is probably the wrong place for this code.
    chdir("../../..");
    GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));
    cCurrentPath[sizeof(cCurrentPath) - 1] = '\0'; /* not really required */
    char buf[1000];
    sprintf (buf,"Working directory: %s", cCurrentPath);
    console.insertHtml(buf);

}

void Gui::open ( void ) {

    fileName = QFileDialog::getOpenFileName (
                         this,
                         tr("Open a .gro file"),
                         directory.absolutePath(),
                         tr("Gro files (*.gro)")
                       );

    if ( !fileName.isNull() ) {
      growidget.open ( fileName );
      setWindowTitle( QString("gro: ") + fileName );
    } else {
        console.insertHtml ( QString("Error opening ") + fileName + "<br />"  );
    }

     zoom = 1.0;
     growidget.zoom(zoom);
     updateActionStates();

}

void Gui::dump(void) {

    QString dumpfile = QFileDialog::getSaveFileName (
                         this,
                         tr("Save positions, volumes, and reporter values"),
                         directory.absolutePath() + "/untitled.csv",
                         tr("Comma Separated Value files (*.csv)")
                       );

    if ( !dumpfile.isNull() ) {

      console.insertHtml ( QString("Saving colony information to ") + dumpfile + "<br />"  );
      growidget.dump ( dumpfile );

    } else {

        console.insertHtml ( QString("Error saving colony information to ") + dumpfile + "<br />"  );

    }

}

void Gui::step ( void ) {

    growidget.step();

}

void Gui::reload ( void ) {

    if ( !fileName.isNull() )
      growidget.open ( fileName );

    growidget.zoom(zoom);
    updateActionStates();

}

void Gui::snapshot ( void ) {

    QString path = QFileDialog::getSaveFileName(
                this,
                "Save screenshot as ...",
                directory.absolutePath()
                );

    growidget.snapshot ( path );

}

#define SET_ACTION_ENABLED(_open_,_reload_,_startstop_,_step_,_snap_,_more_) { \
    ui->actionOpen->setEnabled(_open_);                          \
    ui->actionReload->setEnabled(_reload_);                      \
    ui->actionStartStop->setEnabled(_startstop_);                \
    ui->actionStep->setEnabled(_step_);                          \
    ui->actionScreenshot->setEnabled(_snap_);                    \
    ui->actionMoreCells->setEnabled(_more_);                     \
    ui->actionZoom_In->setEnabled((_startstop_||_step_)&&zoom<3.5);  \
    ui->actionZoom_Out->setEnabled((_startstop_||_step_)&&zoom>0.15); \
}

void Gui::updateActionStates ( void ) {

    switch ( growidget.getThreadState() ) {

    case GroThread::NO_PROGRAM:
        if ( fileName.isNull() ) {
            SET_ACTION_ENABLED ( true, false, false, false, false, false );
        } else {
            SET_ACTION_ENABLED ( true, true, false, false, false, false );
        }
        break;

    case  GroThread::READY:
        SET_ACTION_ENABLED ( true, true, true, true, true, true );
        ui->actionStartStop->setText("Start");
        ui->actionStartStop->setIcon(QIcon(":/icons/icons/start.png"));
        break;

    case GroThread::DEAD:
        SET_ACTION_ENABLED ( true, true, false, false, false, false );
        ui->actionStartStop->setIcon(QIcon(":/icons/icons/start.png"));
        break;

    case GroThread::RUNNING:
        SET_ACTION_ENABLED ( false, false, true, false, false, true );
        ui->actionStartStop->setText("Stop");
        ui->actionStartStop->setIcon(QIcon(":/icons/icons/stop.png"));
        break;

    }

}

void Gui::displayMessage ( QString msg, bool clear ) {

    if ( clear )
        console.clear();

    if ( !msg.isEmpty() ) {
        console.insertHtml ( msg + "<br />" );
        console.moveCursor(QTextCursor::End);
    }

    updateActionStates();

}

void Gui::zoom_in(void) {

    if ( zoom < 3.5 && zoom >= 0.25 )
        zoom += 0.25;
    else if ( zoom < 0.25 )
       zoom += 0.1;

    growidget.zoom(zoom);
    updateActionStates();

}

void Gui::zoom_out(void) {

    if ( zoom > 0.25 )
        zoom -= 0.25;
    else if ( zoom > 0.15 )
        zoom -= 0.1;

    growidget.zoom(zoom);
    updateActionStates();

}

void Gui::reset_zoom(void) {

    zoom = 1.0;
    growidget.zoom(zoom);
    updateActionStates();

}
