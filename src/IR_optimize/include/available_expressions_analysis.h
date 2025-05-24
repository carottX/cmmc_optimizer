//
// Created by hby on 22-12-3.
//

#ifndef CODE_AVAILABLE_EXPRESSIONS_ANALYSIS_H
#define CODE_AVAILABLE_EXPRESSIONS_ANALYSIS_H

#include <dataflow_analysis.h> // 引入通用数据流分析框架的定义

/**
 * @brief 表示一个表达式，由操作符和两个操作数构成。
 * 用于在可用表达式分析中唯一标识一个表达式。
 */
typedef struct {
    IR_OP_TYPE op; // 表达式的操作符 (如 ADD, SUB, MUL, DIV)
    IR_val rs1;    // 左操作数
    IR_val rs2;    // 右操作数
} Expr;

/**
 * @brief 比较两个 Expr 结构体。
 * 用于 TreapMap 的比较函数，以确保键的有序性。
 * 比较顺序：op, rs1, rs2。
 * @param a 第一个表达式。
 * @param b 第二个表达式。
 * @return 如果 a < b 返回 true-like (非-1且小于0)，如果 a > b 返回 false-like (非-1且大于0)，如果 a == b 返回 -1。
 */
extern int Expr_CMP(Expr a, Expr b);

// 定义从 Expr (表达式) 到 IR_var (代表该表达式结果的临时变量编号) 的映射。
// 使用 Expr_CMP 作为比较函数。
DEF_MAP_CMP(Expr, IR_var, Expr_CMP)

/**
 * @brief 表示可用表达式分析中的数据流事实 (Fact)。
 * Fact 本质上是一个 IR_var 的集合，其中每个 IR_var 代表一个当前可用的表达式。
 * 为了处理全集 (TOP) 的情况（例如，在Must Analysis中，初始状态或某些meet操作的结果可能是全集），
 * 引入了 is_top 标志。
 * 当 is_top = true 时，逻辑上表示所有表达式都可用，此时 set 集合可能为空或不完整。
 * 当 is_top = false 时，set 集合明确存储了当前可用的表达式（由其代表变量 IR_var 标识）。
 */
typedef struct {
    bool is_top;     // 标记是否为全集 (TOP)。
    Set_IR_var set;  // 存储可用表达式代表变量的集合。
} Fact_set_var, *Fact_set_var_ptr; // Fact_set_var 结构体及其指针类型

/**
 * @brief 初始化一个 Fact_set_var 实例。
 * @param fact 指向要初始化的 Fact_set_var 实例的指针。
 * @param is_top 布尔值，指示此 Fact 是否应初始化为全集 (TOP)。
 */
extern void Fact_set_var_init(Fact_set_var *fact, bool is_top);

/**
 * @brief 析构（清理）一个 Fact_set_var 实例占用的资源。
 * @param fact 指向要析构的 Fact_set_var 实例的指针。
 */
extern void Fact_set_var_teardown(Fact_set_var *fact);


// 定义从 IR_block_ptr (基本块指针) 到 Fact_set_var_ptr (指向Fact_set_var的指针) 的映射。
// 用于存储每个基本块的 IN 和 OUT 数据流事实。
DEF_MAP(IR_block_ptr, Fact_set_var_ptr)

// 定义 IR_var 动态数组的指针类型。
typedef Vec_IR_var *Vec_ptr_IR_var;
// 定义从 IR_var (被定义的变量) 到 Vec_ptr_IR_var (一个IR_var列表的指针，这些IR_var代表的表达式会被此定义kill掉) 的映射。
// 例如，如果 v1 被重新定义，那么所有使用 v1 作为操作数的表达式（如 expr_v2 := v1 + v3）都会被 kill。
DEF_MAP(IR_var, Vec_ptr_IR_var)

// 前向声明 AvailableExpressionsAnalysis 结构体
typedef struct AvailableExpressionsAnalysis AvailableExpressionsAnalysis;

/**
 * @brief 可用表达式分析的特定实现。
 * 继承自通用的 DataflowAnalysis 框架。
 */
