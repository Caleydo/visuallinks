/*
 * dijkstra.cpp
 *
 *  Created on: Aug 26, 2013
 *      Author: caleydo
 */

#include "dijkstra.h"

#include <vector>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <iostream>
#include <queue>

//------------------------------------------------------------------------------
int signum(int x)
{
  return (x > 0) ? 1 : ((x < 0) ? -1 : 0);
}

//------------------------------------------------------------------------------
size_t divup(size_t x, size_t y)
{
  return (x % y) ? (x / y + 1) : (x / y);
}

//------------------------------------------------------------------------------
size_t divdown(size_t x, size_t y)
{
  return x / y;
}

namespace dijkstra
{
  //----------------------------------------------------------------------------
  Node::Node(uint32_t cost):
    _data(cost & MASK_COST)
  {}

  //----------------------------------------------------------------------------
  void Node::setStatus(Status status)
  {
    _data = (_data & ~MASK_STATUS) | (status << 26);
  }

  //----------------------------------------------------------------------------
  Node::Status Node::getStatus() const
  {
    return static_cast<Status>((_data & MASK_STATUS) >> 26);
  }

  //----------------------------------------------------------------------------
  void Node::setParentOffset(int x, int y)
  {
    // store -1, 0, 1 as 0, 1, 2
    _data = (_data & ~(MASK_PARENT_X | MASK_PARENT_Y))
          | (signum(x) + 1) << 30
          | (signum(y) + 1) << 28;
  }

  //----------------------------------------------------------------------------
  int Node::getParentOffsetX() const
  {
    return ((_data & MASK_PARENT_X) >> 30) - 1;
  }

  //----------------------------------------------------------------------------
  int Node::getParentOffsetY() const
  {
    return ((_data & MASK_PARENT_Y) >> 28) - 1;
  }

  //----------------------------------------------------------------------------
  void Node::setCost(uint32_t cost)
  {
    _data = (_data & ~MASK_COST) | (cost & MASK_COST);
  }

  //----------------------------------------------------------------------------
  uint32_t Node::getCost() const
  {
    return _data & MASK_COST;
  }

  //----------------------------------------------------------------------------
  Node& Node::operator=(uint32_t cost)
  {
    setCost(cost);
    return *this;
  }

//  friend std::ostream& operator<<(std::ostream& strm, const Node& node)
//  {
//    strm << "[" << node._data << "] status = " << node.getStatus()
//                              << ", cost = "   << node.getCost()
//                              << ", parent = " << node.getParentOffsetX() << "|" << node.getParentOffsetY()
//                              << std::endl;
//    return strm;
//  }

  class Queue:
    public std::priority_queue<NodePos>
  {
    public:
      void heapify()
      {
        std::make_heap(c.begin(), c.end(), comp);
      }
  };

  //----------------------------------------------------------------------------
  Grid::Grid(size_t width, size_t height):
    _width(width),
    _height(height),
    _nodes(width * height, Node(Node::MAX_COST)),
    _has_run(false)
  {}

  //----------------------------------------------------------------------------
  void Grid::reset()
  {
    for(auto& node: _nodes)
      node = Node(Node::MAX_COST);
    _has_run = false;
  }

