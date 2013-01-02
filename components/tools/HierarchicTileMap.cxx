/*
 * HierarchicTileMap.cxx
 *
 *  Created on: 20.11.2012
 *      Author: tom
 */

#include "HierarchicTileMap.hpp"
#include <cassert>

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
  return _tiles[x][y];
}

//------------------------------------------------------------------------------
HierarchicTileMap* Layer::getMap() const
{
  return _map;
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
                    
    std::cout << float2(min_x, min_y) << " => " << float2(max_x, max_y)
              << ", tex: " << tex_min << " => " << tex_max << std::endl;

    size_t left = min_x - min[0],
           top = min_y - min[1],
           right = left + (tex_max.x - tex_min.x) * tile.width,
           bottom = top + (tex_max.y - tex_min.y) * tile.height;

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
  for(size_t x = min_tile[0]; x < max_tile[0]; ++x)
    for(size_t y = min_tile[1]; y < max_tile[1]; ++y)
      func(layer.getTile(x, y), x, y);
}

//------------------------------------------------------------------------------
HierarchicTileMap::HierarchicTileMap( unsigned int width,
                                      unsigned int height,
                                      unsigned int tile_width,
                                      unsigned int tile_height ):
  _width( width ),
  _height( height ),
  _tile_width( tile_width ),
  _tile_height( tile_height )
{

}

//------------------------------------------------------------------------------
MapRect HierarchicTileMap::requestRect(const Rect& rect, size_t zoom)
{
  Layer& layer = getLayer(zoom);
  float scale = (std::pow(2, zoom) * _tile_height) / _height;

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

  std::cout << "update tile #" << zoom << " (" << x << "|" << y << ")" << std::endl;
}

//------------------------------------------------------------------------------
float HierarchicTileMap::getLayerScale(size_t level) const
{
  return std::pow(2, level) * _tile_height / static_cast<float>(_height);
}

//------------------------------------------------------------------------------
Layer& HierarchicTileMap::getLayer(size_t level)
{
  if( level >= _layers.size() )
    _layers.resize(level + 1, Layer(this));

  Layer& layer = _layers[level];
  if( !layer.isInit() )
  {
    unsigned int num_tiles_y = std::pow(2, level);
    float scale = getLayerScale(level);
    unsigned int num_tiles_x = std::ceil(scale * _width / _tile_width);

    layer.init( num_tiles_x, num_tiles_y,
                _tile_width, _tile_height,
                scale * _width + 0.5,
                scale * _height + 0.5 );
  }

  return layer;
}
