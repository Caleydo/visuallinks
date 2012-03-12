#include "application.hpp"

int main(int argc, char *argv[])
{
#if !_WIN32
  QCoreApplication::setAttribute(Qt::AA_X11InitThreads); // multithreaded OpenGL
#endif
  qtfullscreensystem::Application a(argc, argv);

  return a.exec();
}
