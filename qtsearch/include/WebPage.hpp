/*
 * WebPage.hpp
 *
 *  Created on: 21.10.2012
 *      Author: tom
 */

#ifndef WEBPAGE_HPP_
#define WEBPAGE_HPP_

#include <QWebPage>

class WebPage:
  public QWebPage
{
    Q_OBJECT
  public:
    WebPage(QObject* parent = 0):
      QWebPage(parent)
    {}

  protected:
    void javaScriptConsoleMessage( const QString& message,
                                   int lineNumber,
                                   const QString& sourceID )
    {
      qDebug() << sourceID << "line #" << QString::number(lineNumber)
               << ": " << message;
    }
};

#endif /* WEBPAGE_HPP_ */
