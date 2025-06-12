# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
project ("rt-tester")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)

# Include sub-projects.
add_subdirectory ("rt-tester")