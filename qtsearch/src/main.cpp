#include "text_widget.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  TextWidget w;
  //w.setWindowFlags(Qt::ToolTip | Qt::CustomizeWindowHint);
  w.show();

  return app.exec();
}
