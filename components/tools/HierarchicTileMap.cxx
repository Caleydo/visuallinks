/*
 * HierarchicTileMap.cxx
 *
 *  Created on: 20.11.2012
 *      Author: tom
 */

#include "HierarchicTileMap.hpp"

#include <GL/gl.h>

#include <algorithm>
#include <cassert>
#include <cstdint>

//----------------------------------------------------------------------------
Layer::Layer(HierarchicTileMap* map):
  _map(map)
{

}

//------------------------------------------------------------------------------
bool Layer::isInit() const
{
  return !_tiles.empty();
}

//------------------------------------------------------------------------------
void Layer::init( size_t num_tiles_x,
                  size_t num_tiles_y,
                  size_t tile_width,
                  size_t tile_height,
                  size_t total_width,
                  size_t total_height )
{
  _tiles.resize(num_tiles_x, std::vector<Tile>(num_tiles_y));
  size_t cur_width = 0;
  for(auto col = _tiles.begin(); col != _tiles.end(); ++col)
  {
    size_t width = std::min(tile_width, total_width - cur_width);
    size_t cur_height = 0;
    for(auto cell = col->begin(); cell != col->end(); ++cell)
    {
      size_t height = std::min(tile_height, total_height - cur_height);

      cell->width = width;
      cell->height = height;
      cell->type = Tile::NONE;

      cur_height += height;
    }

    cur_width += width;
  }
}

//------------------------------------------------------------------------------
Tile& Layer::getTile(size_t x, size_t y)
{
  return _tiles.at(x).at(y);
}

//------------------------------------------------------------------------------
HierarchicTileMap* Layer::getMap() const
{
  return _map;
}

//------------------------------------------------------------------------------
size_t Layer::sizeX() const
{
  return _tiles.size();
}

//------------------------------------------------------------------------------
size_t Layer::sizeY() const
{
  return _tiles.empty() ? 0 : _tiles.front().size();
}

//------------------------------------------------------------------------------
MapRect::QuadList MapRect::getQuads() const
{
  QuadList ret;
  size_t tile_width = layer.getMap()->getTileWidth(),
         tile_height = layer.getMap()->getTileHeight();

  foreachTile([&](Tile& tile, size_t x, size_t y)
  {
    size_t min_x = std::max<float>(min[0] + 0.5, x * tile_width),
           min_y = std::max<float>(min[1] + 0.5, y * tile_height),
           max_x = std::min<float>(max[0] + 0.5, x * tile_width + tile.width),
           max_y = std::min<float>(max[1] + 0.5, y * tile_height + tile.height);

    float2 tex_min( float(min_x - x * tile_width) / tile.width,
                    float(min_y - y * tile_height) / tile.height ),
           tex_max( float(max_x - x * tile_width) / tile.width,
                    float(max_y - y * tile_height) / tile.height );

    size_t left = min_x - min[0],
           top = min_y - min[1],
           right = max_x - min[0],
           bottom = max_y - min[1];

    Quads quads;
    quads._tex_coords.push_back(float2(tex_min.x, tex_min.y));
    quads._tex_coords.push_back(float2(tex_max.x, tex_min.y));
    quads._tex_coords.push_back(float2(tex_max.x, tex_max.y));
    quads._tex_coords.push_back(float2(tex_min.x, tex_max.y));

    quads._coords.push_back(float2(left,  top));
    quads._coords.push_back(float2(right, top));
    quads._coords.push_back(float2(right, bottom));
    quads._coords.push_back(float2(left,  bottom));

    ret[ &tile ] = quads;
  });
  return ret;
}

//------------------------------------------------------------------------------
float2 MapRect::getSize() const
{
  float2 size;
  size_t tile_width = layer.getMap()->getTileWidth(),
         tile_height = layer.getMap()->getTileHeight();
  foreachTile([&](Tile& tile, size_t x, size_t y)
  {
    float max_x = x * tile_width + tile.width,
          max_y = y * tile_height + tile.height;

    if( max_x > size.x )
      size.x = max_x;
    if( max_y > size.y )
      size.y = max_y;
  });

  return size;
}

