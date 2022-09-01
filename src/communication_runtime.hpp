#ifndef __PROJ_COMMUNICATION_RUNTIME_H__
#define __PROJ_COMMUNICATION_RUNTIME_H__

#include <rdma/rdma_cma.h>
#include "graph_types.hpp"
#include <vector>
#include <build_options.hpp>

namespace famgraph {

inline constexpr uint32_t WR_WINDOW_SIZE = famgraph::build_options::opt_wr_window;
inline constexpr uint32_t NULL_VERT = 0xFFFFFFFF;
inline constexpr uint32_t cacheline_size = 64;// in bytes
inline constexpr uint32_t WC_POLL_WINDOW = 100;
inline constexpr uint32_t SINGLE_BUFFER = 1;
inline constexpr uint32_t DOUBLE_BUFFER = 2;

void comm_runtime_worker2(std::vector<struct rdma_cm_id *> &cm_ids,
  application *app) noexcept;
}// namespace famgraph

#endif// __PROJ_COMMUNICATION_RUNTIME_H__