typedef struct AvailableExpressionsAnalysis {
    /**
     * @brief 可用表达式分析的虚函数表。
     */
    struct AvailableExpressionsAnalysis_virtualTable {
        /**
         * @brief 析构（清理）AvailableExpressionsAnalysis实例占用的资源。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         */
        void (*teardown) (AvailableExpressionsAnalysis *t);

        /**
         * @brief 判断是否为前向分析。可用表达式分析是前向分析。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @return 对于可用表达式分析，应返回 true。
         */
        bool (*isForward) (AvailableExpressionsAnalysis *t);

        /**
         * @brief 创建并返回边界条件下的数据流事实。
         * 对于前向的可用表达式分析，这通常是入口基本块的OUT集合。
         * 初始时，通常没有表达式是可用的（空集，is_top=false）。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param func 当前正在分析的 IR_function。
         * @return 指向新创建的 Fact_set_var 的指针。
         */
        Fact_set_var *(*newBoundaryFact) (AvailableExpressionsAnalysis *t, IR_function *func);

        /**
         * @brief 创建并返回数据流事实的初始值。
         * 对于可用表达式分析 (Must Analysis)，通常初始值是全集 TOP (is_top=true)，
         * 表示在没有信息前，我们假设所有表达式都可能有效，通过meet操作（交集）来排除。
         * 或者，也可以初始化为空集 (Bottom)，然后通过transfer函数来生成。
         * 框架的注释中提到 is_top 代表全集，所以这里可能倾向于初始化为 is_top=true。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @return 指向新创建的 Fact_set_var 的指针。
         */
        Fact_set_var *(*newInitialFact) (AvailableExpressionsAnalysis *t);

        /**
         * @brief 设置指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Fact_set_var 的指针。
         */
        void (*setInFact) (AvailableExpressionsAnalysis *t, IR_block *blk, Fact_set_var *fact);

        /**
         * @brief 设置指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Fact_set_var 的指针。
         */
        void (*setOutFact) (AvailableExpressionsAnalysis *t, IR_block *blk, Fact_set_var *fact);

        /**
         * @brief 获取指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Fact_set_var 的指针。
         */
        Fact_set_var *(*getInFact) (AvailableExpressionsAnalysis *t, IR_block *blk);

        /**
         * @brief 获取指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Fact_set_var 的指针。
         */
        Fact_set_var *(*getOutFact) (AvailableExpressionsAnalysis *t, IR_block *blk);

        /**
         * @brief 执行 meet 操作，将一个Fact_set_var (fact) 合并到另一个Fact_set_var (target)。
         * 对于可用表达式分析 (Must Analysis)，meet 操作是交集。
         * 如果 target.is_top 为 true，则 target 变为 fact 的副本。
         * 如果 fact.is_top 为 true，则 target 不变。
         * 否则，计算两个集合的交集。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param fact 指向源 Fact_set_var 的指针。
         * @param target 指向目标 Fact_set_var 的指针 (此Fact会被修改)。
         * @return 如果 target Fact 因 meet 操作发生改变则返回 true，否则返回 false。
         */
        bool (*meetInto) (AvailableExpressionsAnalysis *t, Fact_set_var *fact, Fact_set_var *target);

        /**
         * @brief 执行传递函数，根据基本块的 IN 集合计算其 OUT 集合。
         * OUT[B] = gen[B] U (IN[B] - kill[B])
         * 遍历基本块B中的每条语句：
         * - gen[B]: 如果语句计算了一个新的表达式 expr_var := x op y，则将 expr_var 加入 OUT[B]。
         * - kill[B]: 如果语句重新定义了变量 v，则所有以 v 为操作数的可用表达式都将从 OUT[B] 中移除。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param block 指向当前处理的 IR_block 的指针。
         * @param in_fact 指向输入 Fact (即 IN[B]) 的指针。
         * @param out_fact 指向输出 Fact (即 OUT[B]，此Fact会被计算和修改) 的指针。
         * @return 如果 out_fact 因传递函数发生改变则返回 true，否则返回 false。
         */
        bool (*transferBlock) (AvailableExpressionsAnalysis *t, IR_block *block, Fact_set_var *in_fact, Fact_set_var *out_fact);

        /**
         * @brief 打印可用表达式分析的结果。
         * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
         * @param func 当前分析的 IR_function。
         */
        void (*printResult) (AvailableExpressionsAnalysis *t, IR_function *func);
    } const *vTable; // 指向虚函数表的指针

    // 预处理阶段构建的映射：
    Map_Expr_IR_var mapExpr; // 从表达式 (Expr) 到代表该表达式的唯一临时变量 (IR_var) 的映射。
    Map_IR_var_Vec_ptr_IR_var mapExprKill; // 从被定义的变量 (IR_var) 到一个IR_var列表的映射。
                                         // 该列表包含所有因为此变量被定义而被kill掉的表达式（由它们的代表变量标识）。

    // 存储每个基本块的IN和OUT事实的映射。
    Map_IR_block_ptr_Fact_set_var_ptr mapInFact, mapOutFact;
} AvailableExpressionsAnalysis;

