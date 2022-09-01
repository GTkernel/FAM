find_package(Boost REQUIRED COMPONENTS program_options log log_setup)
find_package(TBB REQUIRED)

# FG Compile Time Options
set(OPT_WR_WINDOW 40 CACHE STRING "WR Window Size")
set(OPT_COALESCE_GAP 5 CACHE STRING "vertex gap before splitting new WR's")
option(USE_TIMING_INSTRUMENTATION "Measure spin and function timing" On)
option(VERTEX_COALESCING "Adjacency list coalescing." On)
configure_file("build_options.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/build_options.hpp")

add_library(FAMGraph
  vertex_table.cpp
  connection_utils.cpp
  communication_runtime.cpp
  server_runtime.cpp
  client_runtime.cpp
  )

target_include_directories(FAMGraph
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ) 

target_link_libraries(FAMGraph PRIVATE
  project_options
  project_warnings
  Boost::program_options
  Boost::log
  Boost::log_setup
  TBB::tbb
  rdmacm
  ibverbs
  pthread
  numa)

target_compile_definitions(FAMGraph PRIVATE BOOST_LOG_DYN_LINK)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE
  project_options
  project_warnings
  FAMGraph
  Boost::program_options
  Boost::log
  Boost::log_setup
  )
target_compile_definitions(main PRIVATE BOOST_LOG_DYN_LINK)
