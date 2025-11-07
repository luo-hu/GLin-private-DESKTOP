# cmake_minimum_required(VERSION 3.12)
# project(glin)
# set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # find geos path

# # 强制指定GEOS路径
# # set(GEOS_INCLUDE_DIR /usr/include/geos CACHE PATH "GEOS include directory")
# # set(GEOS_CXX_LIBRARY /usr/lib/x86_64-linux-gnu/libgeos_c.so CACHE FILEPATH "GEOS C++ library")

# # -------------------------- 新增：显式指定 Boost 路径 --------------------------
# set(Boost_NO_SYSTEM_PATHS ON)  # 禁用系统默认路径搜索，避免与其他 Boost 版本冲突

# # 手动安装的 Boost 1.67.0 头文件目录
# set(BOOST_INCLUDE_DIR "/usr/local/boost-1.67/include" CACHE PATH "Boost include directory")
# # 手动安装的 Boost 1.67.0 库文件目录
# set(BOOST_LIBRARY_DIR "/usr/local/boost-1.67/lib" CACHE PATH "Boost library directory")
# # 告诉 CMake 优先使用指定的路径搜索 Boost
# set(Boost_INCLUDE_DIRS ${BOOST_INCLUDE_DIR})
# set(Boost_LIBRARY_DIRS ${BOOST_LIBRARY_DIR})
# # 4. 显式指定 Boost 版本（避免版本误判）
# set(Boost_VERSION 1.67.0)
# # ------------------------------------------------------------------------------
# # 之后再执行 find_package
# find_package(Boost REQUIRED COMPONENTS system)
# # 在 find_package(Boost) 之后添加
# message("Boost 头文件路径: ${Boost_INCLUDE_DIRS}")
# message("Boost 库文件路径: ${Boost_LIBRARY_DIRS}")
# message("Boost 系统库: ${Boost_SYSTEM_LIBRARY}")

# if(APPLE) # This has to be manually setup
# #    set(Boost_INCLUDE_DIR /usr/local/Cellar/boost/1.75.0/include)
#     set(CMAKE_FIND_FRAMEWORK LAST)
#     set(CMAKE_FIND_APPBUNDLE LAST)
# endif()

# set(Boost_USE_MULTITHREADED ON)
# # FindGEOS.cmake is copied from https://github.com/valhalla/valhalla/blob/master/cmake/FindGEOS.cmake
# find_package(GEOS REQUIRED COMPONENTS geos_cxx)# if not find geo, cannot compile find geos.cmake
# find_package(Boost REQUIRED COMPONENTS system)
# set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -std=c++14")  
# # 在cmake/FindGEOS.cmake中添加以下导出逻辑
# if(GEOS_FOUND AND NOT TARGET GEOS::geos_cxx)
#     add_library(GEOS::geos_cxx UNKNOWN IMPORTED)
#     set_target_properties(GEOS::geos_cxx PROPERTIES
#         IMPORTED_LOCATION "${GEOS_LIBRARY}"
#         INTERFACE_INCLUDE_DIRECTORIES "${GEOS_INCLUDE_DIR}"
#     )
# endif()
# # SIMD
# if(MSVC)
#     set(CMAKE_CXX_FLAGS "/O2 /arch:AVX2 /W1 /EHsc")
# elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
#     set(CMAKE_CXX_FLAGS "-O3 -xHost")
# else()
#     # clang and gcc
#     set(CMAKE_CXX_FLAGS "-O3 -march=native -Wall -Wextra")
# endif()


# # # 添加GEOS头文件目录到全局包含路径
# #include_directories(${GEOS_INCLUDE_DIR})
# include_directories(${GEOS_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})

