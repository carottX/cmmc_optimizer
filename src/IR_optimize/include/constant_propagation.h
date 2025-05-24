//
// Created by hby on 22-12-2.
//

#ifndef CODE_CONSTANT_PROPAGATION_H
#define CODE_CONSTANT_PROPAGATION_H

#include <dataflow_analysis.h> // 引入通用数据流分析框架的定义

/**
 * @brief 表示常量传播分析中变量的值状态。
 */
typedef struct {
    enum {
        UNDEF, // 未定义 (Undefined): 变量的值尚未确定或不可知。
        CONST, // 常量 (Constant): 变量的值是一个已知的编译时常量。
        NAC    // 非常量 (Not A Constant): 变量的值不是一个编译时常量，或者在不同路径上有不同的常量值。
    } kind;         //值的种类
    int const_val;  // 如果 kind 是 CONST，则存储常量值；否则此字段无意义。
} CPValue;

/**
 * @brief 创建一个表示 UNDEF (未定义) 状态的 CPValue。
 * @return CPValue 实例。
 */
static inline CPValue get_UNDEF() {return (CPValue){.kind = UNDEF, .const_val = 0};}
/**
 * @brief 创建一个表示 CONST (常量) 状态的 CPValue。
 * @param const_val 常量的值。
 * @return CPValue 实例。
 */
static inline CPValue get_CONST(int const_val) {return (CPValue){.kind = CONST, .const_val = const_val};}
/**
 * @brief 创建一个表示 NAC (非常量) 状态的 CPValue。
 * @return CPValue 实例。
 */
static inline CPValue get_NAC() {return (CPValue){.kind = NAC, .const_val = 0};}

// 定义从 IR_var (变量编号) 到 CPValue (常量传播值状态) 的映射。
// 这是常量传播分析中数据流事实 (Fact) 的核心。
DEF_MAP(IR_var, CPValue)
typedef Map_IR_var_CPValue *Map_ptr_IR_var_CPValue; // 指向 Map_IR_var_CPValue 的指针类型

// 定义从 IR_block_ptr (基本块指针) 到 Map_ptr_IR_var_CPValue (指向变量到CPValue映射的指针) 的映射。
// 用于存储每个基本块的 IN 和 OUT 数据流事实。
DEF_MAP(IR_block_ptr, Map_ptr_IR_var_CPValue)

// 前向声明 ConstantPropagation 结构体
typedef struct ConstantPropagation ConstantPropagation;

/**
 * @brief 常量传播分析的特定实现。
 * 继承自通用的 DataflowAnalysis 框架。
 */
