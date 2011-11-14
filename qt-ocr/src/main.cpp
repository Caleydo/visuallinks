#include "screenshot_widget.hpp"

#include <QApplication>
#include <QxtGlobalShortcut>

#include <iostream>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);

  QString shortcut = QObject::tr("CTRL+ALT+S");

  ScreenshotWidget w;
  QxtGlobalShortcut shortcut_handler;
  shortcut_handler.setShortcut( QKeySequence(shortcut) );

  QObject::connect(&shortcut_handler, SIGNAL(activated()), &w, SLOT(show()));

  std::cout << "Welcome to ocr for links. Press "
            << shortcut.toStdString()
            << " to select region."
            << std::endl;

  return app.exec();
}
