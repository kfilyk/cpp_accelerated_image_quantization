cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(cpp_project-k-means-image-quantization LANGUAGES CXX)
find_package(Boost 1.76 REQUIRED)
find_package(OpenCV REQUIRED)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -Wextra -std=c++2a")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined") # using undefined sanitizer
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")  # using unallocated address sanitizer
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")  # using thread sanitizer
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")

include_directories( ${OpenCV_INCLUDE_DIRS} )
add_executable(quantize_image ./app/quantize_image.cpp ./lib/thread_pool.cpp)
target_include_directories(quantize_image PUBLIC ${Boost_INCLUDE_DIRS}) # add boost
target_link_libraries(quantize_image ${Boost_LIBRARIES})
target_link_libraries(quantize_image ${OpenCV_LIBS} )
install(TARGETS quantize_image DESTINATION bin)
install(PROGRAMS demo DESTINATION bin)
