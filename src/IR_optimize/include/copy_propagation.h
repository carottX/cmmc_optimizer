//
// Created by hby on 22-12-2.
//

#ifndef CODE_COPY_PROPAGATION_H
#define CODE_COPY_PROPAGATION_H

#include <dataflow_analysis.h> // 引入通用数据流分析框架的定义

// 定义从 IR_var (变量编号) 到 IR_var (变量编号) 的映射。
// 用于表示 def_to_use (定义变量到使用变量的映射) 和 use_to_def (使用变量到定义变量的映射)。
DEF_MAP(IR_var, IR_var)

/**
 * @brief 表示复制传播分析中的数据流事实 (Fact)。
 * 一个 Fact 包含了一组当前有效的复制语句 (def := use)。
 * 为了避免在is_top为true时需要插入所有可能的复制语句，
 * is_top = true 表示全集 (TOP)，此时 def_to_use 和 use_to_def 映射为空，
 * 但逻辑上代表所有可能的 x := y。
 * 当 is_top = false 时，def_to_use[def] = use 且 use_to_def[use] = def
 * 同时成立才表示 def := use 这条复制语句在当前点是有效的。
 */
typedef struct {
    bool is_top; // 标记是否为全集 (TOP)。如果为true，则逻辑上包含所有可能的复制语句。
    Map_IR_var_IR_var def_to_use; // 存储从定义变量到使用变量的映射 (def => use)
    Map_IR_var_IR_var use_to_def; // 存储从使用变量到定义变量的映射 (use => def)
} Fact_def_use, *Fact_def_use_ptr; // Fact_def_use 结构体及其指针类型

/**
 * @brief 初始化一个 Fact_def_use 实例。
 * @param fact 指向要初始化的 Fact_def_use 实例的指针。
 * @param is_top 布尔值，指示此 Fact 是否应初始化为全集 (TOP)。
 */
extern void Fact_def_use_init(Fact_def_use *fact, bool is_top);

/**
 * @brief 析构（清理）一个 Fact_def_use 实例占用的资源。
 * @param fact 指向要析构的 Fact_def_use 实例的指针。
 */
extern void Fact_def_use_teardown(Fact_def_use *fact);

// 定义从 IR_block_ptr (基本块指针) 到 Fact_def_use_ptr (指向Fact_def_use的指针) 的映射。
// 用于存储每个基本块的 IN 和 OUT 数据流事实。
DEF_MAP(IR_block_ptr, Fact_def_use_ptr)

// 前向声明 CopyPropagation 结构体
typedef struct CopyPropagation CopyPropagation;

/**
 * @brief 复制传播分析的特定实现。
 * 继承自通用的 DataflowAnalysis 框架。
 */