//------------------------------------------------------------------------------
void MapRect::foreachTile(const MapRect::tile_callback_t& func) const
{
  size_t max_x = std::min(layer.sizeX(), max_tile[0]),
         max_y = std::min(layer.sizeY(), max_tile[1]);

  for(size_t x = min_tile[0]; x < max_x; ++x)
    for(size_t y = min_tile[1]; y < max_y; ++y)
      func(layer.getTile(x, y), x, y);
}

//------------------------------------------------------------------------------
HierarchicTileMap::HierarchicTileMap( unsigned int width,
                                      unsigned int height,
                                      unsigned int tile_width,
                                      unsigned int tile_height ):
   margin_left(0),
   margin_right(0),
  _width( width ),
  _height( height ),
  _tile_width( tile_width ),
  _tile_height( tile_height ),
  _change_id(0)
{

}

//------------------------------------------------------------------------------
void HierarchicTileMap::addTileChangeCallback(TileChangeCallback cb)
{
  _change_callbacks.push_back(cb);
}

//------------------------------------------------------------------------------
MapRect HierarchicTileMap::requestRect(const Rect& rect, size_t zoom)
{

  float scale = 1.f;
  if( zoom != static_cast<size_t>(-1) )
    scale = (std::pow(2.0f, static_cast<float>(zoom)) * _tile_height) / _height;

  Layer& layer = getLayer(zoom);

  Rect layer_rect = scale * rect;
  size_t min_tile_x = std::floor((layer_rect.l() + 1) / _tile_width),
         min_tile_y = std::floor((layer_rect.t() + 1) / _tile_height),
         max_tile_x = std::ceil((layer_rect.r() - 1) / _tile_width),
         max_tile_y = std::ceil((layer_rect.b() - 1) / _tile_height);
  float min_x = layer_rect.l(),
        min_y = layer_rect.t(),
        max_x = layer_rect.r(),
        max_y = layer_rect.b();

  MapRect map_rect = {
    {min_tile_x, min_tile_y},
    {max_tile_x, max_tile_y},
    {min_x, min_y},
    {max_x, max_y},
    layer
  };

  return map_rect;
}

//------------------------------------------------------------------------------
void HierarchicTileMap::setTileData( size_t x, size_t y, size_t zoom,
                                     const char* data,
                                     size_t data_size )
{
  Tile& tile = getLayer(zoom).getTile(x, y);

  assert( tile.width * tile.height * 4 == data_size );

  if( tile.type == Tile::ImageRGBA8 )
    delete[] tile.pdata;
  tile.pdata = new uint8_t[data_size];
  memcpy(tile.pdata, data, data_size);

  tile.type = Tile::ImageRGBA8;

  ++_change_id;

  for(auto cb: _change_callbacks)
    cb(*this, x, y, zoom);
}

