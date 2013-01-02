/*!
 * @file application.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 14.10.2011
 */

#include "application.hpp"

#include <QDesktopWidget>
#include <QElapsedTimer>
#include <iostream>

namespace qtfullscreensystem
{

  //----------------------------------------------------------------------------
  Application::Application(int& argc, char *argv[]):
    QApplication(argc, argv),
    _gl_widget(argc, argv)
  {
	_gl_widget.show();
	_gl_widget.startRender();
  }

  //----------------------------------------------------------------------------
  Application::~Application()
  {

  }

} // namespace qtfullscreensystem
