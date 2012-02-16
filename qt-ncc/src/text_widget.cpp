/*!
 * @file text_widget.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 15.02.2012
 */

#include "text_widget.hpp"
#include "FastMatchTemplate.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QFont>
#include <QPainter>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <tuple>

cv::Mat QImage2cvMat(const QImage& img)
{
  cv::Mat tmp
  (
    img.height(),
    img.width(),
    CV_8UC4,
    (uchar*)img.bits(),
    img.bytesPerLine()
  );

  cv::Mat mat(tmp.rows, tmp.cols, CV_8UC3);
  int from_to[] = {0,0,  1,1,  2,2};
  cv::mixChannels(&tmp, 1, &mat, 1, from_to, 3);

  return mat;
};

//------------------------------------------------------------------------------
TextWidget::TextWidget(QWidget *parent):
  QWidget( parent ),
  _edit( new QLineEdit(this) ),
  _button( new QPushButton(this) )
{
  QHBoxLayout* layout = new QHBoxLayout(this);

  layout->addWidget(_edit);

  _button->setText("Search String");
  layout->addWidget(_button);

  connect(_button, SIGNAL(pressed()), this, SLOT(triggerSearch()));
}

//------------------------------------------------------------------------------
TextWidget::~TextWidget()
{

}

//------------------------------------------------------------------------------
void TextWidget::triggerSearch()
{
  typedef std::tuple<QString, QFont::StyleHint, int> FontDescription;
  FontDescription fonts[] = {
    std::make_tuple("monospace", QFont::TypeWriter, 8),
    std::make_tuple("monospace", QFont::TypeWriter, 10),
    std::make_tuple("monospace", QFont::TypeWriter, 12),
    std::make_tuple("", QFont::Helvetica,  8),
    std::make_tuple("", QFont::Helvetica, 10),
    std::make_tuple("", QFont::Helvetica, 11),
    std::make_tuple("", QFont::Helvetica, 12),
    std::make_tuple("", QFont::Helvetica, 14),
    std::make_tuple("sans-serif", QFont::SansSerif,  8),
    std::make_tuple("sans-serif", QFont::SansSerif, 10),
    std::make_tuple("sans-serif", QFont::SansSerif, 11),
    std::make_tuple("sans-serif", QFont::SansSerif, 12),
    std::make_tuple("sans-serif", QFont::SansSerif, 13),
    std::make_tuple("sans-serif", QFont::SansSerif, 14),
  };

  // Prepare screenshot
  cv::Mat screenshot = QImage2cvMat( QPixmap::grabWindow(QApplication::desktop()->winId()).toImage() );
  cv::Mat screen;
  cvtColor(screenshot, screen, CV_BGR2GRAY);


  // Get memory for matching
  int result_cols =  screen.cols;
  int result_rows = screen.rows;

  cv::Mat result;
  result.create(result_cols, result_rows, CV_32FC1);

  // Now try every font description
  for( size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i )
  {
    QFont font;
    if( !std::get<0>(fonts[i]).isEmpty() )
      font.setFamily( std::get<0>(fonts[i]) );

    font.setStyleHint( std::get<1>(fonts[i]));
    font.setPointSize( std::get<2>(fonts[i]) );

    std::cout << "Found: " << font.family().toStdString()
                           << " (" << font.pointSize() << ")"
                           << std::endl;

    QFontMetrics font_metrics(font);
    QRect text_rect = font_metrics.boundingRect(_edit->text());

    // render search string
    QPixmap pix(text_rect.size());
    QPainter painter(&pix);
    pix.fill(Qt::white);
    painter.setFont(font);
    painter.drawText(-text_rect.left(), -text_rect.top(), _edit->text());
    painter.end();

    cv::Mat text_template;
    cvtColor(QImage2cvMat(pix.toImage()), text_template, CV_BGR2GRAY);

    //cv::matchTemplate(screen, text_template, result, CV_TM_CCOEFF_NORMED);

    // perform the match
    std::vector<cv::Point> foundPointsList;
    std::vector<double> confidencesList;

    // start the timer
    //clock_t start = clock();
    if( !FastMatchTemplate( screen,
                            text_template,
                            &foundPointsList,
                            &confidencesList,
                            60 ) )
      continue;

    //clock_t elapsed = clock() - start;
    //long int timeMs = ((float)elapsed) / ((float)CLOCKS_PER_SEC) * 1000.0f;
    //printf("\nINFO: FastMatchTemplate() function took %ld ms.\n", timeMs);

    DrawFoundTargets( &screenshot,
                      text_template.size(),
                      foundPointsList,
                      confidencesList );

//    for( int y = 0; y < result.rows - text_template.rows; ++y )
//    {
//      cv::Mat row = result.row(y);
//      for( int x = 0; x < result.cols - text_template.cols; ++x )
//      {
//        if( row.at<float>(x) > 0.8 )
//          cv::rectangle
//          (
//            screenshot,
//            cv::Point(x, y),
//            cv::Point(x + text_template.cols, y + text_template.rows),
//            cv::Scalar(0,0,255),
//            2
//          );
//      }
//    }
  }

  cv::imshow("img", screenshot);
}