/**
 * @brief 初始化 AvailableExpressionsAnalysis 实例。
 * 设置虚函数表，并初始化相关的映射表。
 * @param t 指向要初始化的 AvailableExpressionsAnalysis 实例的指针。
 */
extern void AvailableExpressionsAnalysis_init(AvailableExpressionsAnalysis *t);

/**
 * @brief 预处理步骤：合并公共表达式。
 * 遍历函数中的所有 IR_op_stmt，如果多个语句计算相同的表达式 (x op y)，
 * 则将它们替换为：
 * expr_var := x op y  (如果这个expr_var是第一次遇到此表达式)
 * original_rd := expr_var
 * 同时，填充 mapExpr 和 mapExprKill 供后续数据流分析使用。
 * 还会进行一些简单的表达式优化，如 x+0 -> x。
 * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
 * @param func 指向要处理的 IR_function 的指针。
 */
extern void AvailableExpressionsAnalysis_merge_common_expr(AvailableExpressionsAnalysis *t,
                                                             IR_function *func);
/**
 * @brief （辅助函数）处理单条语句对Fact_set_var的影响 (gen 和 kill)。
 * 用于在 transferBlock 中迭代处理基本块内的语句。
 * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
 * @param stmt 指向当前处理的 IR_stmt 的指针。
 * @param fact 指向当前的 Fact_set_var 的指针 (此Fact会被修改)。
 */
extern void AvailableExpressionsAnalysis_transferStmt (AvailableExpressionsAnalysis *t,
                                                       IR_stmt *stmt,
                                                       Fact_set_var *fact);

/**
 * @brief （具体实现）执行传递函数，根据基本块的 IN 集合计算其 OUT 集合。
 * 这是 AvailableExpressionsAnalysis_virtualTable 中 transferBlock 指针的实际函数。
 * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
 * @param block 指向当前处理的 IR_block 的指针。
 * @param in_fact 指向 IN[B] 的指针。
 * @param out_fact 指向 OUT[B] 的指针 (此Fact会被计算和修改)。
 * @return 如果 out_fact 因传递函数发生改变则返回 true，否则返回 false。
 */
extern bool AvailableExpressionsAnalysis_transferBlock (AvailableExpressionsAnalysis *t,
                                                        IR_block *block,
                                                        Fact_set_var *in_fact,
                                                        Fact_set_var *out_fact);
/**
 * @brief （具体实现）打印可用表达式分析的结果。
 * 这是 AvailableExpressionsAnalysis_virtualTable 中 printResult 指针的实际函数。
 * @param t 指向 AvailableExpressionsAnalysis 实例的指针。
 * @param func 当前分析的 IR_function。
 */
extern void AvailableExpressionsAnalysis_print_result (AvailableExpressionsAnalysis *t, IR_function *func);

/**
 * @brief 根据可用表达式分析的结果，移除冗余的表达式定义。
 * 如果一个表达式 expr_var := x op y 在其定义点是可用的（即 IN[stmt] 中包含 expr_var），
 * 那么这个定义就是冗余的，可以被标记为死代码并移除。
 * @param t 指向 AvailableExpressionsAnalysis 实例的指针 (应已包含分析结果)。
 * @param func 指向要优化的 IR_function 的指针。
 */
extern void AvailableExpressionsAnalysis_remove_available_expr_def (AvailableExpressionsAnalysis *t, IR_function *func);

#endif //CODE_AVAILABLE_EXPRESSIONS_ANALYSIS_H