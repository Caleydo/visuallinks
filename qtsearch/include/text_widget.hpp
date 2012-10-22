/*!
 * @file text_widget.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 15.02.2012
 */

#ifndef _TEXT_WIDGET_HPP_
#define _TEXT_WIDGET_HPP_

#include <QComboBox>
#include <QPushButton>
#include <QWidget>
#include <QWsSocket.h>

class TextWidget: public QWidget
{
  Q_OBJECT

  public:

    TextWidget(QWidget *parent = 0);
    virtual ~TextWidget();

  public slots:
    void onStatusChange(QString status);

  private slots:

    void triggerSearch();

  private:

    QComboBox      *_edit;
    QPushButton    *_button;
    QWsSocket      *_socket;
};

#endif /* _TEXT_WIDGET_HPP_ */
