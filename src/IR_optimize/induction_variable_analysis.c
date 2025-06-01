//
// Created by Assistant
// 归纳变量分析实现 (Induction Variable Analysis Implementation)
// 基于循环分析识别基本归纳变量和派生归纳变量
//

#include "include/induction_variable_analysis.h"
#include "include/loop_analysis.h"
#include "include/dominance_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//// ================================== 容器类型定义 ==================================

// 为了支持容器操作，需要定义比较函数等
int BasicInductionVariable_ptr_cmp(const void *a, const void *b) {
    BasicInductionVariable_ptr *ptr_a = (BasicInductionVariable_ptr*)a;
    BasicInductionVariable_ptr *ptr_b = (BasicInductionVariable_ptr*)b;
    
    if ((*ptr_a)->variable < (*ptr_b)->variable) return -1;
    if ((*ptr_a)->variable > (*ptr_b)->variable) return 1;
    return 0;
}

int DerivedInductionVariable_ptr_cmp(const void *a, const void *b) {
    DerivedInductionVariable_ptr *ptr_a = (DerivedInductionVariable_ptr*)a;
    DerivedInductionVariable_ptr *ptr_b = (DerivedInductionVariable_ptr*)b;
    
    if ((*ptr_a)->variable < (*ptr_b)->variable) return -1;
    if ((*ptr_a)->variable > (*ptr_b)->variable) return 1;
    return 0;
}

//// ================================== 归纳变量结构体操作 ==================================

void BasicInductionVariable_init(BasicInductionVariable_ptr basic_iv,
                                IR_var variable,
                                IR_block_ptr increment_block,
                                IR_stmt *increment_stmt,
                                int step,
                                bool is_increment) {
    if (!basic_iv) return;
    
    basic_iv->variable = variable;
    basic_iv->increment_block = increment_block;
    basic_iv->increment_stmt = increment_stmt;
    basic_iv->step = step;
    basic_iv->is_increment = is_increment;
}

void DerivedInductionVariable_init(DerivedInductionVariable_ptr derived_iv,
                                  IR_var variable,
                                  BasicInductionVariable_ptr basic_iv,
                                  int coefficient,
                                  int constant,
                                  IR_stmt *definition_stmt) {
    if (!derived_iv) return;
    
    derived_iv->variable = variable;
    derived_iv->basic_iv = basic_iv;
    derived_iv->coefficient = coefficient;
    derived_iv->constant = constant;
    derived_iv->definition_stmt = definition_stmt;
}

void LoopInductionVariables_init(LoopInductionVariables_ptr loop_ivs, Loop_ptr loop) {
    if (!loop_ivs || !loop) return;
    
    loop_ivs->loop = loop;
    List_BasicInductionVariable_ptr_init(&loop_ivs->basic_ivs);
    List_DerivedInductionVariable_ptr_init(&loop_ivs->derived_ivs);
    Map_IR_var_BasicInductionVariable_ptr_init(&loop_ivs->basic_iv_map);
    Map_IR_var_DerivedInductionVariable_ptr_init(&loop_ivs->derived_iv_map);
}

void LoopInductionVariables_teardown(LoopInductionVariables_ptr loop_ivs) {
    if (!loop_ivs) return;
    
    // 释放基本归纳变量
    ListNode_BasicInductionVariable_ptr *basic_node = loop_ivs->basic_ivs.head;
    while (basic_node) {
        free(basic_node->val);
        basic_node = basic_node->nxt;
    }
    
    // 释放派生归纳变量
    ListNode_DerivedInductionVariable_ptr *derived_node = loop_ivs->derived_ivs.head;
    while (derived_node) {
        free(derived_node->val);
        derived_node = derived_node->nxt;
    }
    
    List_BasicInductionVariable_ptr_teardown(&loop_ivs->basic_ivs);
    List_DerivedInductionVariable_ptr_teardown(&loop_ivs->derived_ivs);
    Map_IR_var_BasicInductionVariable_ptr_teardown(&loop_ivs->basic_iv_map);
    Map_IR_var_DerivedInductionVariable_ptr_teardown(&loop_ivs->derived_iv_map);
    
    loop_ivs->loop = NULL;
}

//// ================================== 归纳变量分析器操作 ==================================

