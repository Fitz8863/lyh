cmake_minimum_required(VERSION 3.4.1)

project(main)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ 3rdparty.out)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ utils.out)


find_package(OpenCV REQUIRED)

find_package(yaml-cpp REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0)

find_library(MQTT_LIBRARY NAMES paho-mqtt3a paho-mqtt3c paho-mqtt3as paho-mqtt3cs PATHS /usr/local/lib)
if(NOT MQTT_LIBRARY)
    message(FATAL_ERROR "MQTT library not found")
endif()

add_executable(main
    src/main.cc
    src/camera_status.cc
    src/capture_thread.cc
    src/publisher_thread.cc
    src/yolo_detector.cc
    src/rknpu2/yolov8.cc
    src/postprocess.cc
)

add_executable(camera_capture_helper
    src/camera_capture_helper.cc
)

include_directories(${GST_INCLUDE_DIRS})

target_link_libraries(main
    imageutils
    fileutils
    imagedrawing
    ${LIBRKNNRT}
    ${OpenCV_LIBS}
    ${MQTT_LIBRARY}
    paho-mqttpp3
    yaml-cpp
    ${GST_LIBRARIES}
)

target_link_libraries(camera_capture_helper
    ${OpenCV_LIBS}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/thread_pool
    ${LIBRKNNRT_INCLUDES}
    ${THREAD_INCLUDES}
)
