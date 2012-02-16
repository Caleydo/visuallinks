/*!
 * @file text_widget.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 15.02.2012
 */

#ifndef _TEXT_WIDGET_HPP_
#define _TEXT_WIDGET_HPP_

#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class TextWidget: public QWidget
{
  Q_OBJECT

  public:

    TextWidget(QWidget *parent = 0);
    virtual ~TextWidget();

  private slots:

    void triggerSearch();

  private:

    QLineEdit*      _edit;
    QPushButton*    _button;
};

#endif /* _TEXT_WIDGET_HPP_ */