void InductionVariableAnalyzer_init(InductionVariableAnalyzer *analyzer, 
                                   IR_function *func, 
                                   LoopAnalyzer *loop_analyzer) {
    if (!analyzer || !func || !loop_analyzer) return;
    
    analyzer->function = func;
    analyzer->loop_analyzer = loop_analyzer;
    
    List_LoopInductionVariables_ptr_init(&analyzer->loop_ivs);
    Map_IR_var_BasicInductionVariable_ptr_init(&analyzer->global_basic_iv_map);
    Map_IR_var_DerivedInductionVariable_ptr_init(&analyzer->global_derived_iv_map);
}

void InductionVariableAnalyzer_teardown(InductionVariableAnalyzer *analyzer) {
    if (!analyzer) return;
    
    // 释放所有循环归纳变量信息
    ListNode_LoopInductionVariables_ptr *node = analyzer->loop_ivs.head;
    while (node) {
        LoopInductionVariables_teardown(node->val);
        free(node->val);
        node = node->nxt;
    }
    
    List_LoopInductionVariables_ptr_teardown(&analyzer->loop_ivs);
    Map_IR_var_BasicInductionVariable_ptr_teardown(&analyzer->global_basic_iv_map);
    Map_IR_var_DerivedInductionVariable_ptr_teardown(&analyzer->global_derived_iv_map);
    
    analyzer->function = NULL;
    analyzer->loop_analyzer = NULL;
}

//// ================================== 基本归纳变量识别 ==================================

/**
 * @brief 检查语句是否为基本归纳变量的增量操作
 * 形如 i = i + c 或 i = i - c
 */
static bool is_basic_induction_increment(IR_stmt *stmt, 
                                        IR_var *variable, 
                                        int *step, 
                                        bool *is_increment) {
    if (!stmt || stmt->stmt_type != IR_OP_STMT) return false;
    
    IR_op_stmt *op_stmt = (IR_op_stmt*)stmt;
    
    // 检查操作类型是否为加法或减法
    if (op_stmt->op != IR_OP_ADD && op_stmt->op != IR_OP_SUB) return false;
    
    // 检查左操作数是否为变量且与目标变量相同
    if (op_stmt->rs1.is_const || op_stmt->rs1.var != op_stmt->rd) return false;
    
    // 检查右操作数是否为常数
    if (!op_stmt->rs2.is_const) return false;
    
    *variable = op_stmt->rd;
    *step = op_stmt->rs2.const_val;
    *is_increment = (op_stmt->op == IR_OP_ADD);
    
    // 如果是减法，步长应为负数
    if (!(*is_increment)) {
        *step = -(*step);
    }
    
    return true;
}

/**
 * @brief 检查变量在循环中是否只有一个定义点
 */
static bool has_single_definition_in_loop(Loop_ptr loop, IR_var variable, IR_stmt **def_stmt, IR_block_ptr *def_block) {
    int def_count = 0;
    IR_stmt *found_stmt = NULL;
    IR_block_ptr found_block = NULL;
    
    // printf("  检查变量 v%u 在循环中的定义...\n", variable);
    
    // 遍历循环中的所有基本块
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        // 遍历基本块中的所有语句
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head;
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            
            IR_stmt *stmt = stmt_node->val;
            
            // 检查语句是否定义了该变量
            bool defines_variable = false;
            
            switch (stmt->stmt_type) {
                case IR_OP_STMT: {
                    IR_op_stmt *op_stmt = (IR_op_stmt*)stmt;
                    if (op_stmt->rd == variable) {
                        defines_variable = true;
                    }
                    break;
                }
                case IR_ASSIGN_STMT: {
                    IR_assign_stmt *assign_stmt = (IR_assign_stmt*)stmt;
                    if (assign_stmt->rd == variable) {
                        defines_variable = true;
                    }
                    break;
                }
                case IR_CALL_STMT: {
                    IR_call_stmt *call_stmt = (IR_call_stmt*)stmt;
                    if (call_stmt->rd == variable) {
                        defines_variable = true;
                    }
                    break;
                }
                case IR_LOAD_STMT: {
                    IR_load_stmt *load_stmt = (IR_load_stmt*)stmt;
                    if (load_stmt->rd == variable) {
                        defines_variable = true;
                    }
                    break;
                }
                default:
                    // 其他语句类型不定义变量
                    break;
            }
            
            if (defines_variable) {
                def_count++;
                found_stmt = stmt;
                found_block = block;
                
                // printf("    在基本块 B%u 发现变量 v%u 的定义\n", block->label, variable);
                
                // 如果已经有超过一个定义，直接返回false
                if (def_count > 1) {
                    // printf("    变量 v%u 有多个定义点，不是归纳变量\n", variable);
                    return false;
                }
            }
        }
    }
    
    if (def_count == 1) {
        *def_stmt = found_stmt;
        *def_block = found_block;
        // printf("    变量 v%u 在循环中只有一个定义点\n", variable);
        return true;
    }
    
    // printf("    变量 v%u 在循环中没有定义点\n", variable);
    return false;
}

