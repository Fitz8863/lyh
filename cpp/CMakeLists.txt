cmake_minimum_required(VERSION 3.10)

project(rknn_yolov8_demo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread")

# 支持debug
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ 3rdparty.out)

# Use system OpenCV (ARM64 on RK3588)
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME}
    main.cc
    postprocess.cc
    yolo_detector.cc
    rknpu2/yolov8.cc
    utils/file_utils.c
    utils/image_drawing.c
    utils/image_utils.cc
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/librga/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stb_image
    ${LIBRKNNRT_INCLUDES}
    ${THREAD_INCLUDES}
)

target_link_libraries(${PROJECT_NAME}
    ${LIBRKNNRT}
    ${OpenCV_LIBS}
    ${GST_LIBRARIES}
    ${LIBRGA}
    dl
)
