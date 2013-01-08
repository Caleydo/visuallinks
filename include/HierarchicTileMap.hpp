#ifndef HIERARCHIC_TILE_MAP_HPP_
#define HIERARCHIC_TILE_MAP_HPP_

#include "float2.hpp"
#include "slotdata/image.hpp"
#include "PartitionHelper.hxx"

#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <iostream>

struct Tile:
  public LinksRouting::SlotType::Image
{

};

class HierarchicTileMap;
class Layer
{
  public:
    Layer(HierarchicTileMap* map);
    bool isInit() const;
    void init( size_t num_tiles_x,
               size_t num_tiles_y,
               size_t tile_width,
               size_t tile_height,
               size_t total_width,
               size_t total_height );

    Tile& getTile(size_t x, size_t y);
    HierarchicTileMap* getMap() const;
  
  private:
    HierarchicTileMap* _map;
    std::vector<std::vector<Tile>> _tiles;
};

struct MapRect
{
  size_t min_tile[2],
         max_tile[2];
  float min[2],
        max[2];
  Layer& layer;

  struct Quads
  {
    std::vector<float2> _coords;
    std::vector<float2> _tex_coords;
  };
  typedef std::map<Tile*, Quads> QuadList;

  QuadList getQuads() const;
  float2 getSize() const;

  typedef std::function<void(Tile&,size_t x,size_t y)> tile_callback_t;
  void foreachTile(const tile_callback_t& func) const;
};

class HierarchicTileMap
{
  public:
  
    HierarchicTileMap( unsigned int width,
                       unsigned int height,
                       unsigned int tile_width,
                       unsigned int tile_height );
    
    MapRect requestRect(const Rect& rect, size_t zoom);

    void setTileData( size_t x, size_t y, size_t zoom,
                      const char* data,
                      size_t data_size );

    float getLayerScale(size_t level) const;

    void setWidth(size_t width);
    void setHeight(size_t height);

    size_t getWidth() const { return _width; }
    size_t getHeight() const { return _height; }

    size_t getTileWidth() const { return _tile_width; }
    size_t getTileHeight() const { return _tile_height; }

    Partitions partitions_src, partitions_dest;
  
  private:
    unsigned int _width,
                 _height,
                 _tile_width,
                 _tile_height;
    
    std::vector<Layer> _layers;
    
    Layer& getLayer(size_t level);
};

typedef std::shared_ptr<HierarchicTileMap> HierarchicTileMapPtr;
typedef std::weak_ptr<HierarchicTileMap> HierarchicTileMapWeakPtr;

#endif /* HIERARCHIC_TILE_MAP_HPP_ */
