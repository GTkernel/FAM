#include <server_runtime.hpp>
#include <client_runtime.hpp>

#include <iostream>
#include <stdexcept>

#include <boost/program_options.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

namespace po = boost::program_options;

void logging_init(boost::log::v2_mt_posix::trivial::severity_level level)
{
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= level);
}

int main(int argc, char *argv[])
{
  try {
    po::options_description desc{ "Options" };
    desc.add_options()("help,h", "Help screen")("verbose,v", "verbose")(
      "mode,m", po::value<std::string>(), "client or server mode")("server-addr,a",
      po::value<std::string>()->default_value("192.168.12.2"),
      "Server's IPoIB addr")("port,p",
      po::value<std::string>()->default_value("12345"),
      "server port")("indexfile,i", po::value<std::string>(), "path to .idx file")(
      "edgefile,e", po::value<std::string>(), "path to .adj file")("kernel,k",
      po::value<std::string>()->default_value("print_graph"),
      "kernel to run")("ofile,o",
      po::value<std::string>()->default_value("/home/username/graphs/kernel-out.txt"),
      "path to output file")("threads,t",
      po::value<unsigned long>()->default_value(0),
      "Number of Worker threads. 0 for all available cores")(
      "print-table", "Print full vertex table after kernel")(
      "kcore-k", po::value<uint32_t>()->default_value(100), "The k in k-core")(
      "delta", po::value<uint32_t>()->default_value(25), "delta step divisor for MIS")(
      "start-vertex", po::value<uint32_t>()->default_value(1), "start vertex for bfs")(
      "hp", "use huge pages")("double-buffer", "use double buffering")("edgewindow",
      po::value<uint32_t>()->default_value(1),
      "how many times larger than max outdegree the edgewindow should be")(
      "no-numa-bind", "don't do numa bind");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    auto const level = [&]() {
      if (vm.count("verbose"))
        return boost::log::trivial::debug;
      else
        return boost::log::trivial::info;
    }();

    logging_init(level);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    }

    if (!vm.count("mode")
        && (!(vm["mode"].as<std::string>() == "client"
              || vm["mode"].as<std::string>() == "server"))) {
      throw po::validation_error(po::validation_error::invalid_option_value, "mode");
    }

    if (vm["mode"].as<std::string>() == "client") {
      run_client(vm);
    } else /* server mode */ {
      run_server(vm);
    }

    return 0;
  } catch (const std::exception &ex) {
    std::cout << "Caught Runtime Exception: " << ex.what() << std::endl;
    return 1;
  }
}
