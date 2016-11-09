cmake_minimum_required(VERSION 2.8)

set(PROJECT_NAME esp_mesh)
project(${PROJECT_NAME})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(public_includes
	${CMAKE_SOURCE_DIR}/include
	/usr/local/include
	/usr/include
)

file(GLOB mesh_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "../mesh/*.c")
add_library(mesh STATIC ${mesh_sources})

set(sources
	${CMAKE_SOURCE_DIR}/src/main.cpp
	${CMAKE_SOURCE_DIR}/src/mesh_platform.cpp
)

include_directories(${public_includes} ./src ../mesh )
add_executable(${PROJECT_NAME} ${sources})
target_link_libraries(${PROJECT_NAME} ev mesh)


