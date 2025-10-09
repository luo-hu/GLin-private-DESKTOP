#!/usr/bin/env bash
# if [[ "$#" -ne 0 && $1 == "debug" ]]
# then
#     mkdir -p build_debug;
#     cd build_debug;
#     cmake -DCMAKE_BUILD_TYPE=Debug ..;
# else
#     mkdir -p build;
#     cd build;
#     cmake -DCMAKE_BUILD_TYPE=Release ..;
# fi
# make;
# cd ..;

#!/usr/bin/env bash
# if [[ "$#" -ne 0 && $1 == "debug" ]]
# then
#     mkdir -p build_debug;
#     cd build_debug;
#     cmake -DCMAKE_BUILD_TYPE=Debug \
#           # 仅保留 GEOS 路径（手动安装的 GEOS 仍需指定）
#           -DGEOS_INCLUDE_DIR=/usr/local/include \
#           -DGEOS_LIBRARY=/usr/local/lib/libgeos.so \
#           ..;
# else
#     mkdir -p build;
#     cd build;
#     cmake -DCMAKE_BUILD_TYPE=Release \
#           -DGEOS_INCLUDE_DIR=/usr/local/include \
#           -DGEOS_LIBRARY=/usr/local/lib/libgeos.so \
#           ..;
# fi
# make;
# cd ..;



#!/usr/bin/env bash

# 处理发布模式（简化后，保留核心逻辑）
mkdir -p build || { echo "创建 build 目录失败"; exit 1; }
cd build || { echo "进入 build 目录失败"; exit 1; }

# CMake 配置：修正 Boost 变量名大小写，补充缺失参数
cmake -DCMAKE_BUILD_TYPE=Release \
      -DGEOS_INCLUDE_DIR=/usr/local/include \
      -DGEOS_LIBRARY=/usr/local/lib/libgeos.so \
      -DBoost_INCLUDE_DIR=/usr/local/boost-1.67/include \
      -DBoost_LIBRARY_DIR=/usr/local/boost-1.67/lib \
      -DBoost_NO_SYSTEM_PATHS=ON \
      -DBoost_USE_MULTITHREADED=ON \
      -DBoost_USE_STATIC_LIBS=OFF \
      .. 2>&1 | tee cmake_error.log;

# 编译（确保在 build 目录内）
make || { echo "编译失败"; exit 1; }

# 返回根目录
cd .. || { echo "返回根目录失败"; exit 1; }

#cmake 指定目录时，中间不要出现任何注释，否则会报错
#!/usr/bin/env bash

# 处理发布模式（简化后，保留核心逻辑）
# mkdir -p build || { echo "创建 build 目录失败"; exit 1; }
# cd build || { echo "进入 build 目录失败"; exit 1; }

# # CMake 配置：修正 Boost 变量名大小写，补充缺失参数
# cmake -DCMAKE_BUILD_TYPE=Release \
#       -DGEOS_INCLUDE_DIR=/usr/local/include \
#       -DGEOS_LIBRARY=/usr/local/lib/libgeos.so \
#       -DBoost_INCLUDE_DIR=/usr/local/boost-1.67/include \  # 修正：Boost_ 而非 BOOST_
#       -DBoost_LIBRARY_DIR=/usr/local/boost-1.67/lib \      # 修正：同上
#       -DBoost_NO_SYSTEM_PATHS=ON \                        # 保留，禁用系统路径
#       -DBoost_USE_MULTITHREADED=ON \                     # 保留，启用多线程
#       -DBoost_USE_STATIC_LIBS=OFF \                      # 补充：明确使用动态库
#       .. 2>&1 | tee cmake_error.log;  # 日志输出保留

# # 编译（确保在 build 目录内）
# make || { echo "编译失败"; exit 1; }

# # 返回根目录
# cd .. || { echo "返回根目录失败"; exit 1; }