/**
 * @brief 检查变量是否在循环头支配的路径上被定义
 */
static bool is_defined_on_dominated_path(Loop_ptr loop, 
                                        DominanceAnalyzer *dom_analyzer,
                                        IR_var variable, 
                                        IR_stmt *def_stmt, 
                                        IR_block_ptr def_block) {
    // 检查定义块是否被循环头支配
    return DominanceAnalyzer_dominates(dom_analyzer, loop->header, def_block);
}

void InductionVariableAnalyzer_analyze_basic_ivs(InductionVariableAnalyzer *analyzer,
                                                Loop_ptr loop,
                                                LoopInductionVariables_ptr loop_ivs) {
    if (!analyzer || !loop || !loop_ivs) return;
    
    // printf("=== 分析循环 B%u 的基本归纳变量 ===\n", loop->header->label);
    
    // 遍历循环中的所有基本块和语句
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        // printf("  分析基本块 B%u 中的语句...\n", block->label);
        
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head;
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            
            IR_stmt *stmt = stmt_node->val;
            IR_var variable;
            int step;
            bool is_increment;
            
            if (is_basic_induction_increment(stmt, &variable, &step, &is_increment)) {
                // printf("  发现潜在基本归纳变量 v%u, 步长: %d\n", variable, step);
                
                // 检查该变量在循环中是否只有一个定义点
                IR_stmt *def_stmt;
                IR_block_ptr def_block;
                if (has_single_definition_in_loop(loop, variable, &def_stmt, &def_block)) {
                    // 检查定义是否在循环头支配的路径上
                    if (is_defined_on_dominated_path(loop, analyzer->loop_analyzer->dom_analyzer, 
                                                    variable, def_stmt, def_block)) {
                        
                        // 检查是否已经存在
                        if (!VCALL(loop_ivs->basic_iv_map, exist, variable)) {
                            // 创建基本归纳变量
                            BasicInductionVariable_ptr basic_iv = 
                                (BasicInductionVariable_ptr)malloc(sizeof(BasicInductionVariable));
                            BasicInductionVariable_init(basic_iv, variable, block, 
                                                      stmt, step, is_increment);
                            
                            // 添加到循环归纳变量信息中
                            VCALL(loop_ivs->basic_ivs, push_back, basic_iv);
                            VCALL(loop_ivs->basic_iv_map, set, variable, basic_iv);
                            VCALL(analyzer->global_basic_iv_map, set, variable, basic_iv);
                            
                            // printf("  确认基本归纳变量 v%u, 步长: %d (%s)\n", 
                                //    variable, step, is_increment ? "递增" : "递减");
                        }
                    } else {
                        // printf("  变量 v%u 的定义不在循环头支配的路径上\n", variable);
                    }
                }
            }
        }
    }
    
    // 统计结果
    int basic_count = 0;
    for (ListNode_BasicInductionVariable_ptr *node = loop_ivs->basic_ivs.head;
         node != NULL; node = node->nxt) {
        basic_count++;
    }
    
    // printf("=== 循环 B%u 基本归纳变量分析完成，发现 %d 个基本归纳变量 ===\n\n", 
    //        loop->header->label, basic_count);
}

//// ================================== 派生归纳变量识别 ==================================

/**
 * @brief 检查语句是否为派生归纳变量的定义
 * 形如 j = c1 * i + c2，其中 i 是基本归纳变量
 */