# add_executable(test_glin test/unittest_main.cpp glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
# target_link_libraries(test_glin PRIVATE ${GEOS_LIBRARY} ${Boost_SYSTEM_LIBRARY})
# target_include_directories(test_glin PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# add_executable(test_glin_piece test/unittest_main.cpp glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
# #target_compile_definitions(test_glin_piece PRIVATE PIECE) 这一行代码要去掉，否则test_glin_piece 在make时会出错
# target_link_libraries(test_glin_piece PRIVATE ${GEOS_LIBRARY} ${Boost_SYSTEM_LIBRARY})
# target_include_directories(test_glin_piece PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# # add_executable(test_program test_program.cpp  ./test/unittest_main.cpp ./glin/hilbert/hilbert.h ./glin/hilbert/hilbert.cpp)#./test/unittest_main.cpp
# # #target_compile_definitions(test_glin_piece PRIVATE PIECE) 这一行代码要去掉，否则test_glin_piece 在make时会出错
# # target_link_libraries(test_program PRIVATE 
# #     ${GEOS_LIBRARIES}  # 使用复数形式变量
# #     Boost::system
# # )
# # target_link_libraries(test_program PRIVATE 
# #     /usr/local/lib/libgeos.so  # 直接指定C++库路径
# #     ${Boost_SYSTEM_LIBRARY}
# # )
# add_executable(my_glin_test indexBuildAndFindByMe.cpp ./glin/hilbert/hilbert.h ./glin/hilbert/hilbert.cpp)
# # target_link_libraries(my_glin_test PRIVATE 
# #   ${GEOS_CXX_LIBRARY}  # 现在应该包含C++接口
# #   ${Boost_SYSTEM_LIBRARY}
# # )
# target_link_libraries(my_glin_test PRIVATE 
#     ${GEOS_LIBRARIES}  # 使用复数形式变量
#     Boost::system
# )
# target_link_libraries(my_glin_test PRIVATE 
#     /usr/local/lib/libgeos.so  # 直接指定C++库路径
#     #/usr/lib/x86_64-linux-gnu/libgeos.so  # 直接指定C++库路径

#     ${Boost_SYSTEM_LIBRARY}
# )

# enable_testing()
# add_test(test_glin test_glin_piece) #running test while compile




cmake_minimum_required(VERSION 3.12)
project(glin)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # 保持不变

# -------------------------- 1. 仅在 find_package(Boost) 前设置路径（只执行一次）--------------------------
set(Boost_NO_SYSTEM_PATHS ON)  # 禁用系统默认路径，只认手动指定的 1.67.0
set(BOOST_INCLUDE_DIR "/usr/local/include" CACHE PATH "Boost 头文件目录")
set(BOOST_LIBRARY_DIR "/usr/local/lib" CACHE PATH "Boost 库文件目录")
set(Boost_INCLUDE_DIRS ${BOOST_INCLUDE_DIR})
set(Boost_LIBRARY_DIRS ${BOOST_LIBRARY_DIR})
set(Boost_VERSION 1.67.0)  # 显式指定版本
set(Boost_USE_STATIC_LIBS OFF)  # 明确使用动态库（你的系统中存在 libboost_system.so）
set(Boost_USE_MULTITHREADED ON)  # 启用多线程（Boost 必需）

# 告诉 find_package(GEOS) 去哪里找
set(GEOS_INCLUDE_DIR "/usr/local/geos/include" CACHE PATH "GEOS 头文件目录")
set(GEOS_LIBRARY_DIR "/usr/local/geos/lib" CACHE PATH "GEOS 库文件目录")
set(GEOS_INCLUDE_DIR ${GEOS_INCLUDE_DIR})
set(GEOS_LIBRARY_DIR ${GEOS_LIBRARY})
# --------------------------------------------------------------------------------------------------------

# 2. 只调用一次 find_package(Boost)，确保路径生效
find_package(Boost REQUIRED COMPONENTS system)
# 打印日志，验证路径是否正确（方便调试）
message("=== Boost 配置验证 ===")
message("Boost 头文件路径: ${Boost_INCLUDE_DIRS}")  # 应输出 /usr/local/boost-1.67/include
message("Boost 库文件路径: ${Boost_LIBRARY_DIRS}")  # 应输出 /usr/local/boost-1.67/lib
message("Boost System 库: ${Boost_SYSTEM_LIBRARY}")  # 应输出 /usr/local/boost-1.67/lib/libboost_system.so
message("======================")

# 3. 处理 Apple 系统（保持不变）
if(APPLE)
    set(CMAKE_FIND_FRAMEWORK LAST)
    set(CMAKE_FIND_APPBUNDLE LAST)
endif()

# 4. 查找 GEOS（保持不变，但确保在 Boost 之后，避免干扰）
find_package(GEOS REQUIRED COMPONENTS geos_cxx)
# 打印 GEOS 日志，验证是否找到
message("=== GEOS 配置验证 ===")
message("GEOS 头文件路径: ${GEOS_INCLUDE_DIR}")  # 应输出 /usr/local/include
message("GEOS 库文件路径: ${GEOS_LIBRARY}")      # 应输出 /usr/local/lib/libgeos.so
message("======================")

