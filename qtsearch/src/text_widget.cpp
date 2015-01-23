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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QxtGui/qxtwindowsystem.h>

QJsonValue to_json(const QPoint& p)
{
  QJsonArray a;
  a.append(p.x());
  a.append(p.y());
  return a;
}

QJsonValue to_json(const QSize& size)
{
  return to_json(QPoint(size.width(), size.height()));
}

QJsonValue to_json(const QRect& r)
{
  QJsonArray a;
  a.append(r.x());
  a.append(r.y());
  a.append(r.width());
  a.append(r.height());
  return a;
}

//------------------------------------------------------------------------------
TextWidget::TextWidget(QWidget *parent):
  QWidget( parent ),
  _edit( new QComboBox(this) ),
  _button( new QPushButton(this) ),
  _menu( new QMenu(this) ),
  _socket( new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this) )
{
  setWindowTitle("Concept: Empty");
  setWindowIconText("VisLink - Icon text");

  QxtWindowSystem::setWindowProperty(
    winId(),
    "LINKS_SYSTEM_TYPE",
    "CONCEPT_NODE"
  );

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

  connect(_edit->lineEdit(), &QLineEdit::returnPressed,
             this, &TextWidget::triggerSearch );
  connect(_button, &QPushButton::pressed, this, &TextWidget::triggerSearch);
  connect(_socket, &QWebSocket::stateChanged, this, &TextWidget::stateChanged);
  connect(_socket, &QWebSocket::textMessageReceived,
             this, &TextWidget::onTextReceived );
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
    case QAbstractSocket::ConnectedState:
    {
      // Remove active link TODO check if owner (started link)
      while( !_actions.empty() )
        (*_actions.begin())->trigger();

      QString id = _edit->currentText().simplified();
      if( id.isEmpty() )
        return;

      QJsonObject msg;
      msg["task"] = "INITIATE";
      msg["id"] = id;
      msg["stamp"] = 0;
      //msg["regions"] = jsonBoundingBox();

      sendMessage(msg);
      setWindowTitle("Concept: " + id);

      QAction* action = new QAction(_edit->currentText(), this);
      action->setIcon( QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton) );
      connect(action, SIGNAL(triggered()), this, SLOT(abortRoute()));
      _actions.insert(action);
      _menu->addAction(action);
      break;
    }
    default:
      _socket->open(QUrl("ws://localhost:4487"));
      break;
  }
}

//------------------------------------------------------------------------------
void TextWidget::stateChanged(QAbstractSocket::SocketState state)
{
  qDebug() << "State = " << state;
  switch( _socket->state() )
  {
    case QAbstractSocket::ConnectingState:
      break;
    case QAbstractSocket::ConnectedState:
    {
      _button->setIcon(QIcon("icon-active-16.png"));

      // register
      {
        QJsonObject msg;
        msg["task"] = "REGISTER";
        msg["pos"] = to_json(pos() + QPoint(10, 10));

        QRect const geom = QxtWindowSystem::windowGeometry(winId());
        msg["viewport"] = to_json(QRect(0, 0, geom.width(), geom.height()));
        sendMessage(msg);
      }

      // get search history (for autocompletion)
//      {
//        QJsonObject msg;
//        msg["task"] = "GET";
//        msg["id"] = "/search-history";
//        sendMessage(msg);
//      }

      triggerSearch();
      break;
    }
//    case QAbstractSocket::UnconnectedState:
//      _button->setIcon(QIcon("icon-error-16.png"));
//      break;
    default:
    {
      _button->setIcon(QIcon("icon-error-16.png"));
      for(auto it = _actions.begin(); it != _actions.end(); ++it)
        _menu->removeAction(*it);
      _actions.clear();
      break;
    }
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

      // TODO (search history)
//      QStringList values = val.split(',', QString::SkipEmptyParts);
//
//      while( _edit->count() )
//        _edit->removeItem(0);
//      _edit->addItems(values);
    }
    else if( task == "REQUEST" )
    {
      QString id = msg.getValue<QString>("id");
      if( _edit->currentText().simplified()
                              .compare(id, Qt::CaseInsensitive) != 0 )
        return;

      QJsonObject msg_ret;
      msg_ret["task"] = "FOUND";
      msg_ret["id"] = id;
      msg_ret["stamp"] = qint64(msg.getValue<uint32_t>("stamp"));
      msg_ret["regions"] = jsonBoundingBox();

      qDebug() << "respond" << msg_ret;

      sendMessage(msg_ret);
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

  QJsonObject msg;
  msg["task"] = "ABORT";
  msg["id"] = action->text();
  msg["stamp"] = 0;
  sendMessage(msg);
}

//------------------------------------------------------------------------------
void TextWidget::sendMessage(QJsonObject const& msg)
{
  _socket->sendTextMessage( QJsonDocument(msg).toJson(QJsonDocument::Compact) );
}

//------------------------------------------------------------------------------
QJsonArray TextWidget::jsonBoundingBox() const
{
  QRect const geom = QxtWindowSystem::windowGeometry(winId());

  QJsonArray bb;
  bb.append( to_json(QPoint(1,                1)) );
  bb.append( to_json(QPoint(geom.width() - 1, 1)) );
  bb.append( to_json(QPoint(geom.width() - 1, geom.height() - 1)) );
  bb.append( to_json(QPoint(1,                geom.height() - 1)) );

  QJsonObject props;
  props["rel"] = true;
  props["type"] = "window-outline";
  bb.append(props);

  QJsonArray regs;
  regs.append(bb);

  return regs;
}
