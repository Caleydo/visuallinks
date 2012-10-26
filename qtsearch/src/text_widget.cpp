/*!
 * @file text_widget.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 15.02.2012
 */

#include "text_widget.hpp"
#include "JSONParser.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDebug>
#include <QLineEdit>

//------------------------------------------------------------------------------
TextWidget::TextWidget(QWidget *parent):
  QWidget( parent ),
  _edit( new QComboBox(this) ),
  _button( new QPushButton(this) ),
  _menu( new QMenu(this) ),
  _socket( new QWsSocket(this) )
{
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0,0,0,0);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
  setMinimumWidth(256);

  _edit->setEditable(true);
  layout->addWidget(_edit, 1);

  _button->setIcon(QIcon("icon-16.png"));
  _button->setIconSize(QSize(16,16));
  layout->addWidget(_button);

  QPushButton* button_dropdown = new QPushButton(this);
  button_dropdown->setMenu(_menu);
  button_dropdown->setFixedWidth(24);
  layout->addWidget(button_dropdown);

  setFixedHeight( layout->minimumSize().height() );

  connect(_edit->lineEdit(), SIGNAL(returnPressed()),
             this, SLOT(triggerSearch()));
  connect(_button, SIGNAL(pressed()),
             this, SLOT(triggerSearch()));
  connect(_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
             this, SLOT(stateChanged(QAbstractSocket::SocketState)) );
  connect(_socket, SIGNAL(frameReceived(QString)),
             this, SLOT(onTextReceived(QString)));
}

//------------------------------------------------------------------------------
TextWidget::~TextWidget()
{

}

//------------------------------------------------------------------------------
void TextWidget::onStatusChange(QString status)
{

}

//------------------------------------------------------------------------------
void TextWidget::triggerSearch()
{
  switch( _socket->state() )
  {
    case QWsSocket::ConnectedState:
    {
      _socket->write("{"
        "\"task\": \"INITIATE\","
        "\"id\": \"" + _edit->currentText().replace('"', "\\\"") + "\","
        "\"stamp\": 0"
      "}");
      QAction* action = new QAction(_edit->currentText(), this);
      connect(action, SIGNAL(triggered()), this, SLOT(abortRoute()));
      _actions.insert(action);
      _menu->addAction(action);
      break;
    }
    default:
      _socket->connectToHost(QHostAddress::LocalHost, 4487);
      break;
  }
}

//------------------------------------------------------------------------------
void TextWidget::stateChanged(QAbstractSocket::SocketState state)
{
  qDebug() << "State = " << state;
  switch( _socket->state() )
  {
    case QWsSocket::ConnectedState:
      _button->setIcon(QIcon("icon-active-16.png"));
      _socket->write(QString("{"
        "\"task\": \"GET\","
        "\"id\": \"/search-history\""
      "}"));
      break;
//    case QWsSocket::UnconnectedState:
//      _button->setIcon(QIcon("icon-error-16.png"));
//      break;
    default:
      _button->setIcon(QIcon("icon-error-16.png"));
      for(auto it = _actions.begin(); it != _actions.end(); ++it)
        _menu->removeAction(*it);
      _actions.clear();
      break;
  }
}

//------------------------------------------------------------------------------
void TextWidget::onTextReceived(QString data)
{
  qDebug() << "Received: " << data;

  try
  {
    JSONParser msg(data);
    QString task = msg.getValue<QString>("task");

    if( task == "GET-FOUND" )
    {
      QString id = msg.getValue<QString>("id"),
              val = msg.getValue<QString>("val");

      QStringList values = val.split(',', QString::SkipEmptyParts);

      while( _edit->count() )
        _edit->removeItem(0);
      _edit->addItems(values);
    }
  }
  catch(std::runtime_error& ex)
  {
    qDebug() << "Failed to handle message: " << ex.what();
  }
}

//------------------------------------------------------------------------------
void TextWidget::abortRoute()
{
  QAction* action = qobject_cast<QAction*>(sender());
  _menu->removeAction(action);
  _actions.erase(action);

  qDebug() << "Abort for " << action->text() << " requested...";

  _socket->write("{"
    "\"task\": \"ABORT\","
    "\"id\": \"" + action->text().replace('"', "\\\"") + "\","
    "\"stamp\": 0"
  "}");
}
