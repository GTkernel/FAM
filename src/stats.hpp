#ifndef STATS_H
#define STATS_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <oneapi/tbb.h>
#pragma GCC diagnostic pop

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include <iostream>
#include <utility>

namespace famgraph {
struct FG_stats
{
  tbb::enumerable_thread_specific<long> spin_time;// 1) spin time
  tbb::enumerable_thread_specific<long>
    function_time;// 2) time spent applying functions..
  tbb::enumerable_thread_specific<std::tuple<unsigned int, unsigned int, unsigned int>>
    wrs_verts_sends;

  long total_spin_time{ 0 };
  long total_function_time{ 0 };
  unsigned int wrs{ 0 };
  unsigned int verts{ 0 };
  unsigned int sends{ 0 };
};

// const? does combine mutate -- i think const ok
inline void print_stats_round(FG_stats const &stats)
{
  BOOST_LOG_TRIVIAL(debug) << "Spin Time(s): ";
  for (auto const &t : stats.spin_time) {
    BOOST_LOG_TRIVIAL(debug) << static_cast<double>(t) / 1000000000 << " ";
  }

  BOOST_LOG_TRIVIAL(debug) << "Function Time(s): ";
  for (auto const &t : stats.function_time) {
    BOOST_LOG_TRIVIAL(debug) << static_cast<double>(t) / 1000000000 << " ";
  }
  BOOST_LOG_TRIVIAL(debug) << "\n";

  BOOST_LOG_TRIVIAL(debug) << "WR / send: ";
  for (auto const &p : stats.wrs_verts_sends) {
    BOOST_LOG_TRIVIAL(debug) << static_cast<double>(std::get<0>(p))
                                  / static_cast<double>(std::get<2>(p))
                             << " ";
  }

  BOOST_LOG_TRIVIAL(debug) << "\n";
  BOOST_LOG_TRIVIAL(debug) << "verts / send: ";
  for (auto const &p : stats.wrs_verts_sends) {
    BOOST_LOG_TRIVIAL(debug) << static_cast<double>(std::get<1>(p))
                                  / static_cast<double>(std::get<2>(p))
                             << " ";
  }

  BOOST_LOG_TRIVIAL(debug) << "\n";
  BOOST_LOG_TRIVIAL(debug) << "\n";
}

inline void clear_stats_round(FG_stats &stats)
{
  for (auto &t : stats.spin_time) {
    stats.total_spin_time += t;
    t = 0;
  }

  for (auto &t : stats.function_time) {
    stats.total_function_time += t;
    t = 0;
  }

  for (auto &p : stats.wrs_verts_sends) {
    stats.wrs += std::get<0>(p);
    stats.verts += std::get<1>(p);
    stats.sends += std::get<2>(p);
    std::get<0>(p) = 0;
    std::get<1>(p) = 0;
    std::get<2>(p) = 0;
  }
}

inline void print_stats_summary(FG_stats const &stats)
{
  BOOST_LOG_TRIVIAL(info) << "Total Spin Time (s): "
                          << static_cast<double>(stats.total_spin_time) / 1000000000 / 10
                          << " Total Function Time (s) "
                          << static_cast<double>(stats.total_function_time) / 1000000000
                               / 10
                          << " WR's: " << stats.wrs << " sends: " << stats.sends
                          << std::endl;
}

inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result)
{
  result->tv_sec = a->tv_sec - b->tv_sec;
  result->tv_nsec = a->tv_nsec - b->tv_nsec;
  if (result->tv_nsec < 0) {
    --result->tv_sec;
    result->tv_nsec += 1000000000L;
  }
}
}// namespace famgraph

#endif// STATS_H