typedef struct ConstantPropagation {
    /**
     * @brief 常量传播分析的虚函数表。
     * 包含对此特定分析的各种操作的函数指针。
     */
    struct ConstantPropagation_virtualTable {
        /**
         * @brief 析构（清理）ConstantPropagation实例占用的资源。
         * @param t 指向 ConstantPropagation 实例的指针。
         */
        void (*teardown) (ConstantPropagation *t);

        /**
         * @brief 判断是否为前向分析。常量传播是前向分析。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @return 对于常量传播分析，应返回 true。
         */
        bool (*isForward) (ConstantPropagation *t);

        /**
         * @brief 创建并返回边界条件下的数据流事实。
         * 对于前向的常量传播分析，这通常是入口基本块的OUT集合。
         * 函数参数可以初始化为UNDEF或NAC，全局变量可能需要特殊处理。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param func 当前正在分析的 IR_function。
         * @return 指向新创建的 Map_IR_var_CPValue 的指针。
         */
        Map_IR_var_CPValue *(*newBoundaryFact) (ConstantPropagation *t, IR_function *func);

        /**
         * @brief 创建并返回数据流事实的初始值。
         * 对于常量传播分析，通常所有变量的初始状态都是UNDEF (Top元素，因为meet是向下走的)。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @return 指向新创建的 Map_IR_var_CPValue 的指针。
         */
        Map_IR_var_CPValue *(*newInitialFact) (ConstantPropagation *t);

        /**
         * @brief 设置指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Map_IR_var_CPValue 的指针。
         */
        void (*setInFact) (ConstantPropagation *t, IR_block *blk, Map_IR_var_CPValue *fact);

        /**
         * @brief 设置指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @param fact 指向要设置的 Map_IR_var_CPValue 的指针。
         */
        void (*setOutFact) (ConstantPropagation *t, IR_block *blk, Map_IR_var_CPValue *fact);

        /**
         * @brief 获取指定基本块的输入数据流事实 (IN fact)。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Map_IR_var_CPValue 的指针。
         */
        Map_IR_var_CPValue *(*getInFact) (ConstantPropagation *t, IR_block *blk);

        /**
         * @brief 获取指定基本块的输出数据流事实 (OUT fact)。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param blk 指向目标 IR_block 的指针。
         * @return 指向获取到的 Map_IR_var_CPValue 的指针。
         */
        Map_IR_var_CPValue *(*getOutFact) (ConstantPropagation *t, IR_block *blk);

        /**
         * @brief 执行 meet 操作，将一个CPValue映射 (fact) 合并到另一个CPValue映射 (target)。
         * 对于常量传播分析，meet 操作定义如下：
         * UNDEF meet X = X
         * X meet UNDEF = X
         * CONST(c1) meet CONST(c1) = CONST(c1)
         * CONST(c1) meet CONST(c2) = NAC  (if c1 != c2)
         * NAC meet X = NAC
         * X meet NAC = NAC
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param fact 指向源 Map_IR_var_CPValue 的指针。
         * @param target 指向目标 Map_IR_var_CPValue 的指针 (此映射会被修改)。
         * @return 如果 target 映射因 meet 操作发生改变则返回 true，否则返回 false。
         */
        bool (*meetInto) (ConstantPropagation *t, Map_IR_var_CPValue *fact, Map_IR_var_CPValue *target);

        /**
         * @brief 执行传递函数，根据基本块的 IN 集合计算其 OUT 集合。
         * OUT[B] = transfer(IN[B])
         * 遍历基本块B中的每条语句，根据语句类型更新变量的CPValue状态。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param block 指向当前处理的 IR_block 的指针。
         * @param in_fact 指向输入 Fact (即 IN[B]) 的指针。
         * @param out_fact 指向输出 Fact (即 OUT[B]，此映射会被计算和修改) 的指针。
         * @return 如果 out_fact 因传递函数发生改变则返回 true，否则返回 false。
         */
        bool (*transferBlock) (ConstantPropagation *t, IR_block *block, Map_IR_var_CPValue *in_fact, Map_IR_var_CPValue *out_fact);

        /**
         * @brief 打印常量传播分析的结果。
         * @param t 指向 ConstantPropagation 实例的指针。
         * @param func 当前分析的 IR_function。
         */
        void (*printResult) (ConstantPropagation *t, IR_function *func);
    } const *vTable; // 指向虚函数表的指针

    // 存储每个基本块的IN和OUT事实的映射。
    // Fact 是 Map_IR_var_CPValue 类型。
    Map_IR_block_ptr_Map_ptr_IR_var_CPValue mapInFact, mapOutFact;
} ConstantPropagation;

/**
 * @brief 初始化 ConstantPropagation 实例。
 * 设置虚函数表，并初始化存储IN/OUT事实的映射。
 * @param t 指向要初始化的 ConstantPropagation 实例的指针。
 */
extern void ConstantPropagation_init(ConstantPropagation *t);

/**
 * @brief （辅助函数）处理单条语句对CPValue映射的影响。
 * 用于在 transferBlock 中迭代处理基本块内的语句。
 * @param t 指向 ConstantPropagation 实例的指针。
 * @param stmt 指向当前处理的 IR_stmt 的指针。
 * @param fact 指向当前CPValue映射的指针 (此映射会被修改)。
 */
extern void ConstantPropagation_transferStmt (ConstantPropagation *t,
                                              IR_stmt *stmt,
                                              Map_IR_var_CPValue *fact);

/**
 * @brief （具体实现）执行传递函数，根据基本块的 IN 集合计算其 OUT 集合。
 * 这是 ConstantPropagation_virtualTable 中 transferBlock 指针的实际函数。
 * @param t 指向 ConstantPropagation 实例的指针。
 * @param block 指向当前处理的 IR_block 的指针。
 * @param in_fact 指向 IN[B] 的指针。
 * @param out_fact 指向 OUT[B] 的指针 (此映射会被计算和修改)。
 * @return 如果 out_fact 因传递函数发生改变则返回 true，否则返回 false。
 */
extern bool ConstantPropagation_transferBlock (ConstantPropagation *t,
                                               IR_block *block,
                                               Map_IR_var_CPValue *in_fact,
                                               Map_IR_var_CPValue *out_fact);
/**
 * @brief （具体实现）打印常量传播分析的结果。
 * 这是 ConstantPropagation_virtualTable 中 printResult 指针的实际函数。
 * @param t 指向 ConstantPropagation 实例的指针。
 * @param func 当前分析的 IR_function。
 */
extern void ConstantPropagation_print_result (ConstantPropagation *t, IR_function *func);

/**
 * @brief 根据常量传播分析的结果，执行常量折叠 (Constant Folding) 优化。
 * 将表达式中的常量操作数替换为其值，并预计算结果。
 * 将变量的use替换为已知的常量值。
 * @param t 指向 ConstantPropagation 实例的指针 (应已包含分析结果)。
 * @param func 指向要优化的 IR_function 的指针。
 */
extern void ConstantPropagation_constant_folding (ConstantPropagation *t, IR_function *func);

#endif //CODE_CONSTANT_PROPAGATION_H