static bool is_derived_induction_definition(IR_stmt *stmt,
                                           LoopInductionVariables_ptr loop_ivs,
                                           IR_var *derived_var,
                                           BasicInductionVariable_ptr *basic_iv,
                                           int *coefficient,
                                           int *constant) {
    if (!stmt) return false;
    
    // 检查乘法形式：j = c * i 或 j = i * c
    if (stmt->stmt_type == IR_OP_STMT) {
        IR_op_stmt *op_stmt = (IR_op_stmt*)stmt;
        
        if (op_stmt->op == IR_OP_MUL) {
            // 情况1: j = c * i
            if (op_stmt->rs1.is_const && !op_stmt->rs2.is_const) {
                if (VCALL(loop_ivs->basic_iv_map, exist, op_stmt->rs2.var)) {
                    *derived_var = op_stmt->rd;
                    *basic_iv = VCALL(loop_ivs->basic_iv_map, get, op_stmt->rs2.var);
                    *coefficient = op_stmt->rs1.const_val;
                    *constant = 0;
                    return true;
                }
            }
            // 情况2: j = i * c
            else if (!op_stmt->rs1.is_const && op_stmt->rs2.is_const) {
                if (VCALL(loop_ivs->basic_iv_map, exist, op_stmt->rs1.var)) {
                    *derived_var = op_stmt->rd;
                    *basic_iv = VCALL(loop_ivs->basic_iv_map, get, op_stmt->rs1.var);
                    *coefficient = op_stmt->rs2.const_val;
                    *constant = 0;
                    return true;
                }
            }
        }
        // 检查加法形式：j = i + c 或 j = c + i  
        else if (op_stmt->op == IR_OP_ADD) {
            // 情况1: j = i + c
            if (!op_stmt->rs1.is_const && op_stmt->rs2.is_const) {
                if (VCALL(loop_ivs->basic_iv_map, exist, op_stmt->rs1.var)) {
                    *derived_var = op_stmt->rd;
                    *basic_iv = VCALL(loop_ivs->basic_iv_map, get, op_stmt->rs1.var);
                    *coefficient = 1;
                    *constant = op_stmt->rs2.const_val;
                    return true;
                }
            }
            // 情况2: j = c + i
            else if (op_stmt->rs1.is_const && !op_stmt->rs2.is_const) {
                if (VCALL(loop_ivs->basic_iv_map, exist, op_stmt->rs2.var)) {
                    *derived_var = op_stmt->rd;
                    *basic_iv = VCALL(loop_ivs->basic_iv_map, get, op_stmt->rs2.var);
                    *coefficient = 1;
                    *constant = op_stmt->rs1.const_val;
                    return true;
                }
            }
        }
        // 检查减法形式：j = i - c
        else if (op_stmt->op == IR_OP_SUB) {
            if (!op_stmt->rs1.is_const && op_stmt->rs2.is_const) {
                if (VCALL(loop_ivs->basic_iv_map, exist, op_stmt->rs1.var)) {
                    *derived_var = op_stmt->rd;
                    *basic_iv = VCALL(loop_ivs->basic_iv_map, get, op_stmt->rs1.var);
                    *coefficient = 1;
                    *constant = -op_stmt->rs2.const_val;
                    return true;
                }
            }
        }
    }
    // 检查赋值形式：j = i（系数为1，常数为0）
    else if (stmt->stmt_type == IR_ASSIGN_STMT) {
        IR_assign_stmt *assign_stmt = (IR_assign_stmt*)stmt;
        
        if (!assign_stmt->rs.is_const) {
            if (VCALL(loop_ivs->basic_iv_map, exist, assign_stmt->rs.var)) {
                *derived_var = assign_stmt->rd;
                *basic_iv = VCALL(loop_ivs->basic_iv_map, get, assign_stmt->rs.var);
                *coefficient = 1;
                *constant = 0;
                return true;
            }
        }
    }
    
    return false;
}

void InductionVariableAnalyzer_analyze_derived_ivs(InductionVariableAnalyzer *analyzer,
                                                  Loop_ptr loop,
                                                  LoopInductionVariables_ptr loop_ivs) {
    if (!analyzer || !loop || !loop_ivs) return;
    
    // printf("=== 分析循环 B%u 的派生归纳变量 ===\n", loop->header->label);
    
    // 如果没有基本归纳变量，则没有派生归纳变量
    if (!loop_ivs->basic_ivs.head) {
        // printf("  没有基本归纳变量，跳过派生归纳变量分析\n");
        return;
    }
    
    // 遍历循环中的所有基本块和语句寻找派生归纳变量
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        // printf("  分析基本块 B%u 中的派生归纳变量...\n", block->label);
        
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head;
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            
            IR_stmt *stmt = stmt_node->val;
            IR_var derived_var;
            BasicInductionVariable_ptr basic_iv;
            int coefficient, constant;
            
            if (is_derived_induction_definition(stmt, loop_ivs, &derived_var, 
                                              &basic_iv, &coefficient, &constant)) {
                
                // 检查是否已经存在（避免重复添加）
                if (!VCALL(loop_ivs->derived_iv_map, exist, derived_var) &&
                    !VCALL(loop_ivs->basic_iv_map, exist, derived_var)) {
                    
                    // printf("  在基本块 B%u 发现派生归纳变量 v%u = %d * v%u + %d\n", 
                    //        block->label, derived_var, coefficient, basic_iv->variable, constant);
                    
                    // 创建派生归纳变量
                    DerivedInductionVariable_ptr derived_iv = 
                        (DerivedInductionVariable_ptr)malloc(sizeof(DerivedInductionVariable));
                    DerivedInductionVariable_init(derived_iv, derived_var, basic_iv, 
                                                coefficient, constant, stmt);
                    
                    // 添加到循环归纳变量信息中
                    VCALL(loop_ivs->derived_ivs, push_back, derived_iv);
                    VCALL(loop_ivs->derived_iv_map, set, derived_var, derived_iv);
                    VCALL(analyzer->global_derived_iv_map, set, derived_var, derived_iv);
                } else {
                    // printf("  变量 v%u 已经被识别为归纳变量，跳过\n", derived_var);
                }
            }
        }
    }
    
    // 统计结果
    int derived_count = 0;
    for (ListNode_DerivedInductionVariable_ptr *node = loop_ivs->derived_ivs.head;
         node != NULL; node = node->nxt) {
        derived_count++;
    }
    
    // printf("=== 循环 B%u 派生归纳变量分析完成，发现 %d 个派生归纳变量 ===\n\n", 
    //        loop->header->label, derived_count);
}

