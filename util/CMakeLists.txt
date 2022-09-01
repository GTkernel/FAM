find_package(Boost REQUIRED COMPONENTS program_options log log_setup)
find_package(TBB REQUIRED)
find_package(oneDPL REQUIRED)

add_executable(util main.cpp)
target_link_libraries(util PRIVATE
  project_options
  project_warnings
  Boost::program_options
  Boost::log
  Boost::log_setup
  oneDPL
  )

target_compile_definitions(util PRIVATE BOOST_LOG_DYN_LINK)

add_executable(maxdegree maxd.cpp)
target_link_libraries(maxdegree PRIVATE
  project_options
  project_warnings
  Boost::program_options
  Boost::log
  Boost::log_setup
  oneDPL
  )

target_compile_definitions(maxdegree PRIVATE BOOST_LOG_DYN_LINK)

add_executable(fg2gr fg2gr.cpp)
target_link_libraries(fg2gr PRIVATE
  project_options
  project_warnings
  Boost::program_options
  Boost::log
  Boost::log_setup
  )

target_compile_definitions(fg2gr PRIVATE BOOST_LOG_DYN_LINK)

add_executable(compare_gr compare_gr.cpp)
target_link_libraries(compare_gr PRIVATE
  project_options
  project_warnings
  Boost::program_options
  Boost::log
  Boost::log_setup
  )

target_compile_definitions(compare_gr PRIVATE BOOST_LOG_DYN_LINK)

add_executable(memusage memusage.cpp)
target_link_libraries(memusage PRIVATE
  project_options
  project_warnings
  Boost::program_options
  Boost::log
  Boost::log_setup
  oneDPL
  )

target_compile_definitions(memusage PRIVATE BOOST_LOG_DYN_LINK)
