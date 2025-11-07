
#!/usr/bin/env bash

# 1. 强制清理旧的构建目录
rm -rf build

# 2. (核心) 清理根目录可能存在的污染缓存
rm -f CMakeCache.txt
rm -rf CMakeFiles/

# 3. 创建并进入 build 目录
mkdir -p build || { echo "创建 build 目录失败"; exit 1; }
cd build || { echo "进入 build 目录失败"; exit 1; }

# 4. 运行 cmake（使用你修复了换行符 \ 之后的版本）
#    注意：斜杠后面一定要检查有没有空格歌或者其它看不见的字符，这里最容易出错，还不容易发现
cmake -DCMAKE_BUILD_TYPE=Release \
      -DGEOS_INCLUDE_DIR=/usr/local/geos/include \
      -DGEOS_LIBRARY=/usr/local/geos/lib/libgeos.so \
      -DBoost_INCLUDE_DIR=/usr/local/include \
      -DBoost_LIBRARY_DIR=/usr/local/lib \
      -DBoost_NO_SYSTEM_PATHS=ON \
      -DBoost_USE_MULTITHREADED=ON \
      -DBoost_USE_STATIC_LIBS=OFF \
      .. 2>&1 | tee cmake_error.log

# 5. 检查 CMake 是否成功
if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo "CMake 配置失败，请检查 cmake_error.log"
    exit 1
fi

# 6. 编译
make || { echo "编译失败"; exit 1; }

# 7. 返回根目录
cd .. || { echo "返回根目录失败"; exit 1; }

echo "构建成功！"
# cmake 指定目录时，中间不要出现任何注释，否则会报错
# !/usr/bin/env bash

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