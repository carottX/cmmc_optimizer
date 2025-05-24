//
// Created by hby on 22-12-2.
//

#ifndef CODE_DATAFLOW_ANALYSIS_H
#define CODE_DATAFLOW_ANALYSIS_H

#include <IR.h>

//// ============================ 数据流分析 (Dataflow Analysis) ============================

typedef struct DataflowAnalysis DataflowAnalysis; // 数据流分析结构体的前向声明

typedef void *Fact; // 数据流事实的通用类型，具体类型由特定的分析定义

/**
 * @brief 通用数据流分析的虚函数表结构体。
 * 定义了数据流分析所需的一组通用操作。
 * 具体的分析（如活跃变量分析、常量传播等）会提供这些操作的实现。
 */
struct DataflowAnalysis {
    struct {
        /**
         * @brief 析构（清理）数据流分析实例占用的资源。
         * @param t 指向 DataflowAnalysis 实例的指针。
         */
        void (*teardown) (DataflowAnalysis *t);

        /**
         * @brief 判断当前分析是前向分析还是后向分析。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @return 如果是前向分析则返回 true，否则返回 false。
         */
        bool (*isForward) (DataflowAnalysis *t);

        /**
         * @brief 创建并返回边界条件下的数据流事实 (Boundary Fact)。
         * 对于前向分析，通常是入口基本块的OUT集合；
         * 对于后向分析，通常是出口基本块的IN集合。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param func 当前正在分析的 IR_function。
         * @return 指向新创建的 Fact 的指针。
         */
        Fact *(*newBoundaryFact) (DataflowAnalysis *t, IR_function *func);

        /**
         * @brief 创建并返回数据流事实的初始值 (Initial Fact)。
         * 用于初始化非边界基本块的 IN/OUT 集合。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @return 指向新创建的 Fact 的指针。
         */
        Fact *(*newInitialFact) (DataflowAnalysis *t);

        /**
         * @brief 设置指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Fact 的指针。
         */
        void (*setInFact) (DataflowAnalysis *t, IR_block *blk, Fact *fact);

        /**
         * @brief 设置指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Fact 的指针。
         */
        void (*setOutFact) (DataflowAnalysis *t, IR_block *blk, Fact *fact);

        /**
         * @brief 获取指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Fact 的指针。
         */
        Fact *(*getInFact) (DataflowAnalysis *t, IR_block *blk);

        /**
         * @brief 获取指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Fact 的指针。
         */
        Fact *(*getOutFact) (DataflowAnalysis *t, IR_block *blk);

        /**
         * @brief 执行 meet 操作，将一个数据流事实 (fact) 合并到另一个数据流事实 (target)。
         * meet 操作的具体定义（如并集、交集）取决于具体的分析。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param fact 指向源 Fact 的指针。
         * @param target 指向目标 Fact 的指针 (此 Fact 会被修改)。
         * @return 如果 target Fact 因 meet 操作发生改变则返回 true，否则返回 false。
         */
        bool (*meetInto) (DataflowAnalysis *t, Fact *fact, Fact *target);

        /**
         * @brief 执行传递函数，根据输入数据流事实 (in_fact) 计算输出数据流事实 (out_fact)。
         * 此函数模拟了基本块内语句对数据流事实的影响。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param block 指向当前处理的 IR_block 的指针。
         * @param in_fact 指向输入 Fact 的指针。
         * @param out_fact 指向输出 Fact 的指针 (此 Fact 会被计算和修改)。
         * @return 如果 out_fact 因传递函数发生改变则返回 true，否则返回 false。
         */
        bool (*transferBlock) (DataflowAnalysis *t, IR_block *block, Fact *in_fact, Fact *out_fact);

        /**
         * @brief 打印数据流分析的结果。
         * 通常用于调试，显示每个基本块的 IN 和 OUT 集合。
         * @param t 指向 DataflowAnalysis 实例的指针。
         * @param func 当前分析的 IR_function。
         */
        void (*printResult) (DataflowAnalysis *t, IR_function *func);
    } const *vTable; // 指向虚函数表的指针
};

//// Fact (数据流事实的具体类型定义)

// set (基于集合的数据流事实)
// 例如，活跃变量分析使用 IR_var 的集合作为 Fact。

DEF_SET(IR_var) // 定义 IR_var 类型的集合 (Set_IR_var)
typedef Set_IR_var *Set_ptr_IR_var; // 指向 Set_IR_var 的指针类型
DEF_MAP(IR_block_ptr, Set_ptr_IR_var) // 定义从 IR_block_ptr 到 Set_ptr_IR_var 的映射

//// ============================ 优化 (Optimize) ============================

/**
 * @brief 移除函数中标记为死代码的基本块。
 * @param func 指向要处理的 IR_function 的指针。
 */
extern void remove_dead_block(IR_function *func);

/**
 * @brief 移除基本块中标记为死代码的语句。
 * @param blk 指向要处理的 IR_block 的指针。
 */
extern void remove_dead_stmt(IR_block *blk);

/**
 * @brief 使用迭代算法执行数据流分析。
 * @param t 指向 DataflowAnalysis 实例的指针 (具体分析的实例)。
 * @param func 指向要分析的 IR_function 的指针。
 */
extern void iterative_solver(DataflowAnalysis *t, IR_function *func);

/**
 * @brief 使用工作列表算法 (worklist algorithm) 执行数据流分析。
 * @param t 指向 DataflowAnalysis 实例的指针 (具体分析的实例)。
 * @param func 指向要分析的 IR_function 的指针。
 */
extern void worklist_solver(DataflowAnalysis *t, IR_function *func);

#endif //CODE_DATAFLOW_ANALYSIS_H
