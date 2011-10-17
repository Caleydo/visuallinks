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
#include <QBitmap>
#include <QImage>
#include <QTimer>

namespace qtfullscreensystem
{

  class Application: public QApplication
  {
    Q_OBJECT

    public:

      Application(int& argc, char *argv[]);
      virtual ~Application();

    private:

      QTimer    _timer;
      GLWidget  _gl_widget;

      /** The last screenshot of the whole desktop */
      QImage _last_screenshot;

      /** The mask for the parts of the overlay to be visible */
      QBitmap _mask;

    private slots:

      void timeOut();

  };

} // namespace qtfullscreensystem

#endif /* _APPLICATION_HPP_ */
