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
#include <QMenu>
#include <QPushButton>
#include <QWidget>

#include <QWebSocket>

#include <set>

class TextWidget: public QWidget
{
  Q_OBJECT

  public:

    TextWidget(QWidget *parent = 0);
    virtual ~TextWidget();

  public slots:
    void onStatusChange(QString status);

  protected slots:

    void triggerSearch();
    void stateChanged(QAbstractSocket::SocketState state);
    void onTextReceived(QString data);
    void abortRoute();

  private:

    QComboBox      *_edit;
    QPushButton    *_button;
    QMenu          *_menu;
    QWebSocket     *_socket;

    std::set<QAction*> _actions;

    void sendMessage(QJsonObject const& msg);
    QJsonArray jsonBoundingBox() const;
};

#endif /* _TEXT_WIDGET_HPP_ */
