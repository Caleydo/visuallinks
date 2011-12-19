/*!
 * @file image.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 17.12.2011
 */

#ifndef _IMAGE_HPP_
#define _IMAGE_HPP_

namespace LinksRouting
{
namespace SlotType
{

  struct Image
  {
    enum
    {
      ImageRGBA8,
      ImageGray8,
      ImageRGBA32F,
      ImageGray32F,
      OpenGLTexture,
      OpenCLTexture,
      OpenCLBufferRGBA8,
      OpenCLBufferGray8,
      OpenCLBufferRGBA32F,
      OpenCLBufferGray32F
    } type;
    union
    {
      unsigned int id;
      unsigned char* pdata;
    };
    unsigned int width, height;
    unsigned int layers;
  };

} // namespace SlotType
} // namespace LinksRouting

#endif /* _IMAGE_HPP_ */
