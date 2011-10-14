#include "qglwidget.h"

#include <QApplication>
#include <QBitmap>
#include <QDesktopWidget>
#include <QPixmap>

#include <iostream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GLWidget w;

    w.setGeometry(QApplication::desktop()->screenGeometry(&w));
    //w.setFixedSize(w.size());
    w.setWindowFlags( Qt::WindowStaysOnTopHint
                    | Qt::FramelessWindowHint
                    | Qt::MSWindowsOwnDC
                    | Qt::X11BypassWindowManagerHint
                    );

    QBitmap mask = w.renderPixmap().createMaskFromColor( QColor(0,0,0) );
    mask.save("mask.png");
    w.setMask(mask);

    w.show();

    /*int i = 0;
    while(true) {
        QPixmap bla = QPixmap::grabWindow(QApplication::desktop()->winId());
        cout << "test " << i++ << endl;
    }*/

    return a.exec();
}
