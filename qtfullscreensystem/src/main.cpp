#include "application.hpp"

int main(int argc, char *argv[])
{
  QCoreApplication::setAttribute(Qt::AA_X11InitThreads); // multithreaded OpenGL
  qtfullscreensystem::Application a(argc, argv);

  return a.exec();
}
