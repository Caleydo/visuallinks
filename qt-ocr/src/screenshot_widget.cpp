#include "screenshot_widget.hpp"

#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QClipboard>

#include <iostream>

//------------------------------------------------------------------------------
ScreenshotWidget::ScreenshotWidget(QWidget *parent):
  QWidget( parent ),
  _state( State::SELECT_POSITION )
{
  // transparent, always on top window without any decoration
  // TODO check windows
  setWindowFlags( Qt::WindowStaysOnTopHint
                | Qt::FramelessWindowHint
                //| Qt::X11BypassWindowManagerHint
                );
  setWindowState( Qt::WindowMaximized );
  setGeometry( QApplication::desktop()->screenGeometry(this) );
}

//------------------------------------------------------------------------------
ScreenshotWidget::~ScreenshotWidget()
{

}

//------------------------------------------------------------------------------
void ScreenshotWidget::updateClipboard()
{
  std::cout << "Clipboard: " << QApplication::clipboard()->text().toStdString() << std::endl;
  std::cout << "Selection: " << QApplication::clipboard()->text(QClipboard::Selection).toStdString() << std::endl;
}

//------------------------------------------------------------------------------
void ScreenshotWidget::showEvent(QShowEvent* event)
{
  _state = State::SELECT_POSITION;
  setCursor(Qt::CrossCursor);

  _screenshot = QPixmap::grabWindow(QApplication::desktop()->winId());
}

//------------------------------------------------------------------------------
void ScreenshotWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  // darkened screenshot
  painter.drawPixmap(0, 0, _screenshot);
  painter.setBrush( QBrush(QColor(50, 30, 30, 120)) );
  painter.drawRect( rect() );

  if( _state == State::SELECT_SIZE )
  {
    QPoint top_left( std::min(_selection.left(), _selection.right()),
                     std::min(_selection.top(), _selection.bottom()) );
    QSize size( std::abs(_selection.right() - _selection.left()),
                std::abs(_selection.top() - _selection.bottom()) );

    if( size.width() > 1 && size.height() > 1 )
      // repaint selected area
      painter.drawPixmap(top_left, _screenshot, QRect(top_left, size));

    // and add a border
    painter.setBrush( Qt::NoBrush );
    painter.setPen( QPen(QBrush(Qt::red), 2) );
    painter.drawRect(_selection);
  }
  else
  {
    painter.setPen( QPen(QBrush(Qt::white), 2) );
    painter.setFont( QFont("Arial", 20, QFont::Bold) );
    painter.drawText(10,30,"Select a region.");
  }
}

//------------------------------------------------------------------------------
void ScreenshotWidget::mousePressEvent(QMouseEvent* event)
{
  if( event->button() == Qt::LeftButton )
  {
    if( _state == State::SELECT_POSITION )
    {
      _state = State::SELECT_SIZE;

      _selection.setTopLeft( event->pos() );
      _selection.setSize( QSize(0,0) );

      update();
    }
  }
}

//------------------------------------------------------------------------------
void ScreenshotWidget::mouseMoveEvent(QMouseEvent* event)
{
  if( _state != State::SELECT_SIZE )
    return;

  _selection.setBottomRight(event->pos());

  int dx = _selection.left() - _selection.right(),
      dy = _selection.top() - _selection.bottom();

  if( dx * dy > 0 )
    setCursor(Qt::SizeFDiagCursor);
  else
    setCursor(Qt::SizeBDiagCursor);

  update();
}

//------------------------------------------------------------------------------
void ScreenshotWidget::mouseReleaseEvent(QMouseEvent* event)
{
  if( event->button() == Qt::LeftButton )
  {
    if( _state == State::SELECT_SIZE )
    {
      hide();

      QSize size( std::abs(_selection.right() - _selection.left()),
                  std::abs(_selection.top() - _selection.bottom()) );

      detect( _screenshot.copy(_selection).scaled(4 * size, Qt::KeepAspectRatio, Qt::SmoothTransformation) );
    }
  }
}

// TODO move to separate class

#include <clocale>
#include <cstdlib>
#include <tesseract/baseapi.h>

//------------------------------------------------------------------------------
void ScreenshotWidget::detect(QPixmap selection)
{
  std::cout << "Tesseract version "
            << tesseract::TessBaseAPI::Version() << std::endl;

  const char* tess_data_path = getenv("TESSERACT_DATA_DIR");
  if( !tess_data_path )
  {
    tess_data_path = "/usr/share/tesseract-ocr/";
    std::cout << "TESSERACT_DATA_DIR not set. Defaulting to "
              << tess_data_path
              << std::endl;
  }

  // Qt seems to modify locale while Tesseract needs the correct locale for
  // parsing config files with the correct separators for floating point
  // numbers
  setlocale(LC_NUMERIC, "C");

  tesseract::TessBaseAPI tess_api;
  tess_api.Init(tess_data_path, "eng");
  tess_api.SetVariable
  (
    "tessedit_char_whitelist",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789,.:;\"!?=+-/*_()[]"
  );
  tess_api.SetVariable("tessedit_unrej_any_wd", "true");

  QImage img = selection.toImage();

  tess_api.SetImage
  (
    img.constBits(),
    img.width(),
    img.height(),
    img.depth() / 8,
    img.bytesPerLine()
  );

  tess_api.Recognize(nullptr);
  //tess_api.DumpPGM("test.pgm");

  QMessageBox msgBox;
  msgBox.setText(tess_api.GetUTF8Text());
  msgBox.exec();
}