# 5. 编译选项（保持不变，但注意不要重复设置 CMAKE_CXX_FLAGS）
set(CMAKE_CXX_STANDARD 14)
# 合并编译选项，避免重复覆盖（原代码中两次设置 CMAKE_CXX_FLAGS，这里合并）
set(CMAKE_CXX_FLAGS "-O3 -march=native -Wall -Wextra -std=c++14")  
if(MSVC)
    set(CMAKE_CXX_FLAGS "/O2 /arch:AVX2 /W1 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set(CMAKE_CXX_FLAGS "-O3 -xHost")
endif()

# 6. GEOS 目标定义（保持不变）
if(GEOS_FOUND AND NOT TARGET GEOS::geos_cxx)
    add_library(GEOS::geos_cxx UNKNOWN IMPORTED)
    set_target_properties(GEOS::geos_cxx PROPERTIES
        IMPORTED_LOCATION "${GEOS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GEOS_INCLUDE_DIR}"
    )
endif()

# 7. 全局头文件目录（保持不变）
include_directories(${GEOS_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})

# 8. 目标 1: test_glin（保持不变，但确保链接正确）
add_executable(test_glin test/unittest_main.cpp glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
target_link_libraries(test_glin PRIVATE ${GEOS_LIBRARY} ${Boost_SYSTEM_LIBRARY})
target_include_directories(test_glin PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# 9. 目标 2: test_glin_piece（保持不变）
add_executable(test_glin_piece test/unittest_main.cpp glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
target_link_libraries(test_glin_piece PRIVATE ${GEOS_LIBRARY} ${Boost_SYSTEM_LIBRARY})
target_include_directories(test_glin_piece PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# 10. 目标 3: my_glin_test（简化链接，避免重复）
add_executable(my_glin_test indexBuildAndFindByMe.cpp ./glin/hilbert/hilbert.h ./glin/hilbert/hilbert.cpp)
# 合并重复的 target_link_libraries，避免冗余
target_link_libraries(my_glin_test PRIVATE 
    ${GEOS_LIBRARY}  # 直接使用 GEOS_LIBRARY（已验证路径正确）
    ${Boost_SYSTEM_LIBRARY}  # 直接使用 Boost 动态库
)
target_include_directories(my_glin_test PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# 10. 目标 3: （简化链接，避免重复）
add_executable(tmp tmp.cpp ./glin/hilbert/hilbert.h ./glin/hilbert/hilbert.cpp)
# 合并重复的 target_link_libraries，避免冗余
target_link_libraries(tmp PRIVATE 
    ${GEOS_LIBRARY}  # 直接使用 GEOS_LIBRARY（已验证路径正确）
    ${Boost_SYSTEM_LIBRARY}  # 直接使用 Boost 动态库
)
target_include_directories(tmp PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# 10. 目标 3: （简化链接，避免重复）
add_executable(tmp1 tmp1.cpp ./glin/hilbert/hilbert.h ./glin/hilbert/hilbert.cpp)
# 合并重复的 target_link_libraries，避免冗余
target_link_libraries(tmp1 PRIVATE 
    ${GEOS_LIBRARY}  # 直接使用 GEOS_LIBRARY（已验证路径正确）
    ${Boost_SYSTEM_LIBRARY}  # 直接使用 Boost 动态库
)
target_include_directories(tmp1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")


#    目标 4： src/becnch/main.cpp
    # 添加基准测试可执行文件
add_executable(benchmark src/benchmark/main.cpp src/benchmark/utils.h src/benchmark/flags.h src/benchmark/zipf.h glin/hilbert/hilbert.cpp)
target_link_libraries(benchmark PRIVATE ${GEOS_LIBRARY} ${Boost_SYSTEM_LIBRARY})
target_include_directories(benchmark PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${CMAKE_CURRENT_SOURCE_DIR}/src/benchmark;${GEOS_INCLUDE_DIR};${Boost_INCLUDE_DIRS}>"
)


# 目标 5：src/benchmark/dataGen.cpp
add_executable(dataGen dataGen.cpp glin/glin.h glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
# 合并重复的 target_link_libraries，避免冗余
target_link_libraries(dataGen PRIVATE 
    ${GEOS_LIBRARY}  # 直接使用 GEOS_LIBRARY（已验证路径正确）
    ${Boost_SYSTEM_LIBRARY}  # 直接使用 Boost 动态库
)
target_include_directories(dataGen PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${GEOS_INCLUDE_DIR};${Boost_INCLUDE_DIRS}>")

# 目标 6：src/benchmark/dataGen1.cpp
add_executable(dataGen1 dataGen1.cpp glin/glin.h glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
# 合并重复的 target_link_libraries，避免冗余
target_link_libraries(dataGen1 PRIVATE 
    ${GEOS_LIBRARY}  # 直接使用 GEOS_LIBRARY（已验证路径正确）
    ${Boost_SYSTEM_LIBRARY}  # 直接使用 Boost 动态库
)
target_include_directories(dataGen1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${GEOS_INCLUDE_DIR};${Boost_INCLUDE_DIRS}>")


# 11. 测试配置（保持不变）
enable_testing()
add_test(test_glin test_glin_piece dataGen)




# cmake_minimum_required(VERSION 3.12)
# project(glin)
# set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # find geos path

# # 强制指定GEOS路径
# # set(GEOS_INCLUDE_DIR /usr/include/geos CACHE PATH "GEOS include directory")
# # set(GEOS_CXX_LIBRARY /usr/lib/x86_64-linux-gnu/libgeos_c.so CACHE FILEPATH "GEOS C++ library")

# # -------------------------- 显式指定 Boost 路径 --------------------------
# if(DEFINED BOOST_INCLUDE_DIR AND DEFINED BOOST_LIBRARY_DIR)
#     set(Boost_NO_SYSTEM_PATHS ON)  # 禁用系统默认路径搜索
#     set(Boost_INCLUDE_DIRS ${BOOST_INCLUDE_DIR})
#     set(Boost_LIBRARY_DIRS ${BOOST_LIBRARY_DIR})
#     set(CMAKE_PREFIX_PATH ${BOOST_LIBRARY_DIR}/.. ${CMAKE_PREFIX_PATH})
# endif()

# if(APPLE) # This has to be manually setup
#     # set(Boost_INCLUDE_DIR /usr/local/Cellar/boost/1.75.0/include)
#     set(CMAKE_FIND_FRAMEWORK LAST)
#     set(CMAKE_FIND_APPBUNDLE LAST)
# endif()

# set(Boost_USE_MULTITHREADED ON)

# # FindGEOS.cmake is copied from https://github.com/valhalla/valhalla/blob/master/cmake/FindGEOS.cmake
# find_package(GEOS REQUIRED COMPONENTS geos_cxx)
# # 只查找一次 Boost
# find_package(Boost REQUIRED COMPONENTS system)

# set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -std=c++14")  

# # 在cmake/FindGEOS.cmake中添加以下导出逻辑
# if(GEOS_FOUND AND NOT TARGET GEOS::geos_cxx)
#     add_library(GEOS::geos_cxx UNKNOWN IMPORTED)
#     set_target_properties(GEOS::geos_cxx PROPERTIES
#         IMPORTED_LOCATION "${GEOS_LIBRARY}"
#         INTERFACE_INCLUDE_DIRECTORIES "${GEOS_INCLUDE_DIR}"
#     )
# endif()

# # SIMD
# if(MSVC)
#     set(CMAKE_CXX_FLAGS "/O2 /arch:AVX2 /W1 /EHsc")
# elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
#     set(CMAKE_CXX_FLAGS "-O3 -xHost")
# else()
#     # clang and gcc
#     set(CMAKE_CXX_FLAGS "-O3 -march=native -Wall -Wextra")
# endif()

# # 添加包含目录
# include_directories(${GEOS_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})

# add_executable(test_glin test/unittest_main.cpp glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
# target_link_libraries(test_glin PRIVATE ${GEOS_LIBRARY} ${Boost_SYSTEM_LIBRARY})
# target_include_directories(test_glin PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# add_executable(test_glin_piece test/unittest_main.cpp glin/hilbert/hilbert.h glin/hilbert/hilbert.cpp)
# # target_compile_definitions(test_glin_piece PRIVATE PIECE) 这一行代码要去掉，否则test_glin_piece 在make时会出错
# target_link_libraries(test_glin_piece PRIVATE ${GEOS_LIBRARY} ${Boost_SYSTEM_LIBRARY})
# target_include_directories(test_glin_piece PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core;${Boost_INCLUDE_DIRS}>")

# add_executable(my_glin_test indexBuildAndFindByMe.cpp ./glin/hilbert/hilbert.h ./glin/hilbert/hilbert.cpp)
# target_link_libraries(my_glin_test PRIVATE 
#     ${GEOS_LIBRARY}
#     ${Boost_SYSTEM_LIBRARY}
# )

# enable_testing()
# add_test(test_glin test_glin_piece) # running test while compile