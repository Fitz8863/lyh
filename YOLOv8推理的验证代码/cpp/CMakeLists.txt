cmake_minimum_required(VERSION 3.10)

project(rknn_yolov8_demo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread")


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ 3rdparty.out)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ utils.out)

# Use system OpenCV (ARM64 on RK3588)
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME}
    main.cc
    postprocess.cc
    yolo_detector.cc
    rknpu2/yolov8.cc
)

target_link_libraries(${PROJECT_NAME}
    imageutils
    fileutils
    imagedrawing
    ${LIBRKNNRT}
    ${OpenCV_LIBS}
    ${GST_LIBRARIES}
    dl
)


# message(STATUS "!!!!!!!!!!!CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

# if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
#     set(THREADS_PREFER_PTHREAD_FLAG ON)
#     find_package(Threads REQUIRED)
#     target_link_libraries(${PROJECT_NAME} Threads::Threads)
# endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBRKNNRT_INCLUDES}
    ${THREAD_INCLUDES}
)



