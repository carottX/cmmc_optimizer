//
// Created by Assistant
// 循环分析 (Loop Analysis) - 基于支配节点
//

#ifndef CODE_LOOP_ANALYSIS_H
#define CODE_LOOP_ANALYSIS_H

#include <IR.h>
#include <dominance_analysis.h>
#include <container/list.h>
#include <container/treap.h>

//// ================================== 循环数据结构 ==================================

// 前向声明
typedef struct Loop Loop;
typedef Loop* Loop_ptr;

// 定义Loop_ptr的列表类型
DEF_LIST(Loop_ptr)

/**
 * @brief 回边结构体
 * 表示控制流图中的一条回边
 */
typedef struct BackEdge {
    IR_block_ptr source;                    // 回边源节点
    IR_block_ptr target;                    // 回边目标节点（通常是循环头）
} BackEdge;

DEF_LIST(BackEdge)  // 定义回边列表类型

// 定义从基本块到循环的映射
DEF_MAP(IR_block_ptr, Loop_ptr)

/**
 * @brief 循环结构体
 * 表示程序中的一个自然循环
 */
typedef struct Loop {
    IR_block_ptr header;                    // 循环头节点（唯一入口）
    Set_IR_block_ptr blocks;                // 构成循环的所有基本块集合
    List_IR_block_ptr back_edges_sources;   // 指向头节点的回边的源节点列表
    
    // 嵌套循环支持
    struct Loop* parent_loop;               // 父循环（如果当前循环嵌套在其他循环中）
    List_Loop_ptr nested_loops;             // 直接嵌套在当前循环中的子循环列表
    
    // 循环预备首部（可选，用于优化）
    IR_block_ptr preheader;                 // 循环的预备首部（插入循环不变代码的位置）
    
    // 循环深度和其他属性
    int depth;                              // 循环嵌套深度（从1开始）
    bool is_reducible;                      // 是否为可约循环
} Loop;

/**
 * @brief 循环分析器
 * 管理整个函数的循环检测和分析
 */
typedef struct LoopAnalyzer {
    IR_function *function;                  // 当前分析的函数
    DominanceAnalyzer *dom_analyzer;        // 支配节点分析器（必需）
    
    List_Loop_ptr all_loops;                // 函数中发现的所有循环
    List_Loop_ptr top_level_loops;          // 顶层循环（非嵌套循环）
    List_BackEdge back_edges;               // 函数中的所有回边
    
    Map_IR_block_ptr_Loop_ptr block_to_loop; // 基本块到其所属最内层循环的映射
} LoopAnalyzer;

//// ================================== 循环分析 API ==================================

/**
 * @brief 初始化循环分析器
 * @param analyzer 指向要初始化的循环分析器的指针
 * @param func 要分析的函数
 * @param dom_analyzer 已完成分析的支配节点分析器
 */
extern void LoopAnalyzer_init(LoopAnalyzer *analyzer, IR_function *func, DominanceAnalyzer *dom_analyzer);

/**
 * @brief 析构循环分析器，释放相关资源
 * @param analyzer 指向要析构的分析器的指针
 */
extern void LoopAnalyzer_teardown(LoopAnalyzer *analyzer);

/**
 * @brief 执行循环检测分析
 * 识别函数中的所有自然循环
 * @param analyzer 循环分析器
 */
extern void LoopAnalyzer_detect_loops(LoopAnalyzer *analyzer);

/**
 * @brief 构建循环嵌套层次结构
 * 在检测到循环后调用，建立循环间的父子关系
 * @param analyzer 循环分析器
 */
extern void LoopAnalyzer_build_loop_hierarchy(LoopAnalyzer *analyzer);

/**
 * @brief 为循环创建预备首部
 * 为需要的循环插入预备首部基本块
 * @param analyzer 循环分析器
 */
extern void LoopAnalyzer_create_preheaders(LoopAnalyzer *analyzer);

