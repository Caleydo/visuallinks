/*!
 * @file text_widget.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 15.02.2012
 */

#include "text_widget.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QLineEdit>
#include <QDebug>

//------------------------------------------------------------------------------
TextWidget::TextWidget(QWidget *parent):
  QWidget( parent ),
  _edit( new QComboBox(this) ),
  _button( new QPushButton(this) ),
  _socket( new QWsSocket(this) )
{
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0,0,0,0);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  setMinimumSize(256, 128);
  setMaximumWidth(256);

  QStringList wordList;
  wordList << "alpha" << "omega" << "omicron" << "zeta";
  _edit->addItems(wordList);
  _edit->setEditable(true);
  layout->addWidget(_edit);

  _button->setIcon(QIcon("icon-16.png"));
  _button->setIconSize(QSize(16,16));
  layout->addWidget(_button);

  connect(_edit->lineEdit(), SIGNAL(returnPressed()), this, SLOT(triggerSearch()));
  connect(_button, SIGNAL(pressed()), this, SLOT(triggerSearch()));
  connect(_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
             this, SLOT(stateChanged(QAbstractSocket::SocketState)) );
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
    case QWsSocket::UnconnectedState:
      _socket->connectToHost(QHostAddress::LocalHost, 4487);
      break;
    case QWsSocket::ConnectedState:
      _socket->write("{"
        "\"task\": \"INITIATE\","
        "\"id\": \"" + _edit->currentText().replace('"', "\\\"") + "\","
        "\"stamp\": 0"
      "}");
      break;
    default:
      qDebug() << "Unknown socket state: " << _socket->state();
      break;
  }
  //std::cout << "search: " << _edit->currentText().toStdString() << std::endl;
}

//------------------------------------------------------------------------------
void TextWidget::stateChanged(QAbstractSocket::SocketState state)
{
  qDebug() << "State = " << state;
}
