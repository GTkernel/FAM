#ifndef __PROJ_PAGERANKDELTA_H__
#define __PROJ_PAGERANKDELTA_H__

#include "edgemap.hpp"
#include "graph_kernel.hpp"
#include "vertex_table.hpp"
#include "graph_types.hpp"
#include <client_runtime.hpp>
#include <array>
#include <atomic>
#include <memory>
#include <cmath>

#include <utility>
#include <algorithm>
#include <assert.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <oneapi/tbb.h>
#pragma GCC diagnostic pop

namespace pagerank_delta {

constexpr float alpha = 0.85f;
constexpr float epsilon = 0.001f;
constexpr float INIT_RESIDUAL = 1 - alpha;

struct pagerank_delta_vertex
{
  float value;
  float delta;
  std::atomic<float> residual;

  bool update_add_atomic(float const s_val) noexcept
  {
    auto current = residual.load(std::memory_order_relaxed);
    while (!residual.compare_exchange_weak(
      current, current + s_val, std::memory_order_relaxed, std::memory_order_relaxed))
      ;
    return true;
  }

  bool vertex_map() noexcept
  {
    auto res = residual.load(std::memory_order_relaxed);
    if (std::abs(res) > epsilon) {
      value += res;
      delta = res;// load up
      residual.store(0, std::memory_order_relaxed);// clear
      return true;
    } else {
      return false;
    }
  }
};

template<famgraph::Buffering b> struct pagerank_delta_kernel
{
public:
  famgraph::Generic_ctx<pagerank_delta::pagerank_delta_vertex> c;

  pagerank_delta_kernel(struct client_context &ctx) : c(ctx, b) {}

  void operator()()
  {
    auto const total_verts = c.num_vertices;
    auto vtable = c.p.second.get();
    auto *frontier = &c.frontierA;
    auto *next_frontier = &c.frontierB;
    uint32_t const max_iterations = 200;

    tbb::blocked_range<uint32_t> const my_range(0, total_verts);
    tbb::parallel_for(my_range, [&](auto const &range) {
      for (uint32_t v = range.begin(); v < range.end(); ++v) {
        vtable[v].value = 0.0;
        vtable[v].delta = pagerank_delta::INIT_RESIDUAL;
        vtable[v].residual = 0.0;
      }
    });

    frontier->set_all();

    auto pagerank_push = [&](
      uint32_t const v, uint32_t *const edges, uint32_t const n) noexcept
    {
      if (n > 0) {// unneeded?
        float const my_delta = vtable[v].delta;
        vtable[v].delta = 0.0;// used all of our sauce
        float const my_val = my_delta * pagerank_delta::alpha / static_cast<float>(n);
        for (uint32_t i = 0; i < n; ++i) {
          uint32_t w = edges[i];
          vtable[w].update_add_atomic(my_val);
        }
      }
    };

    uint32_t iter = 0;
    while (!frontier->is_empty() && ++iter < max_iterations) {
      tbb::tick_count start = tbb::tick_count::now();
      famgraph::single_buffer::for_each_active_batch(
        *frontier, my_range, c, pagerank_push);

      tbb::parallel_for(my_range, [&](auto const &range) {
        for (uint32_t v = range.begin(); v < range.end(); ++v) {
          if (vtable[v].vertex_map()) { next_frontier->set_bit(v); }
        }
      });

      frontier->clear();
      std::swap(frontier, next_frontier);

      tbb::tick_count end = tbb::tick_count::now();
      BOOST_LOG_TRIVIAL(info) << "Iteration time(s) " << (end - start).seconds();
    }
  }

  void print_result()
  {
    auto const total_verts = c.num_vertices;
    auto const vtable = c.p.second.get();

    print_top_20(vtable, total_verts);
  }

private:
  void print_top_20(pagerank_delta::pagerank_delta_vertex *const vtable,
    const uint32_t num_verts)
  {
    std::vector<std::pair<float, uint32_t>> t20;
    for (uint32_t v = 0; v < num_verts; ++v) {
      if (t20.size() < 20) {
        t20.push_back(std::make_pair(vtable[v].value, v));
        std::sort(t20.begin(), t20.end());
      } else {
        if (vtable[v].value > t20[0].first) {
          t20[0] = std::make_pair(vtable[v].value, v);
          std::sort(t20.begin(), t20.end());
        }
      }
    }
    for (auto it = t20.rbegin(); it != t20.rend(); ++it) {
      const float rank = it->first;
      const uint32_t v = it->second;
      std::cout << v << ": " << rank << std::endl;
    }
  }
};

// void run_pagerank_delta_sb(std::unique_ptr<famgraph::vertex, famgraph::mmap_deleter>
// index,
//                            std::unique_ptr<pagerank_delta_vertex,
//                            famgraph::mmap_deleter> vertex_table, struct client_context*
//                            context);

// void run_pagerank_delta_db(std::unique_ptr<famgraph::vertex, famgraph::mmap_deleter>
// index,
//                            std::unique_ptr<pagerank_delta_vertex,
//                            famgraph::mmap_deleter> vertex_table, struct client_context*
//                            context);

}// namespace pagerank_delta
#endif// __PROJ_PAGERANKDELTA_H__
