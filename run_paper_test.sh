#!/bin/bash

# GLIN论文标准实验框架 - 运行脚本

echo "=================================================="
echo "  GLIN论文标准实验框架"
echo "=================================================="
echo ""
echo "配置信息："
echo "  - 数据量: 100,000个真实几何对象"
echo "  - 查询窗口: 每个选择性100个"
echo "  - 选择性级别: 0.1%, 1%, 5%, 10%"
echo "  - 测试方法: 原始GLIN, GLIN-HF, Lite-AMF"
echo ""
echo "预计运行时间: 30分钟 - 2小时"
echo ""

# 切换到build目录
cd /home/lh/Software/GLin-private/build || {
    echo "❌ 错误：找不到build目录"
    exit 1
}

# 检查可执行文件是否存在
if [ ! -f "./test_glin_paper_standard" ]; then
    echo "⚠️  可执行文件不存在，开始编译..."
    make test_glin_paper_standard
    if [ $? -ne 0 ]; then
        echo "❌ 编译失败"
        exit 1
    fi
    echo "✅ 编译成功"
fi

# 询问用户运行方式
echo ""
echo "请选择运行方式："
echo "  1) 前台运行（直接显示输出）"
echo "  2) 后台运行（保存到results.log）"
echo "  3) 取消"
echo ""
read -p "请选择 [1-3]: " choice

case $choice in
    1)
        echo ""
        echo "🚀 开始前台运行..."
        echo ""
        ./test_glin_paper_standard
        ;;
    2)
        echo ""
        echo "🚀 开始后台运行，输出保存到 results.log"
        echo ""
        nohup ./test_glin_paper_standard > results.log 2>&1 &
        PID=$!
        echo "✅ 进程已启动，PID: $PID"
        echo ""
        echo "查看实时输出："
        echo "  tail -f results.log"
        echo ""
        echo "查看进程状态："
        echo "  ps -p $PID"
        echo ""
        echo "终止进程："
        echo "  kill $PID"
        echo ""
        echo "查看最终结果："
        echo "  grep -A 50 '📊 GLIN论文标准实验结果' results.log"
        ;;
    3)
        echo "已取消"
        exit 0
        ;;
    *)
        echo "❌ 无效选择"
        exit 1
        ;;
esac
