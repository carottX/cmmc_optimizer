//
// Created by Assistant
// 归纳变量分析 (Induction Variable Analysis)
// 基于循环分析识别基本归纳变量和派生归纳变量
//

#ifndef CODE_INDUCTION_VARIABLE_ANALYSIS_H
#define CODE_INDUCTION_VARIABLE_ANALYSIS_H

#include <IR.h>
#include <loop_analysis.h>
#include <container/list.h>
#include <container/treap.h>
#include <stdio.h>

//// ================================== 归纳变量数据结构 ==================================

/**
 * @brief 基本归纳变量结构体
 * 表示形如 i = i + c 或 i = i - c 的循环变量
 */
typedef struct BasicInductionVariable {
    IR_var variable;                    // 归纳变量
    IR_block_ptr increment_block;       // 增量操作所在的基本块
    IR_stmt *increment_stmt;            // 增量操作语句 (i = i + c)
    int step;                           // 步长值 (正数表示递增，负数表示递减)
    bool is_increment;                  // true: i = i + c, false: i = i - c
} BasicInductionVariable;

typedef BasicInductionVariable* BasicInductionVariable_ptr;

DEF_LIST(BasicInductionVariable_ptr)    // 定义基本归纳变量列表类型
DEF_MAP(IR_var, BasicInductionVariable_ptr)  // 定义变量到基本归纳变量的映射

/**
 * @brief 派生归纳变量结构体  
 * 表示形如 j = c1 * i + c2 的变量，其中 i 是基本归纳变量
 */
typedef struct DerivedInductionVariable {
    IR_var variable;                    // 派生归纳变量
    BasicInductionVariable_ptr basic_iv; // 对应的基本归纳变量
    int coefficient;                    // 系数 c1
    int constant;                       // 常数项 c2
    IR_stmt *definition_stmt;           // 定义语句
} DerivedInductionVariable;

typedef DerivedInductionVariable* DerivedInductionVariable_ptr;

DEF_LIST(DerivedInductionVariable_ptr)   // 定义派生归纳变量列表类型
DEF_MAP(IR_var, DerivedInductionVariable_ptr)  // 定义变量到派生归纳变量的映射

/**
 * @brief 循环的归纳变量信息
 * 包含该循环中的所有基本和派生归纳变量
 */
typedef struct LoopInductionVariables {
    Loop_ptr loop;                      // 对应的循环
    List_BasicInductionVariable_ptr basic_ivs;     // 基本归纳变量列表
    List_DerivedInductionVariable_ptr derived_ivs; // 派生归纳变量列表
    
    // 快速查找映射
    Map_IR_var_BasicInductionVariable_ptr basic_iv_map;
    Map_IR_var_DerivedInductionVariable_ptr derived_iv_map;
} LoopInductionVariables;

typedef LoopInductionVariables* LoopInductionVariables_ptr;

DEF_LIST(LoopInductionVariables_ptr)    // 定义循环归纳变量信息列表类型

/**
 * @brief 归纳变量分析器
 * 管理整个函数的归纳变量分析
 */
typedef struct InductionVariableAnalyzer {
    IR_function *function;              // 当前分析的函数
    LoopAnalyzer *loop_analyzer;        // 循环分析器（必需）
    
    List_LoopInductionVariables_ptr loop_ivs;  // 所有循环的归纳变量信息
    
    // 全局映射（跨所有循环）
    Map_IR_var_BasicInductionVariable_ptr global_basic_iv_map;
    Map_IR_var_DerivedInductionVariable_ptr global_derived_iv_map;
} InductionVariableAnalyzer;

//// ================================== 归纳变量分析 API ==================================

/**
 * @brief 初始化归纳变量分析器
 * @param analyzer 指向要初始化的归纳变量分析器的指针
 * @param func 要分析的函数
 * @param loop_analyzer 已完成分析的循环分析器
 */
extern void InductionVariableAnalyzer_init(InductionVariableAnalyzer *analyzer, 
                                           IR_function *func, 
                                           LoopAnalyzer *loop_analyzer);

/**
 * @brief 析构归纳变量分析器，释放相关资源
 * @param analyzer 指向要析构的分析器的指针
 */
extern void InductionVariableAnalyzer_teardown(InductionVariableAnalyzer *analyzer);

/**
 * @brief 执行归纳变量分析
 * 识别函数中所有循环的基本和派生归纳变量
 * @param analyzer 归纳变量分析器
 */
