#ifndef __PROJ_BFS_H__
#define __PROJ_BFS_H__

#include "graph_kernel.hpp"
#include "vertex_table.hpp"
#include "graph_types.hpp"
#include <client_runtime.hpp>
#include <atomic>
#include <memory>

#include "communication_runtime.hpp"
#include "bitmap.hpp"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <oneapi/tbb.h>
#pragma GCC diagnostic pop

namespace bfs {
constexpr uint32_t NULLVERT = 0xFFFFFFFF;

struct bfs_vertex
{
  std::atomic<uint32_t> parent{ NULLVERT };

  bool update_atomic(uint32_t const t_parent) noexcept
  {// returns true if update succeeded
    uint32_t expect = NULLVERT;
    return parent.compare_exchange_strong(
      expect, t_parent, std::memory_order_relaxed, std::memory_order_relaxed);
  }
};


template<famgraph::Buffering b> struct bfs_kernel
{
public:
  famgraph::Generic_ctx<bfs::bfs_vertex> c;
  uint32_t const start_v;

  bfs_kernel(struct client_context &ctx)
    : c(ctx, b), start_v{ (*ctx.vm)["start-vertex"].as<uint32_t>() }
  {}

  void operator()()
  {
    auto const total_verts = c.num_vertices;
    auto vtable = c.p.second.get();
    auto *frontier = &c.frontierA;
    auto *next_frontier = &c.frontierB;
    tbb::blocked_range<uint32_t> const my_range(0, total_verts);

    BOOST_LOG_TRIVIAL(info) << "bfs start vertex: " << start_v;
    frontier->set_bit(start_v);
    vtable[start_v].update_atomic(0);// 0 distance to self
    uint32_t round = 0;
    auto bfs_push = [&](uint32_t const, uint32_t *const edges, uint32_t const n) noexcept
    {
      for (uint32_t i = 0; i < n; ++i) {// push out updates //make parallel
        uint32_t w = edges[i];
        if (vtable[w].update_atomic(round)) {
          next_frontier->set_bit(w);// activate w
        }
      }
    };
    while (!frontier->is_empty()) {
      ++round;
      famgraph::single_buffer::for_each_active_batch(
        *frontier, my_range, c, bfs_push);
      frontier->clear();
      std::swap(frontier, next_frontier);
    }

    BOOST_LOG_TRIVIAL(info) << "bfs rounds " << round;
  }

  void print_result() {}
};


void run_bfs_sb(std::unique_ptr<famgraph::vertex, famgraph::mmap_deleter> index,
  std::unique_ptr<bfs_vertex, famgraph::mmap_deleter> vertex_table,
  struct client_context *context);

void run_bfs_db(std::unique_ptr<famgraph::vertex, famgraph::mmap_deleter> index,
  std::unique_ptr<bfs_vertex, famgraph::mmap_deleter> vertex_table,
  struct client_context *context);
}// namespace bfs
#endif// __PROJ_BFS_H__
