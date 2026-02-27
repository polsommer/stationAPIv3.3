add_library(
  stationapi
  Node.hpp
  NodeClient.cpp
  NodeClient.hpp
  Serialization.hpp
  StreamUtils.cpp
  StreamUtils.hpp
  StringUtils.cpp
  StringUtils.hpp)

target_include_directories(
  stationapi
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/externals/catch
         ${PROJECT_SOURCE_DIR}/externals/easyloggingpp ${UDPLIBRARY_SOURCE_DIR}
         ${Boost_INCLUDE_DIRS})

target_link_libraries(stationapi udplibrary)
