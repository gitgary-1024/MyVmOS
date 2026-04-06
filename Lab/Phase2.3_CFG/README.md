# Phase 2.3: 完整反汇编图（CFG）开发

## 🎯 目标

构建完整的控制流图（Control Flow Graph, CFG），支持：
- 基本块（Basic Block）划分
- 控制流边（Edges）
- 递归反汇编所有可达代码
- 为后续优化提供基础

## 📋 开发步骤

### Step 1: 基础模板（当前阶段）
- [x] 创建项目结构
- [ ] 定义 BasicBlock 数据结构
- [ ] 定义 ControlFlowGraph 类
- [ ] 创建简化的 Mock VM
- [ ] 创建 DisassemblyTracker

### Step 2: 入口点追踪
- [ ] 实现 DisassemblyTracker
- [ ] 标记入口点和跳转目标
- [ ] 检测指令重叠

### Step 3: 基本块划分
- [ ] 实现基本块创建逻辑
- [ ] 识别基本块边界
- [ ] 构建指令列表

### Step 4: CFG 构建算法
- [ ] BFS 反汇编算法
- [ ] 添加控制流边
- [ ] 处理条件跳转的 fall-through

### Step 5: 集成测试
- [ ] 单元测试
- [ ] 集成到 Mock VM
- [ ] 性能基准测试

## 📁 文件结构

```
Phase2.3_CFG/
├── README.md
├── include/
│   ├── BasicBlock.h          # 基本块定义
│   ├── ControlFlowGraph.h    # CFG 主类
│   ├── DisassemblyTracker.h  # 入口点追踪器
│   └── MockVM.h              # 简化的 VM
├── src/
│   ├── ControlFlowGraph.cpp  # CFG 实现
│   └── DisassemblyTracker.cpp
├── tests/
│   └── test_cfg.cpp          # 单元测试
└── CMakeLists.txt
```

## 🔗 参考资料

- [Phase 2 设计文档](../../docs/Phase2_VM集成与反汇编优化设计.md)
- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [Capstone Engine](https://www.capstone-engine.org/)