extern void InductionVariableAnalyzer_analyze(InductionVariableAnalyzer *analyzer);

/**
 * @brief 分析单个循环中的基本归纳变量
 * @param analyzer 归纳变量分析器
 * @param loop 要分析的循环
 * @param loop_ivs 循环归纳变量信息结构体
 */
extern void InductionVariableAnalyzer_analyze_basic_ivs(InductionVariableAnalyzer *analyzer,
                                                        Loop_ptr loop,
                                                        LoopInductionVariables_ptr loop_ivs);

/**
 * @brief 分析单个循环中的派生归纳变量
 * @param analyzer 归纳变量分析器
 * @param loop 要分析的循环
 * @param loop_ivs 循环归纳变量信息结构体
 */
extern void InductionVariableAnalyzer_analyze_derived_ivs(InductionVariableAnalyzer *analyzer,
                                                          Loop_ptr loop,
                                                          LoopInductionVariables_ptr loop_ivs);

//// ================================== 查询接口 ==================================

/**
 * @brief 检查变量是否为基本归纳变量
 * @param analyzer 归纳变量分析器
 * @param variable 要检查的变量
 * @return 如果是基本归纳变量返回指针，否则返回NULL
 */
extern BasicInductionVariable_ptr InductionVariableAnalyzer_get_basic_iv(
    InductionVariableAnalyzer *analyzer, IR_var variable);

/**
 * @brief 检查变量是否为派生归纳变量
 * @param analyzer 归纳变量分析器
 * @param variable 要检查的变量
 * @return 如果是派生归纳变量返回指针，否则返回NULL
 */
extern DerivedInductionVariable_ptr InductionVariableAnalyzer_get_derived_iv(
    InductionVariableAnalyzer *analyzer, IR_var variable);

/**
 * @brief 获取循环的归纳变量信息
 * @param analyzer 归纳变量分析器
 * @param loop 循环指针
 * @return 循环归纳变量信息指针，如果没有找到返回NULL
 */
extern LoopInductionVariables_ptr InductionVariableAnalyzer_get_loop_ivs(
    InductionVariableAnalyzer *analyzer, Loop_ptr loop);

//// ================================== 结果输出 ==================================

/**
 * @brief 打印基本归纳变量的详细信息
 * @param basic_iv 基本归纳变量指针
 * @param out 输出文件流
 */
extern void BasicInductionVariable_print(BasicInductionVariable_ptr basic_iv, FILE *out);

/**
 * @brief 打印派生归纳变量的详细信息
 * @param derived_iv 派生归纳变量指针
 * @param out 输出文件流
 */
extern void DerivedInductionVariable_print(DerivedInductionVariable_ptr derived_iv, FILE *out);

/**
 * @brief 打印循环归纳变量的详细信息
 * @param loop_ivs 循环归纳变量信息指针
 * @param out 输出文件流
 */
extern void LoopInductionVariables_print(LoopInductionVariables_ptr loop_ivs, FILE *out);

/**
 * @brief 打印归纳变量分析的完整结果
 * @param analyzer 归纳变量分析器
 * @param out 输出文件流
 */
extern void InductionVariableAnalyzer_print_result(InductionVariableAnalyzer *analyzer, FILE *out);

//// ================================== 高层接口 ==================================

/**
 * @brief 对单个函数执行完整的归纳变量分析
 * 包括循环分析和归纳变量识别
 * @param func 要分析的IR函数
 * @param loop_analyzer 已完成的循环分析器
 */
extern void perform_induction_variable_analysis(IR_function *func, LoopAnalyzer *loop_analyzer);

/**
 * @brief 对所有函数执行归纳变量分析
 * 自动进行循环分析，然后执行归纳变量分析
 */
extern void analyze_all_functions_induction_variables();

//// ================================== 强度削减优化 ==================================

/**
 * @brief 强度削减变量结构体
 * 表示为强度削减创建的新增量变量
 */
typedef struct StrengthReductionVariable {
    IR_var new_variable;                // 新创建的增量变量
    DerivedInductionVariable_ptr original_derived_iv;  // 原始的派生归纳变量
    int increment_value;                // 每次循环的增量值 (coefficient * basic_step)
    IR_stmt *initialization_stmt;       // 在preheader中的初始化语句
    IR_stmt *increment_stmt;           // 在循环体中的增量语句
    IR_block_ptr increment_block;      // 增量语句所在的块
} StrengthReductionVariable;

