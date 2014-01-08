/*
 * Shader.hpp
 *
 *  Created on: Nov 4, 2013
 *      Author: tom
 */

#ifndef SHADER_HPP_
#define SHADER_HPP_

#include <QFile>
#include <QGLShaderProgram>
#include <memory>

namespace qtfullscreensystem
{

  class Shader;
  typedef std::shared_ptr<Shader> ShaderPtr;

  class Shader:
    public QGLShaderProgram
  {
    public:

      /**
       *
       * @param vert  Path to vertex shader
       * @param frag  Path to fragment shader
       */
      static ShaderPtr loadFromFiles(QString vert, QString frag);
  };


} // namespace qtfullscreensystem

#endif /* SHADER_HPP_ */