//// ================================== 主分析算法 ==================================

void InductionVariableAnalyzer_analyze(InductionVariableAnalyzer *analyzer) {
    if (!analyzer || !analyzer->loop_analyzer) return;
    
    // printf("\n=== 执行归纳变量分析 ===\n");
    // printf("函数: %s\n", analyzer->function->func_name);
    
    // 遍历所有循环
    for_list(Loop_ptr, loop_node, analyzer->loop_analyzer->all_loops){
        
        Loop_ptr loop = loop_node->val;
        
        // 为每个循环创建归纳变量信息结构体
        LoopInductionVariables_ptr loop_ivs = 
            (LoopInductionVariables_ptr)malloc(sizeof(LoopInductionVariables));
        LoopInductionVariables_init(loop_ivs, loop);
        
        // 分析基本归纳变量
        InductionVariableAnalyzer_analyze_basic_ivs(analyzer, loop, loop_ivs);
        
        // 分析派生归纳变量
        InductionVariableAnalyzer_analyze_derived_ivs(analyzer, loop, loop_ivs);
        
        // 添加到分析器的循环列表中
        VCALL(analyzer->loop_ivs, push_back, loop_ivs);
    }
    
    // printf("=== 归纳变量分析完成 ===\n\n");
}

//// ================================== 查询接口实现 ==================================

BasicInductionVariable_ptr InductionVariableAnalyzer_get_basic_iv(
    InductionVariableAnalyzer *analyzer, IR_var variable) {
    if (!analyzer) return NULL;
    
    if (VCALL(analyzer->global_basic_iv_map, exist, variable)) {
        return VCALL(analyzer->global_basic_iv_map, get, variable);
    }
    return NULL;
}

DerivedInductionVariable_ptr InductionVariableAnalyzer_get_derived_iv(
    InductionVariableAnalyzer *analyzer, IR_var variable) {
    if (!analyzer) return NULL;
    
    if (VCALL(analyzer->global_derived_iv_map, exist, variable)) {
        return VCALL(analyzer->global_derived_iv_map, get, variable);
    }
    return NULL;
}

LoopInductionVariables_ptr InductionVariableAnalyzer_get_loop_ivs(
    InductionVariableAnalyzer *analyzer, Loop_ptr loop) {
    if (!analyzer || !loop) return NULL;
    
    for (ListNode_LoopInductionVariables_ptr *node = analyzer->loop_ivs.head;
         node != NULL; node = node->nxt) {
        if (node->val->loop == loop) {
            return node->val;
        }
    }
    return NULL;
}

//// ================================== 结果输出 ==================================

void BasicInductionVariable_print(BasicInductionVariable_ptr basic_iv, FILE *out) {
    if (!basic_iv || !out) return;
    
    fprintf(out, "    基本归纳变量 v%u:\n", basic_iv->variable);
    fprintf(out, "      步长: %d (%s)\n", basic_iv->step, 
            basic_iv->is_increment ? "递增" : "递减");
    fprintf(out, "      增量块: B%u\n", basic_iv->increment_block->label);
    fprintf(out, "      增量语句: ");
    VCALL(*basic_iv->increment_stmt, print, out);
}