  //----------------------------------------------------------------------------
  void Grid::run(size_t src_x, size_t src_y)
  {
    dijkstra::Queue open_nodes;

    dijkstra::NodePos node{src_x, src_y, this};
    node->setStatus(dijkstra::Node::QUEUED);
    node->setCost(0);
    open_nodes.push(node);

    do
    {
      auto cur_node = open_nodes.top();
      open_nodes.pop();

      cur_node->setStatus(dijkstra::Node::VISITED);
      size_t min_x = cur_node.x == 0 ? 0 : cur_node.x - 1,
             min_y = cur_node.y == 0 ? 0 : cur_node.y - 1,
             max_x = std::min(cur_node.x + 1, _width - 1),
             max_y = std::min(cur_node.y + 1, _height - 1);

      for(size_t x = min_x; x <= max_x; ++x)
        for(size_t y = min_y; y <= max_y; ++y)
        {
          dijkstra::NodePos neighbour{x, y, this};

          if( neighbour->getStatus() == dijkstra::Node::VISITED )
            continue;

          uint32_t new_cost = cur_node->getCost()
                            // TODO penalty (eg. for covering highlight regions)
                            + ((x == cur_node.x || y == cur_node.y) ? 2 : 3);

          if( new_cost < neighbour->getCost() )
          {
            neighbour->setCost(new_cost);
            neighbour->setParentOffset( cur_node.x - x,
                                        cur_node.y - y );

            if( neighbour->getStatus() == dijkstra::Node::QUEUED )
              open_nodes.heapify();
          }

          if( neighbour->getStatus() == dijkstra::Node::NORMAL )
          {
            neighbour->setStatus(dijkstra::Node::QUEUED);
            open_nodes.push(neighbour);
          }
        }
    } while( !open_nodes.empty() );

    _has_run = true;
  }

  //----------------------------------------------------------------------------
  bool Grid::hasRun() const
  {
    return _has_run;
  }

  //----------------------------------------------------------------------------
  const Node& Grid::operator()(size_t x, size_t y) const
  {
    x = std::min(x, _width - 1);
    y = std::min(y, _height - 1);
    //assert( x < _width && y < _height );
    return _nodes[y * _width + x];
  }

  //----------------------------------------------------------------------------
  void Grid::print() const
  {
//    for(size_t y = 0; y < _height; ++y)
//    {
//      for(size_t x = 0; x < _width; ++x)
//        std::cout << (*this)(x, y) << ", ";
//      std::cout << "\n";
//    }
//    std::cout << std::endl;
  }

  //----------------------------------------------------------------------------
  void Grid::writeImage(const std::string& name)
  {
    std::ofstream img( (name + ".pgm").c_str() );
    img << "P2\n"
        << _width << " " << _height << "\n"
        << (_width + _height) * 2 << "\n";

    for(auto& node: _nodes)
      img << node.getCost() << "\n";
  }

  //----------------------------------------------------------------------------
  bool NodePos::operator<(const NodePos& rhs) const
  {
    return (*this)->getCost() > rhs->getCost();
  }

  //----------------------------------------------------------------------------
  NodePos const NodePos::getParent() const
  {
    Node const& self = operator*();
    return NodePos{ x + self.getParentOffsetX(),
                    y + self.getParentOffsetY(),
                    grid };
  }

  //----------------------------------------------------------------------------
  Node& NodePos::operator*()
  {
    return (*grid)(x, y);
  }

  //----------------------------------------------------------------------------
  Node const& NodePos::operator*() const
  {
    return (*grid)(x, y);
  }

  //----------------------------------------------------------------------------
  Node* NodePos::operator->()
  {
    return &(*grid)(x, y);
  }

  //----------------------------------------------------------------------------
  Node const* NodePos::operator->() const
  {
    return &(*grid)(x, y);
  }
}

#ifdef BUILD_MAIN
int main(int, char*[])
{
  const size_t GRID_SIZE = 16;
  const size_t grid_width  = divup(1920, GRID_SIZE),
               grid_height = divup(1080, GRID_SIZE);
  size_t num_regions = 100;
  srand(time(0));

  std::vector<dijkstra::Grid> grids
  (
    num_regions,
    dijkstra::Grid(grid_width, grid_height)
  );

  for(auto& grid: grids)
  {
    grid.run(rand() % grid_width, rand() % grid_height);
  }

  dijkstra::NodePos min_node{0,0, &grids[0]};
  size_t min_cost = dijkstra::Node::MAX_COST;

  for(size_t x = 0; x < grid_width; ++x)
    for(size_t y = 0; y < grid_height; ++y)
    {
      size_t cost = 0;
      for(auto const& grid: grids)
        cost += grid(x, y).getCost();

      if( cost < min_cost )
      {
        min_cost = cost;
        min_node.x = x;
        min_node.y = y;
      }
    }
  //std::cout << "min: " << *min_node << std::endl;

  return 0;
}
#endif
