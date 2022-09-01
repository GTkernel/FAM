#ifndef __PROJ_CC_H__
#define __PROJ_CC_H__

#include "edgemap.hpp"
#include "graph_kernel.hpp"
#include "vertex_table.hpp"
#include "graph_types.hpp"
#include <client_runtime.hpp>
#include <array>
#include <atomic>
#include <memory>

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

namespace connected_components {
struct connected_components_vertex
{

  std::atomic<uint32_t> id;

  bool update_atomic(uint32_t const proposed_label) noexcept
  {
    bool ret = false;
    auto current = id.load(std::memory_order_relaxed);
    while (should_update(proposed_label)
           && !(ret = id.compare_exchange_weak(current,
                  proposed_label,
                  std::memory_order_relaxed,
                  std::memory_order_relaxed)))
      ;
    return ret;
  }

  bool vertex_map()
  {// non_atomic
    return true;
  }

  bool vertex_map_shortcut(connected_components_vertex *const vtable) noexcept
  {// non_atomic
    auto const my_id = id.load(std::memory_order_relaxed);// parent
    auto const l = vtable[my_id].id.load(std::memory_order_relaxed);// parent's parent
    if (my_id != l) {
      id.store(l, std::memory_order_relaxed);// need to add ourself back to frontier
      return true;
    } else {
      return false;
    }
  }

  bool should_update(uint32_t const proposed_label) noexcept
  {
    return proposed_label < id.load(std::memory_order_relaxed);
  }
};

template<famgraph::Buffering b> struct connected_components_kernel
{
public:
  famgraph::Generic_ctx<connected_components::connected_components_vertex> c;

  connected_components_kernel(struct client_context &ctx) : c(ctx, b) {}

  void operator()()
  {
    auto const total_verts = c.num_vertices;
    auto vtable = c.p.second.get();
    auto *frontier = &c.frontierA;
    auto *next_frontier = &c.frontierB;

    tbb::blocked_range<uint32_t> const my_range(0, total_verts);

    tbb::parallel_for(my_range, [&](auto const &range) {
      for (uint32_t v = range.begin(); v < range.end(); ++v) {
        vtable[v].id.store(v, std::memory_order_relaxed);
      }
    });

    auto CC_push = [&](uint32_t const v, uint32_t *const edges, uint32_t const n) noexcept
    {
      uint32_t const my_id = vtable[v].id.load(std::memory_order_relaxed);
      for (uint32_t i = 0; i < n; ++i) {// push out updates //make parallel
        uint32_t w = edges[i];
        if (vtable[w].update_atomic(my_id)) {
          next_frontier->set_bit(w);// activate w
        }
      }
    };

    frontier->set_all();
    while (!frontier->is_empty()) {
      famgraph::single_buffer::for_each_active_batch(
        *frontier, my_range, c, CC_push);
      frontier->clear();
      std::swap(frontier, next_frontier);
    }
  }
  void print_result()
  {
    auto const total_verts = c.num_vertices;
    auto const vtable = c.p.second.get();
    print_top_n(vtable, total_verts);
  }

private:
  void print_top_n(connected_components::connected_components_vertex const *const vtable,
    const uint32_t num_verts)
  {
    std::ofstream myfile;

    std::unordered_map<uint32_t, uint32_t> comp_counts;
    for (uint32_t v = 0; v < num_verts; ++v) {
      uint32_t rep_comp = vtable[v].id.load(std::memory_order_relaxed);

      if (comp_counts.find(rep_comp) == comp_counts.end()) {
        comp_counts.insert(std::make_pair(rep_comp, 1));
      } else {
        comp_counts[rep_comp]++;
      }
    }
    std::vector<std::pair<uint32_t, uint32_t>> v;
    for (auto p : comp_counts) { v.push_back(std::make_pair(p.second, p.first)); }
    std::sort(v.begin(), v.end());

    int trivial_comps = 0;
    int non_trivial = 0;
    for (auto it = v.rbegin(); it != v.rend(); ++it) {

      const uint32_t count = it->first;
      if (count > 1) {
        non_trivial++;

      } else {
        trivial_comps++;
      }
    }

    BOOST_LOG_TRIVIAL(info) << "# of total components: " << non_trivial + trivial_comps;
    BOOST_LOG_TRIVIAL(info) << "# of non-trivial components: " << non_trivial
                            << " largest component size: " << v.rbegin()->first;


  }
};
}// namespace connected_components
#endif// __PROJ_CC_H__