typedef struct CopyPropagation {
    /**
     * @brief 复制传播分析的虚函数表。
     * 包含对此特定分析的各种操作的函数指针。
     */
    struct CopyPropagation_virtualTable {
        /**
         * @brief 析构（清理）CopyPropagation实例占用的资源。
         * @param t 指向 CopyPropagation 实例的指针。
         */
        void (*teardown) (CopyPropagation *t);

        /**
         * @brief 判断是否为前向分析。复制传播是前向分析。
         * @param t 指向 CopyPropagation 实例的指针。
         * @return 对于复制传播分析，应返回 true。
         */
        bool (*isForward) (CopyPropagation *t);

        /**
         * @brief 创建并返回边界条件下的数据流事实。
         * 对于前向的复制传播分析，这通常是入口基本块的OUT集合。
         * 初始时，可以认为没有复制语句是有效的（空集，is_top=false），或者根据具体语义设定。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param func 当前正在分析的 IR_function。
         * @return 指向新创建的 Fact_def_use 的指针。
         */
        Fact_def_use *(*newBoundaryFact) (CopyPropagation *t, IR_function *func);

        /**
         * @brief 创建并返回数据流事实的初始值。
         * 对于复制传播分析 (Must Analysis)，通常初始值是全集 TOP (is_top=true)，
         * 表示在没有信息前，我们假设所有复制都可能有效，通过meet操作（交集）来排除。
         * 或者，也可以初始化为空集 (Bottom)，然后通过transfer函数来生成。
         * 框架的注释中提到 is_top 代表全集，所以这里可能倾向于初始化为 is_top=true。
         * @param t 指向 CopyPropagation 实例的指针。
         * @return 指向新创建的 Fact_def_use 的指针。
         */
        Fact_def_use *(*newInitialFact) (CopyPropagation *t);

        /**
         * @brief 设置指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Fact_def_use 的指针。
         */
        void (*setInFact) (CopyPropagation *t, IR_block *blk, Fact_def_use *fact);

        /**
         * @brief 设置指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Fact_def_use 的指针。
         */
        void (*setOutFact) (CopyPropagation *t, IR_block *blk, Fact_def_use *fact);

        /**
         * @brief 获取指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Fact_def_use 的指针。
         */
        Fact_def_use *(*getInFact) (CopyPropagation *t, IR_block *blk);

        /**
         * @brief 获取指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Fact_def_use 的指针。
         */
        Fact_def_use *(*getOutFact) (CopyPropagation *t, IR_block *blk);

        /**
         * @brief 执行 meet 操作，将一个Fact_def_use (fact) 合并到另一个Fact_def_use (target)。
         * 对于复制传播分析 (Must Analysis)，meet 操作是交集。
         * 如果 target.is_top 为 true，则 target 变为 fact 的副本。
         * 如果 fact.is_top 为 true，则 target 不变。
         * 否则，计算两个集合的交集。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param fact 指向源 Fact_def_use 的指针。
         * @param target 指向目标 Fact_def_use 的指针 (此Fact会被修改)。
         * @return 如果 target Fact 因 meet 操作发生改变则返回 true，否则返回 false。
         */
        bool (*meetInto) (CopyPropagation *t, Fact_def_use *fact, Fact_def_use *target);

        /**
         * @brief 执行传递函数，根据基本块的 IN 集合计算其 OUT 集合。
         * OUT[B] = gen[B] U (IN[B] - kill[B])
         * 遍历基本块B中的每条语句：
         * - 如果语句是 x := y (且y不是常量)，则 gen 集合加入 {x_to_y, y_to_x}。
         * - 如果语句对 x 或 y 进行了重定义，则 kill 集合移除所有涉及 x 或 y 的复制语句。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param block 指向当前处理的 IR_block 的指针。
         * @param in_fact 指向输入 Fact (即 IN[B]) 的指针。
         * @param out_fact 指向输出 Fact (即 OUT[B]，此Fact会被计算和修改) 的指针。
         * @return 如果 out_fact 因传递函数发生改变则返回 true，否则返回 false。
         */
        bool (*transferBlock) (CopyPropagation *t, IR_block *block, Fact_def_use *in_fact, Fact_def_use *out_fact);

        /**
         * @brief 打印复制传播分析的结果。
         * @param t 指向 CopyPropagation 实例的指针。
         * @param func 当前分析的 IR_function。
         */
        void (*printResult) (CopyPropagation *t, IR_function *func);
    } const *vTable; // 指向虚函数表的指针

    // 存储每个基本块的IN和OUT事实的映射。
    // Fact 是 Fact_def_use 类型。
    Map_IR_block_ptr_Fact_def_use_ptr mapInFact, mapOutFact;
} CopyPropagation;

/**
 * @brief 初始化 CopyPropagation 实例。
 * 设置虚函数表，并初始化存储IN/OUT事实的映射。
 * @param t 指向要初始化的 CopyPropagation 实例的指针。
 */
extern void CopyPropagation_init(CopyPropagation *t);

/**
 * @brief （辅助函数）处理单条语句对Fact_def_use的影响 (gen 和 kill)。
 * 用于在 transferBlock 中迭代处理基本块内的语句。
 * @param t 指向 CopyPropagation 实例的指针。
 * @param stmt 指向当前处理的 IR_stmt 的指针。
 * @param fact 指向当前的 Fact_def_use 的指针 (此Fact会被修改)。
 */
extern void CopyPropagation_transferStmt (CopyPropagation *t,
                                          IR_stmt *stmt,
                                          Fact_def_use *fact);

/**
 * @brief （具体实现）执行传递函数，根据基本块的 IN 集合计算其 OUT 集合。
 * 这是 CopyPropagation_virtualTable 中 transferBlock 指针的实际函数。
 * @param t 指向 CopyPropagation 实例的指针。
 * @param block 指向当前处理的 IR_block 的指针。
 * @param in_fact 指向 IN[B] 的指针。
 * @param out_fact 指向 OUT[B] 的指针 (此Fact会被计算和修改)。
 * @return 如果 out_fact 因传递函数发生改变则返回 true，否则返回 false。
 */
extern bool CopyPropagation_transferBlock (CopyPropagation *t,
                                           IR_block *block,
                                           Fact_def_use *in_fact,
                                           Fact_def_use *out_fact);
/**
 * @brief （具体实现）打印复制传播分析的结果。
 * 这是 CopyPropagation_virtualTable 中 printResult 指针的实际函数。
 * @param t 指向 CopyPropagation 实例的指针。
 * @param func 当前分析的 IR_function。
 */
extern void CopyPropagation_print_result (CopyPropagation *t, IR_function *func);

/**
 * @brief 根据复制传播分析的结果，替换代码中可用的复制。
 * 如果在某个点，复制语句 x := y 是有效的 (available)，并且变量 y 被使用，
 * 那么可以将 y 的使用替换为 x (或者反之，取决于传播方向和策略)。
 * 此函数通常会遍历所有语句，检查其use变量是否可以通过已知的复制进行替换。
 * @param t 指向 CopyPropagation 实例的指针 (应已包含分析结果)。
 * @param func 指向要优化的 IR_function 的指针。
 */
extern void CopyPropagation_replace_available_use_copy (CopyPropagation *t, IR_function *func);

#endif //CODE_COPY_PROPAGATION_H