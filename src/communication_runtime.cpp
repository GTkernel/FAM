#include <infiniband/verbs.h>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <tbb/concurrent_queue.h>

#pragma GCC diagnostic pop

#include "communication_runtime.hpp"
#include "graph_types.hpp"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include <client_runtime.hpp>
#include <connection_utils.hpp>

void famgraph::comm_runtime_worker2(std::vector<struct rdma_cm_id *> &cm_ids,
  famgraph::application *app) noexcept
{
  struct ibv_cq *cq;
  struct ibv_wc wc[famgraph::WC_POLL_WINDOW];
  constexpr unsigned long batch = 1 << 12;
  unsigned long n_comp = 0;

  while (!app->should_stop) {
    for (unsigned long iter = 0; iter < batch; ++iter) {
      for (auto id : cm_ids) {
        cq = id->send_cq;
        if (int n = ibv_poll_cq(cq, famgraph::WC_POLL_WINDOW, wc)) {
          for (int i = 0; i < n; ++i) {
            n_comp += static_cast<unsigned long>(n);
            if (wc[i].status != IBV_WC_SUCCESS) {
              BOOST_LOG_TRIVIAL(fatal) << "poll_cq: status is not IBV_WC_SUCCESS";
              return;
            }
          }
        }
      }
    }
  }
  BOOST_LOG_TRIVIAL(info) << "Comm thread exiting. n_comp = " << n_comp;
}
