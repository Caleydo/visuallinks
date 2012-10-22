/*!
 * @file text_widget.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 15.02.2012
 */

#include "text_widget.hpp"
#include "wid_convert.h"

#include <QApplication>
#include <QBoxLayout>
#include <QLineEdit>
#include <QWebView>
#include <QDebug>

#include <iostream>
#include <tuple>

class WebPage:
  public QWebPage
{

  public:
    WebPage(QObject* parent = 0):
      QWebPage(parent)
    {}

  protected:
    void javaScriptConsoleMessage( const QString& message,
                                   int lineNumber,
                                   const QString& sourceID )
    {
      qDebug() << sourceID << "line" << QString::number(lineNumber)
               << ": " << message;
    }
};

//------------------------------------------------------------------------------
TextWidget::TextWidget(QWidget *parent):
  QWidget( parent ),
  _edit( new QComboBox(this) ),
  _button( new QPushButton(this) ),
  _web_frame(0)
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

  QWebView *view = new QWebView(parent);
  view->setPage(new WebPage);
//  view->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
//  view->setMinimumSize(256, 128);
  view->load(QUrl("./client.html"));

  _web_frame = view->page()->mainFrame();
  _web_frame->addToJavaScriptWindowObject("vislink_control", this);

  view->show();
  //view->resize( view->minimumSize() );
  layout->addWidget(view);

  //resize(256, 128);

  connect(_edit->lineEdit(), SIGNAL(returnPressed()), this, SLOT(triggerSearch()));
  connect(_button, SIGNAL(pressed()), this, SLOT(triggerSearch()));
}

//------------------------------------------------------------------------------
TextWidget::~TextWidget()
{

}

//------------------------------------------------------------------------------
void TextWidget::onStatusChange(QString status)
{
  qDebug() << (_socket_status = status);
}

//------------------------------------------------------------------------------
void TextWidget::triggerSearch()
{
  QString code;

  if( _socket_status != "active" )
    code = "connect()";
  else
    code = "alert('super')";

  //+ QString::fromStdString(LinksRouting::WIdtoStr(winId())) + "')"
  //);

  _web_frame->evaluateJavaScript(code);
  //std::cout << "search: " << _edit->currentText().toStdString() << std::endl;
}
