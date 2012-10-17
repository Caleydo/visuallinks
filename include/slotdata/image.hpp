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
    enum Type
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
    Image():
      type(ImageRGBA8),
      pdata(0),
      width(0),
      height(0),
      layers(1)
    { }
    Image(unsigned int w, 
          unsigned int h, 
          unsigned int i,  
          Type t = OpenGLTexture, 
          unsigned int l = 1) : 
              type(t), id(i),
              width(w), height(h),
              layers(l)
    { }
    Image(unsigned int w, 
          unsigned int h, 
          unsigned char* d,  
          Type t = ImageRGBA8, 
          unsigned int l = 1) : 
              type(t), pdata(d),
              width(w), height(h),
              layers(l)
    { }         
  };

} // namespace SlotType
} // namespace LinksRouting

#endif /* _IMAGE_HPP_ */
