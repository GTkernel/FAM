#ifndef __PROJ_MIS_H__
#define __PROJ_MIS_H__

#include "graph_kernel.hpp"
#include "vertex_table.hpp"
#include "graph_types.hpp"
#include <client_runtime.hpp>
#include <array>
#include <atomic>
#include <memory>

#include "communication_runtime.hpp"
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

namespace mis {
enum class MatchFlag : uint8_t { UNDECIDED, IN, OUT };

struct mis_vertex
{
  MatchFlag flag{ MatchFlag::UNDECIDED };
};

template<famgraph::Buffering b> struct mis_kernel
{
public:
  famgraph::Generic_ctx<mis::mis_vertex> c;
  uint32_t const divisor;

  mis_kernel(struct client_context &ctx)
    : c(ctx, b), divisor((*ctx.vm)["delta"].as<uint32_t>())
  {}

  void operator()()
  {
    auto const total_verts = c.num_vertices;
    auto const num_edges = c.num_edges;
    auto const idx = c.p.first.get();
    auto vtable = c.p.second.get();
    auto RDMA_area = c.RDMA_window.get();
    auto const edge_buf_size = c.edge_buf_size;
    auto *frontier = &c.frontierA;
    auto *next_frontier = &c.frontierB;

    tbb::blocked_range<uint32_t> const my_range(0, total_verts);
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t const delta = total_verts / divisor;
    uint32_t undecided = total_verts;
    tbb::combinable<uint32_t> n_decided;
    tbb::combinable<uint32_t> next_frontier_size;

    auto pull = [&](uint32_t const v, uint32_t *const edges, uint32_t const n) noexcept
    {
      if (vtable[v].flag == mis::MatchFlag::OUT) {
        assert(1 == 0);
        return;// Can this happen? ... No
      }

      mis::MatchFlag v_flag = mis::MatchFlag::IN;// Assume we are in

      for (uint32_t i = 0; i < n; ++i) {
        uint32_t w = edges[i];
        if (w < v) {// w has priority, what should we do?
          switch (vtable[w].flag) {
          case mis::MatchFlag::UNDECIDED:
            v_flag = mis::MatchFlag::UNDECIDED;
            break;
          case mis::MatchFlag::IN:
            v_flag = mis::MatchFlag::OUT;
            break;
          case mis::MatchFlag::OUT:
            break;
          }
          if (v_flag == mis::MatchFlag::OUT) { break; }
        }
      }

      if (v_flag == mis::MatchFlag::UNDECIDED) {
        next_frontier->set_bit(v);
        ++next_frontier_size.local();
      } else {
        // we must be in or out
        ++n_decided.local();
      }

      vtable[v].flag = v_flag;
    };

    tbb::combinable<uint32_t> zero_deg_nodes;
    tbb::parallel_for(my_range, [&](auto const &range) {
      for (uint32_t v = range.begin(); v < range.end(); ++v) {
        const uint32_t d = famgraph::get_num_edges(v, idx, total_verts, num_edges);
        if (d == 0) {
          vtable[v].flag = mis::MatchFlag::IN;
          ++zero_deg_nodes.local();
        }
      }
    });
    undecided -= zero_deg_nodes.combine(std::plus<uint32_t>{});

    uint32_t front_size = 0;
    while (undecided > 0) {
      if (front_size > 0) {// all undecided
        tbb::blocked_range<uint32_t> const f_range(0, end);
        famgraph::single_buffer::for_each_active_batch(
          *frontier, f_range, c, pull);
      }
      assert(front_size <= delta);
      end = std::min(total_verts, end + delta - front_size);
      if (start < end) {// break new ground
        tbb::blocked_range<uint32_t> const range(start, end);
        famgraph::single_buffer::for_each_range(
          range, RDMA_area, edge_buf_size, idx, c.context, pull);
      }
      start = end;
      undecided -= n_decided.combine(std::plus<uint32_t>{});
      n_decided.clear();

      front_size = next_frontier_size.combine(std::plus<uint32_t>{});
      next_frontier_size.clear();

      frontier->clear();
      std::swap(frontier, next_frontier);
    }
  }

  void print_result()
  {
    auto const total_verts = c.num_vertices;
    auto const vtable = c.p.second.get();
    tbb::blocked_range<uint32_t> const my_range(0, total_verts);
    BOOST_LOG_TRIVIAL(info) << "MIS Cardinality: " << get_cardinality(my_range, vtable);
  }

private:
  uint32_t get_cardinality(tbb::blocked_range<uint32_t> const &my_range,
    mis::mis_vertex *const vtable)
  {
    return tbb::parallel_reduce(my_range,
      static_cast<uint32_t>(0),
      [&vtable](auto const &r, auto init) {
        for (uint32_t i = r.begin(); i != r.end(); ++i) {
          init = vtable[i].flag == mis::MatchFlag::IN ? init + 1 : init;
        }
        return init;
      },
      [](auto const &a, auto const &t_b) { return a + t_b; });
  }
};
}// namespace mis
#endif// __PROJ_MIS_H__
