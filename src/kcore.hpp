#ifndef __PROJ_KCORE_H__
#define __PROJ_KCORE_H__

#include "edgemap.hpp"
#include "graph_kernel.hpp"
#include "vertex_table.hpp"
#include "graph_types.hpp"
#include <client_runtime.hpp>
#include <array>
#include <atomic>
#include <memory>

#include "communication_runtime.hpp"
#include "bitmap.hpp"

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

#include <unordered_map>
#include <utility>
#include <algorithm>

namespace kcore {
struct kcore_vertex
{
  std::atomic<uint32_t> degree;

  uint32_t subtract_degree() { return degree.fetch_sub(1, std::memory_order_relaxed); }
};

template<famgraph::Buffering b> struct kcore_kernel
{
public:
  famgraph::Generic_ctx<kcore::kcore_vertex> c;
  uint32_t const k;

  kcore_kernel(struct client_context &ctx)
    : c(ctx, b), k((*ctx.vm)["kcore-k"].as<uint32_t>())
  {}

  void operator()()
  {
    auto const total_verts = c.num_vertices;
    auto const idx = c.p.first.get();
    auto vtable = c.p.second.get();
    auto *frontier = &c.frontierA;
    auto *next_frontier = &c.frontierB;

    tbb::blocked_range<uint32_t> const my_range(0, total_verts);
    tbb::parallel_for(my_range, [&](auto const &range) {
      for (uint32_t v = range.begin(); v < range.end(); ++v) {
        const uint32_t d = famgraph::get_num_edges(v, idx, c.num_vertices, c.num_edges);
        vtable[v].degree.store(d, std::memory_order_relaxed);
        if (d < k) { frontier->set_bit(v); }
      }
    });

    auto kcore_push = [&](
      uint32_t const, uint32_t *const edges, uint32_t const n) noexcept
    {
      for (uint32_t i = 0; i < n; ++i) {// push out updates //make parallel
        uint32_t w = edges[i];
        uint32_t old = vtable[w].subtract_degree();
        if (old == k) { next_frontier->set_bit(w); }
      }
    };
    
    while (!frontier->is_empty()) {
      famgraph::single_buffer::for_each_active_batch(
        *frontier, my_range, c, kcore_push);
      frontier->clear();
      std::swap(frontier, next_frontier);
    }
  }

  void print_result()
  {
    auto const total_verts = c.num_vertices;
    auto const vtable = c.p.second.get();
    tbb::blocked_range<uint32_t> const my_range(0, total_verts);

    BOOST_LOG_TRIVIAL(info) << "The " << k << "-core has size "
                            << kth_core_membership(my_range, vtable, k);
  }

private:
  uint32_t kth_core_membership(tbb::blocked_range<uint32_t> const &my_range,
    kcore::kcore_vertex const *const vtable,
    const uint32_t t_k)
  {
    return tbb::parallel_reduce(my_range,
      static_cast<uint32_t>(0),
      [&vtable, &t_k](auto const &r, auto init) {
        for (uint32_t i = r.begin(); i != r.end(); ++i) {
          init = vtable[i].degree >= t_k ? init + 1 : init;
        }
        return init;
      },
      [](auto const &t_a, auto const &t_b) { return t_a + t_b; });
  }
};
}// namespace kcore
#endif// __PROJ_KCORE_H__