//------------------------------------------------------------------------------
bool HierarchicTileMap::render( const Rect& src_region,
                                const float2& src_size,
                                const Rect& target_region,
                                size_t zoom,
                                bool auto_center,
                                double alpha,
                                GLImageCache* cache )
{
  MapRect rect = requestRect(src_region, zoom);
  float2 rect_size = rect.getSize();
  MapRect::QuadList quads = rect.getQuads();
  for(auto quad = quads.begin(); quad != quads.end(); ++quad)
  {
    GLuint tex_id = 0;

    if( quad->first->type == Tile::ImageRGBA8 )
    {
      if( cache )
      {
        auto cache_it = cache->find(quad->first->pdata);
        if( cache_it != cache->end() )
          tex_id = cache_it->second;
      }

      if( !tex_id )
      {
        std::cout << "load tile to GPU (" << quad->first->width
                                   << "x" << quad->first->height
                                   << ")" << std::endl;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);

        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        glTexImage2D(
          GL_TEXTURE_2D,
          0,
          GL_RGBA,
          quad->first->width,
          quad->first->height,
          0,
          GL_RGBA,
          GL_UNSIGNED_BYTE,
          quad->first->pdata
        );

        if( cache )
        {
          (*cache)[ quad->first->pdata ] = tex_id;
        }
        else
        {
          delete[] quad->first->pdata;
          quad->first->id = tex_id;
          quad->first->type = Tile::OpenGLTexture;
        }
      }
    }
    else if( quad->first->type == Tile::OpenGLTexture )
    {
      tex_id = quad->first->id;
    }

    if( !tex_id )
      continue;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    float offset_x = 0;
    if( !auto_center && target_region.size.x > rect_size.x )
      offset_x = (target_region.size.x - rect_size.x) / 2;

    glPushMatrix();
    glTranslatef( target_region.pos.x + offset_x,
                  target_region.pos.y,
                  0 );

    glColor4f(alpha,alpha,alpha,alpha);
    glBegin(GL_QUADS);
    for(size_t i = 0; i < quad->second._coords.size(); ++i)
    {
      glTexCoord2fv(quad->second._tex_coords[i].ptr());
      glVertex2fv(quad->second._coords[i].ptr());

//      std::cout << "    " << quad->second._coords[i] << ", tex: " << quad->second._tex_coords[i] << "\n";
    }
    glEnd();

    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  // ----------
  // Scrollbars

  float2 bar_size(   std::max(src_region.size.x / src_size.x, 0.05f)
                   * target_region.size.x,
                     std::max(src_region.size.y / src_size.y, 0.05f)
                   * target_region.size.y ),
         rel_pos( src_region.pos.x / (src_size.x - src_region.size.x),
                  src_region.pos.y / (src_size.y - src_region.size.y) );

  glColor4f(1,0.5,0.25,alpha);
  glLineWidth(3);

  // Horizontal
  if( src_region.size.x < src_size.x )
  {
    float x = target_region.pos.x
            + rel_pos.x * (target_region.size.x - bar_size.x),
          y = target_region.pos.y + target_region.size.y - 6;

    glBegin(GL_LINE_STRIP);
      glVertex2f(x, y);
      glVertex2f(x + bar_size.x, y);
    glEnd();
  }

  // Vertical
  if( src_region.size.y < src_size.y )
  {
    float x = target_region.pos.x + target_region.size.x - 6,
          y = target_region.pos.y
            + rel_pos.y * (target_region.size.y - bar_size.y);

    glBegin(GL_LINE_STRIP);
      glVertex2f(x, y);
      glVertex2f(x, y + bar_size.y);
    glEnd();
  }

  return true;
}

//------------------------------------------------------------------------------
float HierarchicTileMap::getLayerScale(size_t zoom) const
{
  if( zoom == static_cast<size_t>(-1) )
    return 1;
  else
    return std::pow(2.0f, static_cast<float>(zoom))
         * _tile_height
         / static_cast<float>(_height);
}

//------------------------------------------------------------------------------
void HierarchicTileMap::setWidth(size_t width)
{
  _layers.clear();
  _width = width;

  ++_change_id;
}

//------------------------------------------------------------------------------
void HierarchicTileMap::setHeight(size_t height)
{
  _layers.clear();
  _height = height;

  ++_change_id;
}

//------------------------------------------------------------------------------
Layer& HierarchicTileMap::getLayer(size_t zoom)
{
  size_t level = (zoom != static_cast<size_t>(-1)) ? zoom : 0;
  if( level >= _layers.size() )
    _layers.resize(level + 1, Layer(this));

  Layer& layer = _layers.at(level);
  if( !layer.isInit() )
  {
    float scale = getLayerScale(zoom);
    unsigned int num_tiles_x = std::ceil(scale * _width / _tile_width),
                 num_tiles_y = std::ceil(scale * _height / _tile_height);

    layer.init( num_tiles_x, num_tiles_y,
                _tile_width, _tile_height,
                scale * _width + 0.5,
                scale * _height + 0.5 );
  }

  return layer;
}
