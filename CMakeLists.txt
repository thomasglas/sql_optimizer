cmake_minimum_required(VERSION 3.0)

project(sqlOptimizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE DEBUG)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADDITIONAL_CFLAGS} -g")

include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/src/postgres/include/)
include_directories(${CMAKE_SOURCE_DIR}/vendor/)
include_directories(${CMAKE_SOURCE_DIR}/src/)

set(SRC_CC
    "${CMAKE_SOURCE_DIR}/src/optimizer/deparse_ra_to_sql.cc"
    "${CMAKE_SOURCE_DIR}/src/optimizer/parse_sql_to_ra.cc"
    "${CMAKE_SOURCE_DIR}/src/optimizer/ra_tree.cc"
    "${CMAKE_SOURCE_DIR}/src/optimizer/relational_algebra.cc"
)

add_library(optimizer STATIC ${SRC_CC})
target_link_libraries(optimizer ${CMAKE_SOURCE_DIR}/libpg_query.a -pthread)

add_executable(sqlOptimizer optimizer/optimizeSQL.cc)
target_link_libraries(sqlOptimizer optimizer)