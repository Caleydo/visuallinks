/*!
 * @file application.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 14.10.2011
 */

#ifndef _APPLICATION_HPP_
#define _APPLICATION_HPP_

#include "qglwidget.hpp"

#include <QApplication>
#include <QTimer>

namespace qtfullscreensystem
{

  class Application: public QApplication
  {
    Q_OBJECT

    public:

      Application(int& argc, char *argv[]);
      virtual ~Application();

      virtual bool notify(QObject* receiver, QEvent* e);

    private:

      QTimer    _timer;
      GLWidget  _gl_widget;

  };

} // namespace qtfullscreensystem

#endif /* _APPLICATION_HPP_ */