typedef StrengthReductionVariable* StrengthReductionVariable_ptr;

DEF_LIST(StrengthReductionVariable_ptr)  // 定义强度削减变量列表类型

/**
 * @brief 对循环执行强度削减优化
 * 将派生归纳变量的乘法操作替换为增量操作
 * @param analyzer 归纳变量分析器
 * @param loop_ivs 循环归纳变量信息
 * @return 创建的强度削减变量列表
 */
extern List_StrengthReductionVariable_ptr perform_strength_reduction(
    InductionVariableAnalyzer *analyzer,
    LoopInductionVariables_ptr loop_ivs);

/**
 * @brief 对所有函数执行强度削减优化
 * 在归纳变量分析的基础上进行强度削减
 */
extern void perform_strength_reduction_all_functions();

/**
 * @brief 对单个函数的所有循环执行强度削减
 * @param function 要处理的函数
 * @param loop_analyzer 循环分析器结果
 */
extern void perform_strength_reduction_for_function(IR_function_ptr function, LoopAnalyzer *loop_analyzer);

/**
 * @brief 为派生归纳变量创建强度削减变量
 * @param analyzer 归纳变量分析器
 * @param derived_iv 派生归纳变量
 * @param loop_ivs 循环归纳变量信息
 * @return 创建的强度削减变量，失败返回NULL
 */
extern StrengthReductionVariable_ptr create_strength_reduction_variable(
    InductionVariableAnalyzer *analyzer,
    DerivedInductionVariable_ptr derived_iv,
    LoopInductionVariables_ptr loop_ivs);

/**
 * @brief 替换派生归纳变量的使用
 * 将对派生归纳变量的使用替换为强度削减变量
 * @param derived_iv 派生归纳变量
 * @param sr_var 强度削减变量
 * @param loop_ivs 循环归纳变量信息
 */
extern void replace_derived_variable_uses(
    DerivedInductionVariable_ptr derived_iv,
    StrengthReductionVariable_ptr sr_var,
    LoopInductionVariables_ptr loop_ivs);

/**
 * @brief 打印强度削减变量信息
 * @param sr_var 强度削减变量
 * @param out 输出文件流
 */
extern void StrengthReductionVariable_print(StrengthReductionVariable_ptr sr_var, FILE *out);

//// ================================== 辅助函数 ==================================

/**
 * @brief 初始化基本归纳变量结构体
 * @param basic_iv 指向要初始化的基本归纳变量的指针
 * @param variable 归纳变量
 * @param increment_block 增量操作所在的基本块
 * @param increment_stmt 增量操作语句
 * @param step 步长值
 * @param is_increment 是否为递增操作
 */
extern void BasicInductionVariable_init(BasicInductionVariable_ptr basic_iv,
                                       IR_var variable,
                                       IR_block_ptr increment_block,
                                       IR_stmt *increment_stmt,
                                       int step,
                                       bool is_increment);

/**
 * @brief 初始化派生归纳变量结构体
 * @param derived_iv 指向要初始化的派生归纳变量的指针
 * @param variable 派生归纳变量
 * @param basic_iv 对应的基本归纳变量
 * @param coefficient 系数
 * @param constant 常数项
 * @param definition_stmt 定义语句
 */
extern void DerivedInductionVariable_init(DerivedInductionVariable_ptr derived_iv,
                                         IR_var variable,
                                         BasicInductionVariable_ptr basic_iv,
                                         int coefficient,
                                         int constant,
                                         IR_stmt *definition_stmt);

/**
 * @brief 初始化循环归纳变量信息结构体
 * @param loop_ivs 指向要初始化的循环归纳变量信息的指针
 * @param loop 对应的循环
 */
extern void LoopInductionVariables_init(LoopInductionVariables_ptr loop_ivs, Loop_ptr loop);

/**
 * @brief 析构循环归纳变量信息结构体
 * @param loop_ivs 指向要析构的循环归纳变量信息的指针
 */
extern void LoopInductionVariables_teardown(LoopInductionVariables_ptr loop_ivs);

/**
 * @brief 演示强度削减优化机会
 * @param function 要分析的函数
 * @param loop_analyzer 循环分析器结果
 */
extern void demonstrate_strength_reduction(IR_function_ptr function, LoopAnalyzer *loop_analyzer);

#endif //CODE_INDUCTION_VARIABLE_ANALYSIS_H
