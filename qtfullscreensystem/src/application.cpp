/*!
 * @file application.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 14.10.2011
 */

#include "application.hpp"
#include <iostream>

namespace qtfullscreensystem
{

  //----------------------------------------------------------------------------
  Application::Application(int argc, char *argv[]):
    QApplication(argc, argv)
  {
    //    GLWidget w;
    //
    //    w.setGeometry(QApplication::desktop()->screenGeometry(&w));
    //    //w.setFixedSize(w.size());
    //    w.setWindowFlags( Qt::WindowStaysOnTopHint
    //                    | Qt::FramelessWindowHint
    //                    | Qt::MSWindowsOwnDC
    //                    | Qt::X11BypassWindowManagerHint
    //                    );
    //
    //    w.setRenderMask();
    //    QBitmap mask = w.renderPixmap().createMaskFromColor( QColor(0,0,0) );
    //    w.clearRenderMask();
    //    mask.save("mask.png");
    //    w.setMask(mask);
    //
    //    w.show();

        /*int i = 0;
        while(true) {
            QPixmap bla = QPixmap::grabWindow(QApplication::desktop()->winId());
            cout << "test " << i++ << endl;
        }*/


    connect(&_timer, SIGNAL(timeout()), this, SLOT(timeOut()));
    _timer.start(100);
  }

  //----------------------------------------------------------------------------
  Application::~Application()
  {

  }

  //----------------------------------------------------------------------------
  void Application::timeOut()
  {
    std::cout << "to" << std::endl;
  }

} // namespace qtfullscreensystem
