/*
 * Shader.cpp
 *
 *  Created on: Nov 4, 2013
 *      Author: tom
 */

#include "Shader.hpp"

namespace qtfullscreensystem
{

  //----------------------------------------------------------------------------
  static const QByteArray loadShaderSource( const QString& file_name,
                                            const QByteArray defs = "" )
  {
    QFile file(file_name);
    if( !file.open(QFile::ReadOnly) )
    {
      qWarning() << "loadShaderSource: Unable to open file" << file_name;
      return QByteArray();
    }

    QByteArray src = file.readAll();
    if( defs.isEmpty() )
      return src;

    // #version needs to come before any other instruction...
    int version_start = src.indexOf("#version");
    if( version_start < 0 )
      return defs + "\n" + src;

    int version_end = src.indexOf('\n', version_start + 8);
    return src.left(version_end + 1) + defs + '\n' + src.mid(version_end + 1);
  }

  //----------------------------------------------------------------------------
  ShaderPtr Shader::loadFromFiles(QString vert, QString frag)
  {
    qDebug() << "loadShader("<< vert << "|" << frag << ")";

    ShaderPtr program = std::make_shared<Shader>();
    QByteArray defs = "#define USE_DESKTOP_BLEND "
#ifdef USE_DESKTOP_BLEND
    "1"
#else
    "0"
#endif
    ;

    if( !program->addShaderFromSourceCode( QGLShader::Vertex,
                                           loadShaderSource(vert, defs) ) )
    {
      qWarning() << "Failed to load vertex shader (" << vert << ")\n"
                 << program->log();
      return ShaderPtr();
    }

    if( !program->addShaderFromSourceCode( QGLShader::Fragment,
                                           loadShaderSource(frag, defs) ) )
    {
      qWarning() << "Failed to load fragment shader (" << frag << ")\n"
                 << program->log();
      return ShaderPtr();
    }

    if( !program->link() )
    {
      qWarning() << "Failed to link shader (" << vert << "|" << frag << ")\n"
                 << program->log();
      return ShaderPtr();
    }

    return program;
  }

} // namespace qtfullscreensystem
