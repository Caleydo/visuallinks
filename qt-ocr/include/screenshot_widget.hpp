#ifndef _SCREENSHOT_WIDGET_HPP_
#define _SCREENSHOT_WIDGET_HPP_

#include <QWidget>
#include <QPixmap>

class ScreenshotWidget: public QWidget
{
  Q_OBJECT

  public:

    ScreenshotWidget(QWidget *parent = 0);
    virtual ~ScreenshotWidget();

  private slots:

    void updateClipboard();

  protected:

    virtual void showEvent(QShowEvent* event);
    virtual void paintEvent(QPaintEvent* event);

    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);

  private:

    enum class State
    {
      SELECT_POSITION,
      SELECT_SIZE
    } _state;

    QPixmap _screenshot;
    QRect _selection;

    void detect(QPixmap selection);

};

#endif /* _SCREENSHOT_WIDGET_HPP_ */
