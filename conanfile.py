from conans import ConanFile

class FAMGraph(ConanFile):
    # Note: options are copied from CMake boolean options.
    # When turned off, CMake sometimes passes them as empty strings.
    options = {
    }
    name = "FAMGraph"
    version = "0.1"
    # "boost/1.77.0",
    requires = (
        "catch2/2.13.7",
    )
    generators = "cmake", "gcc", "txt", "cmake_find_package"
