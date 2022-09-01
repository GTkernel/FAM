#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using std::vector;

namespace {
template<typename T> auto num_elements(fs::path const &p)
{
  const auto file_size = fs::file_size(p);
  const auto n = file_size / sizeof(T);
  return n;
}

uint64_t max_degree(fs::path const &p)
{
  const uint64_t file_size = fs::file_size(p);
  const uint64_t num_vertices = file_size / sizeof(uint64_t);

  std::ifstream input(p.c_str(), std::ios::binary);
  if (!input) throw std::runtime_error("Couldn't open .idx file");

  uint64_t prev_off = 0;
  uint64_t max = 0;
  for (uint64_t i = 0; i < num_vertices; ++i) {
    uint64_t off;
    input.read(reinterpret_cast<char *>(&off), sizeof(uint64_t));
    if (static_cast<unsigned long>(input.gcount()) != sizeof(uint64_t)) {
      throw std::runtime_error("can't read index data");
    }
    auto edges = off - prev_off;
    max = std::max(edges, max);
    prev_off = off;

    // if (i % (num_vertices / 100) == 0) std::cout << i / (num_vertices / 100) << "%\n";
  }

  return max;
}
}// namespace

int main(int argc, char *argv[])
{
  try {
    po::options_description desc{ "Options" };
    desc.add_options()("help,h", "Help screen")(
      "infile,i", po::value<std::string>(), "input filepath")(
      "threads,t", po::value<uint64_t>()->default_value(40), "# of FG threads")(
      "vertsize", po::value<uint64_t>()->default_value(4), "B per vertex")(
      "edgewindow", po::value<uint64_t>()->default_value(1), "EW multiplier");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    }

    std::string file = vm["infile"].as<std::string>();
    fs::path index(file);

    BOOST_LOG_TRIVIAL(info) << "reading file " << index;

    if (fs::exists(index) && fs::is_regular_file(index)) {
      auto const threads = vm["threads"].as<uint64_t>();
      auto const B = vm["vertsize"].as<uint64_t>();
      auto const verts = num_elements<uint64_t>(index);
      auto const max = max_degree(index);
      auto const ew = vm["edgewindow"].as<uint64_t>();
      BOOST_LOG_TRIVIAL(info) << "|V| = " << verts;
      BOOST_LOG_TRIVIAL(info) << "Max outdegree = " << max;
      BOOST_LOG_TRIVIAL(info) << "# of threads = " << threads;
      BOOST_LOG_TRIVIAL(info) << "B per vertex = " << B;
      auto const total = (8 + B) * verts + 4 * max * threads * ew;
      BOOST_LOG_TRIVIAL(info) << "Total memory usage = "
                              << (static_cast<double>(total) / (1 << 30)) << "GB";
    } else {
      throw std::runtime_error("Input file not found");
    }
    return 0;
  } catch (const std::exception &ex) {
    std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
    return 1;
  }
}