void DerivedInductionVariable_print(DerivedInductionVariable_ptr derived_iv, FILE *out) {
    if (!derived_iv || !out) return;
    
    fprintf(out, "    派生归纳变量 v%u:\n", derived_iv->variable);
    fprintf(out, "      表达式: v%u = %d * v%u + %d\n", 
            derived_iv->variable, derived_iv->coefficient, 
            derived_iv->basic_iv->variable, derived_iv->constant);
    fprintf(out, "      基本归纳变量: v%u\n", derived_iv->basic_iv->variable);
    fprintf(out, "      定义语句: ");
    VCALL(*derived_iv->definition_stmt, print, out);
}

void LoopInductionVariables_print(LoopInductionVariables_ptr loop_ivs, FILE *out) {
    if (!loop_ivs || !out) return;
    
    fprintf(out, "  循环 B%u 的归纳变量:\n", loop_ivs->loop->header->label);
    
    // 打印基本归纳变量
    int basic_count = 0;
    for (ListNode_BasicInductionVariable_ptr *node = loop_ivs->basic_ivs.head;
         node != NULL; node = node->nxt) {
        basic_count++;
    }
    
    // fprintf(out, "    基本归纳变量数量: %d\n", basic_count);
    // for (ListNode_BasicInductionVariable_ptr *node = loop_ivs->basic_ivs.head;
    //      node != NULL; node = node->nxt) {
    //     // BasicInductionVariable_print(node->val, out);
    // }
    
    // 打印派生归纳变量
    int derived_count = 0;
    for (ListNode_DerivedInductionVariable_ptr *node = loop_ivs->derived_ivs.head;
         node != NULL; node = node->nxt) {
        derived_count++;
    }
    
    // fprintf(out, "    派生归纳变量数量: %d\n", derived_count);
    // for (ListNode_DerivedInductionVariable_ptr *node = loop_ivs->derived_ivs.head;
    //      node != NULL; node = node->nxt) {
    //     DerivedInductionVariable_print(node->val, out);
    // }
    
    // fprintf(out, "\n");
}

void InductionVariableAnalyzer_print_result(InductionVariableAnalyzer *analyzer, FILE *out) {
    if (!analyzer || !out) return;
    
    // fprintf(out, "\n=== 归纳变量分析结果 ===\n");
    // fprintf(out, "函数: %s\n", analyzer->function->func_name);
    
    // 计算统计信息
    int loop_count = 0, total_basic = 0, total_derived = 0;
    
    for (ListNode_LoopInductionVariables_ptr *node = analyzer->loop_ivs.head;
         node != NULL; node = node->nxt) {
        loop_count++;
        
        // 统计基本归纳变量
        for (ListNode_BasicInductionVariable_ptr *basic_node = node->val->basic_ivs.head;
             basic_node != NULL; basic_node = basic_node->nxt) {
            total_basic++;
        }
        
        // 统计派生归纳变量
        for (ListNode_DerivedInductionVariable_ptr *derived_node = node->val->derived_ivs.head;
             derived_node != NULL; derived_node = derived_node->nxt) {
            total_derived++;
        }
    }
    
    // fprintf(out, "分析循环数: %d\n", loop_count);
    // fprintf(out, "总基本归纳变量数: %d\n", total_basic);
    // fprintf(out, "总派生归纳变量数: %d\n\n", total_derived);
    
    // 打印每个循环的详细信息
    if (!analyzer->loop_ivs.head) {
        // fprintf(out, "没有发现归纳变量\n");
        return;
    }
    // }
    
    // for (ListNode_LoopInductionVariables_ptr *node = analyzer->loop_ivs.head;
    //      node != NULL; node = node->nxt) {
    //     LoopInductionVariables_print(node->val, out);
    // }
    
    // fprintf(out, "\n");
}

//// ================================== 高层接口 ==================================

void perform_induction_variable_analysis(IR_function *func, LoopAnalyzer *loop_analyzer) {
    if (!func || !loop_analyzer) return;
    
    // printf("\n=== 执行归纳变量分析 ===\n");
    // printf("函数: %s\n", func->func_name);
    
    InductionVariableAnalyzer analyzer;
    InductionVariableAnalyzer_init(&analyzer, func, loop_analyzer);
    
    // 执行归纳变量分析
    InductionVariableAnalyzer_analyze(&analyzer);
    
    // 打印结果
    // InductionVariableAnalyzer_print_result(&analyzer, stdout);
    
    InductionVariableAnalyzer_teardown(&analyzer);
}
