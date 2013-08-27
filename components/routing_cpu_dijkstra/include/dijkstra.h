/*
 * dijkstra.h
 *
 *  Created on: Aug 26, 2013
 *      Author: tom
 */

#ifndef DIJKSTRA_H_
#define DIJKSTRA_H_

/*
 * dijkstra.cpp
 *
 *  Created on: Aug 26, 2013
 *      Author: caleydo
 */

#include <vector>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <iostream>
#include <queue>

size_t divup(size_t x, size_t y);
size_t divdown(size_t x, size_t y);

namespace dijkstra
{
  struct Node
  {
    protected:
      static const uint32_t MASK_PARENT_X = 0xc0000000;
      static const uint32_t MASK_PARENT_Y = 0x30000000;
      static const uint32_t MASK_STATUS   = 0x0c000000;
      static const uint32_t MASK_COST     = 0x03ffffff;

      static_assert( !(MASK_PARENT_X & MASK_PARENT_Y), "validate mask");
      static_assert( !(MASK_PARENT_X & MASK_STATUS),   "validate mask");
      static_assert( !(MASK_PARENT_Y & MASK_STATUS),   "validate mask");
      static_assert( !(MASK_PARENT_X & MASK_COST),     "validate mask");
      static_assert( !(MASK_PARENT_Y & MASK_COST),     "validate mask");
      static_assert( !(MASK_COST   & MASK_STATUS),     "validate mask");
      static_assert( (MASK_PARENT_X | MASK_PARENT_Y | MASK_STATUS | MASK_COST)
                     == 0xffffffff,
                     "validate mask (use all bits)" );

    public:
      static const uint32_t MAX_COST  = MASK_COST;
      enum Status: uint8_t
      {
        NORMAL = 0,
        QUEUED,
        VISITED
      };

      Node(uint32_t cost = MAX_COST);

      void setStatus(Status status);
      Status getStatus() const;

      void setParentOffset(int x, int y);

      int getParentOffsetX() const;
      int getParentOffsetY() const;

      void setCost(uint32_t cost);
      uint32_t getCost() const;

      Node& operator=(uint32_t cost);

    private:
      uint32_t _data;
  };

  struct Grid
  {
    public:
      Grid(size_t width, size_t height);

      size_t getWidth() const  { return _width;  }
      size_t getHeight() const { return _height; }

      void reset();
      void run(size_t src_x, size_t src_y);
      bool hasRun() const;

      const Node& operator()(size_t x, size_t y) const;
      Node& operator()(size_t x, size_t y)
      {
        return const_cast<Node&>(static_cast<const Grid&>(*this)(x, y));
      }

      void print() const;
      void writeImage(const std::string& name);

    private:
      size_t    _width,
                _height;
      std::vector<Node> _nodes;

      bool _has_run;
  };

  struct NodePos
  {
    size_t x, y;
    Grid* grid;

    bool operator<(const NodePos& rhs) const;

    NodePos const getParent() const;

    Node& operator*();
    Node const& operator*() const;

    Node* operator->();
    Node const* operator->() const;
  };

}

#endif /* DIJKSTRA_H_ */