//// ================================== 查询接口 ==================================

/**
 * @brief 检查基本块是否在指定循环中
 * @param loop 循环指针
 * @param block 基本块指针
 * @return 如果基本块在循环中返回true，否则返回false
 */
extern bool Loop_contains_block(Loop *loop, IR_block_ptr block);

/**
 * @brief 获取基本块所属的最内层循环
 * @param analyzer 循环分析器
 * @param block 基本块指针
 * @return 最内层循环指针，如果不在任何循环中返回NULL
 */
extern Loop_ptr LoopAnalyzer_get_innermost_loop(LoopAnalyzer *analyzer, IR_block_ptr block);

/**
 * @brief 检查两个循环是否有嵌套关系
 * @param outer 潜在的外层循环
 * @param inner 潜在的内层循环
 * @return 如果inner嵌套在outer中返回true，否则返回false
 */
extern bool Loop_is_nested_in(Loop *inner, Loop *outer);

/**
 * @brief 获取循环的所有退出基本块
 * 退出基本块是循环内的块，但有后继块在循环外
 * @param loop 循环指针
 * @param exits 用于存储退出块的列表（调用者负责初始化）
 */
extern void Loop_get_exit_blocks(Loop *loop, List_IR_block_ptr *exits);

/**
 * @brief 获取循环的所有外部后继基本块
 * 外部后继是循环外的块，但有前驱块在循环内
 * @param loop 循环指针
 * @param exit_targets 用于存储外部后继块的列表（调用者负责初始化）
 */
extern void Loop_get_exit_targets(Loop *loop, List_IR_block_ptr *exit_targets);

//// ================================== 结果输出 ==================================

/**
 * @brief 打印循环分析结果
 * @param analyzer 循环分析器
 * @param out 输出文件流
 */
extern void LoopAnalyzer_print_result(LoopAnalyzer *analyzer, FILE *out);

/**
 * @brief 打印循环嵌套层次结构
 * @param analyzer 循环分析器
 * @param out 输出文件流
 */
extern void LoopAnalyzer_print_loop_hierarchy(LoopAnalyzer *analyzer, FILE *out);

/**
 * @brief 打印单个循环的详细信息
 * @param loop 循环指针
 * @param out 输出文件流
 * @param indent 缩进级别
 */
extern void Loop_print_details(Loop *loop, FILE *out, int indent);

//// ================================== 高层接口 ==================================

/**
 * @brief 对指定函数执行完整的循环分析
 * 包括循环检测、层次构建和预备首部创建
 * @param func 要分析的IR函数
 * @param dom_analyzer 已完成的支配节点分析器
 */
extern void perform_loop_analysis(IR_function *func, DominanceAnalyzer *dom_analyzer);

/**
 * @brief 对所有函数执行循环分析
 * 自动进行支配节点分析，然后执行循环分析
 */
extern void analyze_all_functions_loops();

//// ================================== 辅助函数 ==================================

/**
 * @brief 初始化循环结构体
 * @param loop 指向要初始化的循环的指针
 * @param header 循环头节点
 */
extern void Loop_init(Loop *loop, IR_block_ptr header);

/**
 * @brief 析构循环结构体
 * @param loop 指向要析构的循环的指针
 */
extern void Loop_teardown(Loop *loop);

/**
 * @brief 向循环中添加基本块
 * @param loop 循环指针
 * @param block 要添加的基本块
 */
extern void Loop_add_block(Loop *loop, IR_block_ptr block);

/**
 * @brief 向循环中添加回边源
 * @param loop 循环指针
 * @param source 回边源基本块
 */
extern void Loop_add_back_edge_source(Loop *loop, IR_block_ptr source);

/**
 * @brief 设置循环的父循环
 * @param child 子循环
 * @param parent 父循环
 */
extern void Loop_set_parent(Loop *child, Loop *parent);

#endif //CODE_LOOP_ANALYSIS_H
