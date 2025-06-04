//
// Created by Assistant
// 循环不变代码外提 (Loop Invariant Code Motion, LICM) 头文件
//

#ifndef CODE_LICM_H
#define CODE_LICM_H

#include <IR.h>
#include <loop_analysis.h>
#include <dominance_analysis.h>
#include <container/treap.h>

//// ================================== 容器类型定义 ==================================

// 定义LICM需要的容器类型
DEF_MAP(IR_stmt_ptr, int)  // 语句到int映射，用于缓存 (int: 0=false, 1=true)
DEF_SET(IR_stmt_ptr)       // 语句集合，用于记录已移动的语句

//// ================================== LICM数据结构 ==================================

/**
 * @brief LICM分析器
 * 管理循环不变代码的检测和外提
 */
typedef struct LICMAnalyzer {
    IR_function *function;                      // 当前分析的函数
    LoopAnalyzer *loop_analyzer;                // 循环分析器
    DominanceAnalyzer *dom_analyzer;            // 支配分析器
    
    // 缓存信息用于快速查询
    Map_IR_stmt_ptr_int invariant_cache;        // 语句不变性缓存 (用int: 0=false, 1=true)
    Set_IR_stmt_ptr moved_stmts;                // 已移动的语句集合
} LICMAnalyzer;

//// ================================== LICM API ==================================

/**
 * @brief 初始化LICM分析器
 * @param analyzer 指向要初始化的LICM分析器的指针
 * @param func 要分析的函数
 * @param loop_analyzer 已完成分析的循环分析器
 * @param dom_analyzer 已完成分析的支配分析器
 */
extern void LICMAnalyzer_init(LICMAnalyzer *analyzer, 
                              IR_function *func, 
                              LoopAnalyzer *loop_analyzer,
                              DominanceAnalyzer *dom_analyzer);

/**
 * @brief 析构LICM分析器，释放相关资源
 * @param analyzer 指向要析构的分析器的指针
 */
extern void LICMAnalyzer_teardown(LICMAnalyzer *analyzer);

/**
 * @brief 执行循环不变代码外提优化
 * 对函数中的所有循环执行LICM优化
 * @param analyzer LICM分析器
 * @return 如果有代码被移动返回true，否则返回false
 */
extern bool LICMAnalyzer_optimize(LICMAnalyzer *analyzer);

/**
 * @brief 对单个循环执行LICM优化
 * @param analyzer LICM分析器
 * @param loop 要优化的循环
 * @return 如果有代码被移动返回true，否则返回false
 */
extern bool LICMAnalyzer_optimize_loop(LICMAnalyzer *analyzer, Loop *loop);

//// ================================== 查询接口 ==================================

/**
 * @brief 检查语句是否为循环不变的
 * @param analyzer LICM分析器
 * @param stmt 要检查的语句
 * @param loop 循环指针
 * @return 如果语句是循环不变的返回true，否则返回false
 */
extern bool LICMAnalyzer_is_loop_invariant(LICMAnalyzer *analyzer, 
                                           IR_stmt_ptr stmt, 
                                           Loop *loop);

/**
 * @brief 检查语句是否可以安全地移动到循环外
 * @param analyzer LICM分析器
 * @param stmt 要检查的语句
 * @param loop 循环指针
 * @return 如果语句可以安全移动返回true，否则返回false
 */
extern bool LICMAnalyzer_is_safe_to_move(LICMAnalyzer *analyzer, 
                                         IR_stmt_ptr stmt, 
                                         Loop *loop);

/**
 * @brief 检查变量是否在循环中被修改
 * @param analyzer LICM分析器
 * @param var 要检查的变量
 * @param loop 循环指针
 * @return 如果变量在循环中被修改返回true，否则返回false
 */
extern bool LICMAnalyzer_is_var_modified_in_loop(LICMAnalyzer *analyzer, 
                                                 IR_var var, 
                                                 Loop *loop);

//// ================================== 调试和打印接口 ==================================

/**
 * @brief 打印LICM优化结果
 * @param analyzer LICM分析器
 * @param out 输出文件流
 */
extern void LICMAnalyzer_print_result(LICMAnalyzer *analyzer, FILE *out);

/**
 * @brief 打印循环中的不变语句
 * @param analyzer LICM分析器
 * @param loop 循环指针
 * @param out 输出文件流
 */
extern void LICMAnalyzer_print_invariant_stmts(LICMAnalyzer *analyzer, 
                                               Loop *loop, 
                                               FILE *out);

#endif //CODE_LICM_H
