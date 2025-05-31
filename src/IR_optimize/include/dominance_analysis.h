//
// Created by Assistant
// 支配节点分析 (Dominance Analysis)
//

#ifndef CODE_DOMINANCE_ANALYSIS_H
#define CODE_DOMINANCE_ANALYSIS_H

#include <IR.h>
#include <container/list.h>
#include <container/treap.h>

//// ================================== 支配节点分析数据结构 ==================================

// // 为指针类型定义比较函数
// static inline int IR_block_ptr_cmp(IR_block_ptr a, IR_block_ptr b) {
//     return (a == b) ? 0 : (a < b ? -1 : 1);
// }

// 定义IR_block_ptr的集合类型，使用自定义比较函数
DEF_SET(IR_block_ptr)

/**
 * @brief 支配节点分析结果结构体
 * 记录每个基本块的支配信息
 */
typedef struct DominanceInfo {
    IR_block_ptr block;                    // 当前基本块
    Set_IR_block_ptr dominators;           // 支配当前块的所有基本块集合
    IR_block_ptr immediate_dominator;      // 直接支配节点 (immediate dominator)
    Set_IR_block_ptr dominated_blocks;     // 被当前块支配的基本块集合
    List_IR_block_ptr children_in_dom_tree; // 在支配树中的直接子节点
} DominanceInfo;

DEF_MAP(IR_block_ptr, DominanceInfo)   // 定义从基本块到支配信息的映射

/**
 * @brief 支配节点分析器
 */
typedef struct DominanceAnalyzer {
    IR_function *function;                      // 当前分析的函数
    Map_IR_block_ptr_DominanceInfo dom_info;   // 每个基本块的支配信息映射
    IR_block_ptr entry_block;                  // 入口基本块
} DominanceAnalyzer;

//// ================================== 支配节点分析 API ==================================

/**
 * @brief 初始化支配节点分析器
 * @param analyzer 指向要初始化的分析器的指针
 * @param func 要分析的函数
 */
extern void DominanceAnalyzer_init(DominanceAnalyzer *analyzer, IR_function *func);

/**
 * @brief 析构支配节点分析器，释放相关资源
 * @param analyzer 指向要析构的分析器的指针
 */
extern void DominanceAnalyzer_teardown(DominanceAnalyzer *analyzer);

/**
 * @brief 计算函数中所有基本块的支配关系
 * 使用数据流方程求解支配集合
 * @param analyzer 支配节点分析器
 */
extern void DominanceAnalyzer_compute_dominators(DominanceAnalyzer *analyzer);

/**
 * @brief 构建支配树 (Dominator Tree)
 * 在计算支配关系后调用，构建支配树结构
 * @param analyzer 支配节点分析器
 */
extern void DominanceAnalyzer_build_dominator_tree(DominanceAnalyzer *analyzer);

/**
 * @brief 检查节点A是否支配节点B
 * @param analyzer 支配节点分析器
 * @param dominator 潜在的支配节点A
 * @param dominated 被支配的节点B
 * @return 如果A支配B返回true，否则返回false
 */
extern bool DominanceAnalyzer_dominates(DominanceAnalyzer *analyzer, 
                                        IR_block_ptr dominator, 
                                        IR_block_ptr dominated);

/**
 * @brief 获取基本块的直接支配节点
 * @param analyzer 支配节点分析器
 * @param block 目标基本块
 * @return 直接支配节点，如果是入口节点则返回NULL
 */
extern IR_block_ptr DominanceAnalyzer_get_immediate_dominator(DominanceAnalyzer *analyzer, 
                                                              IR_block_ptr block);

/**
 * @brief 获取基本块的所有支配节点
 * @param analyzer 支配节点分析器
 * @param block 目标基本块
 * @return 支配节点集合的指针
 */
extern Set_IR_block_ptr* DominanceAnalyzer_get_dominators(DominanceAnalyzer *analyzer, 
                                                          IR_block_ptr block);

/**
 * @brief 获取被指定基本块支配的所有基本块
 * @param analyzer 支配节点分析器
 * @param block 支配节点
 * @return 被支配基本块集合的指针
 */
extern Set_IR_block_ptr* DominanceAnalyzer_get_dominated_blocks(DominanceAnalyzer *analyzer, 
                                                               IR_block_ptr block);

/**
 * @brief 打印支配分析结果
 * @param analyzer 支配节点分析器
 * @param out 输出文件流
 */
extern void DominanceAnalyzer_print_result(DominanceAnalyzer *analyzer, FILE *out);

/**
 * @brief 打印支配树结构
 * @param analyzer 支配节点分析器
 * @param out 输出文件流
 */
extern void DominanceAnalyzer_print_dominator_tree(DominanceAnalyzer *analyzer, FILE *out);

/**
 * @brief 对指定函数执行支配节点分析并输出结果
 * @param func 要分析的IR函数
 */
extern void perform_dominance_analysis(IR_function *func);

/**
 * @brief 对所有函数执行支配节点分析
 */
extern void analyze_all_functions_dominance();

//// ================================== 辅助函数 ==================================

/**
 * @brief 初始化支配信息结构体
 * @param info 指向要初始化的支配信息的指针
 * @param block 对应的基本块
 */
extern void DominanceInfo_init(DominanceInfo *info, IR_block_ptr block);

/**
 * @brief 析构支配信息结构体
 * @param info 指向要析构的支配信息的指针
 */
extern void DominanceInfo_teardown(DominanceInfo *info);

#endif //CODE_DOMINANCE_ANALYSIS_H
