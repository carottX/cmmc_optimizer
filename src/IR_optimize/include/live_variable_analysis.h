//
// Created by hby on 22-12-2.
//

#ifndef CODE_LIVE_VARIABLE_ANALYSIS_H
#define CODE_LIVE_VARIABLE_ANALYSIS_H

#include <dataflow_analysis.h> // 引入通用数据流分析框架的定义

// 前向声明 LiveVariableAnalysis 结构体
typedef struct LiveVariableAnalysis LiveVariableAnalysis;

/**
 * @brief 活跃变量分析的特定实现。
 * 继承自通用的 DataflowAnalysis 框架。
 */
typedef struct LiveVariableAnalysis {
    /**
     * @brief 活跃变量分析的虚函数表。
     * 包含对此特定分析的各种操作的函数指针。
     */
    struct LiveVariableAnalysis_virtualTable {
        /**
         * @brief 析构（清理）LiveVariableAnalysis实例占用的资源。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         */
        void (*teardown) (LiveVariableAnalysis *t);

        /**
         * @brief 判断是否为前向分析。活跃变量分析是后向分析。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @return 对于活跃变量分析，应返回 false。
         */
        bool (*isForward) (LiveVariableAnalysis *t);

        /**
         * @brief 创建并返回边界条件下的数据流事实。
         * 对于后向的活跃变量分析，这通常是出口基本块的IN集合，初始为空集。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param func 当前正在分析的 IR_function。
         * @return 指向新创建的 Set_IR_var (变量集合) 的指针。
         */
        Set_IR_var *(*newBoundaryFact) (LiveVariableAnalysis *t, IR_function *func);

        /**
         * @brief 创建并返回数据流事实的初始值。
         * 对于活跃变量分析（May Analysis），通常是空集（Bottom元素）。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @return 指向新创建的 Set_IR_var (变量集合) 的指针。
         */
        Set_IR_var *(*newInitialFact) (LiveVariableAnalysis *t);

        /**
         * @brief 设置指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Set_IR_var (变量集合) 的指针。
         */
        void (*setInFact) (LiveVariableAnalysis *t, IR_block *blk, Set_IR_var *fact);

        /**
         * @brief 设置指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Set_IR_var (变量集合) 的指针。
         */
        void (*setOutFact) (LiveVariableAnalysis *t, IR_block *blk, Set_IR_var *fact);

        /**
         * @brief 获取指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Set_IR_var (变量集合) 的指针。
         */
        Set_IR_var *(*getInFact) (LiveVariableAnalysis *t, IR_block *blk);

        /**
         * @brief 获取指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Set_IR_var (变量集合) 的指针。
         */
        Set_IR_var *(*getOutFact) (LiveVariableAnalysis *t, IR_block *blk);

        /**
         * @brief 执行 meet 操作，将一个变量集合 (fact) 合并到另一个变量集合 (target)。
         * 对于活跃变量分析，meet 操作是并集。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param fact 指向源 Set_IR_var 的指针。
         * @param target 指向目标 Set_IR_var 的指针 (此集合会被修改)。
         * @return 如果 target 集合因 meet 操作发生改变则返回 true，否则返回 false。
         */
        bool (*meetInto) (LiveVariableAnalysis *t, Set_IR_var *fact, Set_IR_var *target);

        /**
         * @brief 执行传递函数，根据基本块的 OUT 集合计算其 IN 集合。
         * IN[B] = use[B] U (OUT[B] - def[B])
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param block 指向当前处理的 IR_block 的指针。
         * @param in_fact 指向输入 Fact (即 IN[B]，此集合会被计算和修改) 的指针。
         * @param out_fact 指向输出 Fact (即 OUT[B]) 的指针。
         * @return 如果 in_fact 因传递函数发生改变则返回 true，否则返回 false。
         */
        bool (*transferBlock) (LiveVariableAnalysis *t, IR_block *block, Set_IR_var *in_fact, Set_IR_var *out_fact);

        /**
         * @brief 打印活跃变量分析的结果。
         * @param t 指向 LiveVariableAnalysis 实例的指针。
         * @param func 当前分析的 IR_function。
         */
        void (*printResult) (LiveVariableAnalysis *t, IR_function *func);
    } const *vTable; // 指向虚函数表的指针

    // 存储每个基本块的IN和OUT事实的映射。
    // Fact 为变量的集合 (Set_IR_var)。
    Map_IR_block_ptr_Set_ptr_IR_var mapInFact, mapOutFact;
} LiveVariableAnalysis;

/**
 * @brief 初始化 LiveVariableAnalysis 实例。
 * 设置虚函数表，并初始化存储IN/OUT事实的映射。
 * @param t 指向要初始化的 LiveVariableAnalysis 实例的指针。
 */
extern void LiveVariableAnalysis_init(LiveVariableAnalysis *t);

/**
 * @brief （辅助函数）处理单条语句对活跃变量集合的影响。
 * 用于在 transferBlock 中迭代处理基本块内的语句。
 * new_fact = use[stmt] U (old_fact - def[stmt])
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param stmt 指向当前处理的 IR_stmt 的指针。
 * @param fact 指向当前活跃变量集合的指针 (此集合会被修改)。
 */
extern void LiveVariableAnalysis_transferStmt (LiveVariableAnalysis *t,
                                               IR_stmt *stmt,
                                               Set_IR_var *fact);

/**
 * @brief （具体实现）执行传递函数，根据基本块的 OUT 集合计算其 IN 集合。
 * 这是 LiveVariableAnalysis_virtualTable 中 transferBlock 指针的实际函数。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param block 指向当前处理的 IR_block 的指针。
 * @param in_fact 指向 IN[B] 的指针 (此集合会被计算和修改)。
 * @param out_fact 指向 OUT[B] 的指针。
 * @return 如果 in_fact 因传递函数发生改变则返回 true，否则返回 false。
 */
extern bool LiveVariableAnalysis_transferBlock (LiveVariableAnalysis *t,
                                                IR_block *block,
                                                Set_IR_var *in_fact,
                                                Set_IR_var *out_fact);
/**
 * @brief （具体实现）打印活跃变量分析的结果。
 * 这是 LiveVariableAnalysis_virtualTable 中 printResult 指针的实际函数。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param func 当前分析的 IR_function。
 */
extern void LiveVariableAnalysis_print_result (LiveVariableAnalysis *t, IR_function *func);

/**
 * @brief 根据活跃变量分析的结果，移除函数中的死定义 (dead definitions)。
 * 如果一个变量被定义了，但在其定义点之后不再活跃（即不在OUT集合中），则该定义是死代码。
 * @param t 指向 LiveVariableAnalysis 实例的指针 (应已包含分析结果)。
 * @param func 指向要优化的 IR_function 的指针。
 * @return 如果成功移除了任何死代码则返回 true，否则返回 false。
 */
extern bool LiveVariableAnalysis_remove_dead_def (LiveVariableAnalysis *t, IR_function *func);

#endif //CODE_LIVE_VARIABLE_ANALYSIS_H