cmake_minimum_required(VERSION 3.5.0)
project(surena_choreonoid)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  sensor_msgs
  trajectory_planner
  choreonoid
  image_transport
  message_generation
  actionlib_msgs
  tf
  )

add_service_files(
  FILES
  bump.srv
)

generate_messages(
  DEPENDENCIES
  std_msgs
  sensor_msgs
  #std_srvs
)

include_directories(
  include/surena_choreonoid
  ${catkin_INCLUDE_DIRS}
)

file(GLOB SOURCES "src/BumpSensor.cpp" "src/Obstacle.cpp")
add_executable(bump_sensor ${SOURCES})
target_link_libraries(bump_sensor ${catkin_LIBRARIES})
add_dependencies(bump_sensor ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

add_executable(odom_to_robot_base_publisher src/robotBase_tf_publisher.cpp)
target_link_libraries(odom_to_robot_base_publisher ${catkin_LIBRARIES})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
    "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
    FORCE)
endif()

set(CHOREONOID_SKIP_QT_CONFIG true)



if(CHOREONOID_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${CHOREONOID_CXX_STANDARD})
  set(CMAKE_CXX_EXTENSIONS OFF)
else()
  add_compile_options(-std=c++14)
endif()

include_directories(${catkin_INCLUDE_DIRS})

add_subdirectory(src)


