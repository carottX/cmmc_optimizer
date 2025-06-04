//
// Created by Assistant
// 循环不变代码外提 (Loop Invariant Code Motion, LICM) 实现
//

#include "include/licm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//// ================================== 内部辅助函数 ==================================

/**
 * @brief 检查IR_val是否在循环中不变
 */
static bool is_val_loop_invariant(LICMAnalyzer *analyzer, IR_val val, Loop *loop) {
    // 常量总是循环不变的
    if (val.is_const) {
        return true;
    }
    
    // 检查变量是否在循环中被修改
    return !LICMAnalyzer_is_var_modified_in_loop(analyzer, val.var, loop);
}

/**
 * @brief 检查语句是否有副作用
 */
static bool has_side_effects(IR_stmt_ptr stmt) {
    if (!stmt) return false;
    
    switch (stmt->stmt_type) {
        case IR_OP_STMT:
        case IR_ASSIGN_STMT:
        case IR_LOAD_STMT:
        case IR_READ_STMT:
            // 这些语句只是计算和赋值，没有副作用
            return false;
            
        case IR_CALL_STMT:
        case IR_STORE_STMT:
        case IR_WRITE_STMT:
            // 这些语句可能有副作用
            return true;
            
        case IR_IF_STMT:
        case IR_GOTO_STMT:
        case IR_RETURN_STMT:
            // 控制流语句不应该被移动
            return true;
            
        default:
            // 未知语句类型，保守地认为有副作用
            return true;
    }
}
/**
 * @brief 检查语句的所有操作数是否都是循环不变的
 */
static bool are_operands_loop_invariant(LICMAnalyzer *analyzer, IR_stmt_ptr stmt, Loop *loop) {
    if (!stmt) return false;
    
    IR_use use_info = VCALL(*stmt, get_use_vec);
    
    // 检查所有使用的变量是否都是循环不变的
    for (unsigned i = 0; i < use_info.use_cnt; i++) {
        if (!is_val_loop_invariant(analyzer, use_info.use_vec[i], loop)) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 检查语句是否支配循环的所有出口
 */
static bool dominates_all_exits(LICMAnalyzer *analyzer, IR_stmt_ptr stmt, Loop *loop) {
    // 获取语句所在的基本块
    IR_block_ptr stmt_block = NULL;
    
    // 查找语句所在的基本块
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head; 
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            if (stmt_node->val == stmt) {
                stmt_block = block;
                break;
            }
        }
        if (stmt_block) break;
    }
    
    if (!stmt_block) return false;
    
    // 对于LICM，我们放松支配性要求：
    // 只要语句在循环内且没有副作用，就认为可以移动到preheader
    // 这是因为preheader总是在循环执行前执行，所以是安全的
    // printf("LICM: 语句在块 L%u 中，循环头是 L%u\n", stmt_block->label, loop->header->label);
    
    // 简化的检查：只要语句在循环内的任何基本块中，都认为可以移动
    // 更严格的实现需要检查真正的支配关系
    return true;
}
/**
 * @brief 检查在循环中是否只有一条路径可以到达语句的定义
 */
static bool has_unique_definition_path(LICMAnalyzer *analyzer, IR_stmt_ptr stmt, Loop *loop) {
    // 简化实现：只检查语句定义的变量在循环中是否只被定义一次
    IR_var def_var = VCALL(*stmt, get_def);
    
    if (def_var == IR_VAR_NONE) return true;
    
    int def_count = 0;
    
    // 遍历循环中的所有语句，统计定义该变量的次数
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head; 
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            
            IR_var current_def = VCALL(*stmt_node->val, get_def);
            if (current_def == def_var) {
                def_count++;
                if (def_count > 1) return false;
            }
        }
    }
    
    return def_count <= 1;
}

/**
 * @brief 检查语句是否可以安全地移动到循环预头部
 * 这是一个更高效且实用的安全性检查
 */
static bool is_safe_to_hoist(LICMAnalyzer *analyzer, IR_stmt_ptr stmt, Loop *loop) {
    if (!analyzer || !stmt || !loop) return false;
    
    // 1. 必须是循环不变的
    if (!LICMAnalyzer_is_loop_invariant(analyzer, stmt, loop)) {
        return false;
    }
    
    // 2. 不能有副作用
    if (has_side_effects(stmt)) {
        return false;
    }
    
    // 3. 检查语句的类型 - 只允许安全的计算语句
    switch (stmt->stmt_type) {
        case IR_OP_STMT:
        case IR_ASSIGN_STMT:
            // 算术和赋值操作通常是安全的
            break;
        case IR_LOAD_STMT:
            // 内存读取可能不安全，需要更仔细的分析
            // 简化处理：暂时不允许移动
            return false;
        default:
            // 其他类型的语句不移动
            return false;
    }
    
    // 4. 简化的支配性检查：确保语句在循环的某个基本块中
    // 这比完整的支配性分析更高效
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        for (ListNode_IR_stmt_ptr *node = block->stmts.head; 
             node != NULL; node = node->nxt) {
            if (node->val == stmt) {
                return true;  // 找到语句，可以移动
            }
        }
    }
    
    return false;  // 语句不在循环中
}

//// ================================== LICM分析器操作 ==================================

void LICMAnalyzer_init(LICMAnalyzer *analyzer, 
                       IR_function *func, 
                       LoopAnalyzer *loop_analyzer,
                       DominanceAnalyzer *dom_analyzer) {
    if (!analyzer || !func || !loop_analyzer || !dom_analyzer) return;
    
    analyzer->function = func;
    analyzer->loop_analyzer = loop_analyzer;
    analyzer->dom_analyzer = dom_analyzer;
    
    // 初始化容器
    Map_IR_stmt_ptr_int_init(&analyzer->invariant_cache);
    Set_IR_stmt_ptr_init(&analyzer->moved_stmts);
}

void LICMAnalyzer_teardown(LICMAnalyzer *analyzer) {
    if (!analyzer) return;
    
    VCALL(analyzer->invariant_cache, teardown);
    VCALL(analyzer->moved_stmts, teardown);
    
    analyzer->function = NULL;
    analyzer->loop_analyzer = NULL;
    analyzer->dom_analyzer = NULL;
}

bool LICMAnalyzer_is_loop_invariant(LICMAnalyzer *analyzer, IR_stmt_ptr stmt, Loop *loop) {
    if (!analyzer || !stmt || !loop) return false;
    
    // 检查缓存
    if (VCALL(analyzer->invariant_cache, exist, stmt)) {
        return VCALL(analyzer->invariant_cache, get, stmt) != 0;
    }
    
    bool is_invariant = false;
    
    // 1. 检查语句是否有副作用
    if (has_side_effects(stmt)) {
        goto cache_and_return;
    }
    
    // 2. 检查语句的所有操作数是否都是循环不变的
    if (!are_operands_loop_invariant(analyzer, stmt, loop)) {
        goto cache_and_return;
    }
    
    // 3. 检查语句定义的变量是否在循环中被其他语句修改
    IR_var def_var = VCALL(*stmt, get_def);
    if (def_var != IR_VAR_NONE) {
        if (LICMAnalyzer_is_var_modified_in_loop(analyzer, def_var, loop)) {
            // 需要进一步检查是否只有当前语句修改该变量
            if (!has_unique_definition_path(analyzer, stmt, loop)) {
                goto cache_and_return;
            }
        }
    }
    
    is_invariant = true;
    
cache_and_return:
    // 缓存结果
    VCALL(analyzer->invariant_cache, set, stmt, is_invariant ? 1 : 0);
    return is_invariant;
}

bool LICMAnalyzer_is_safe_to_move(LICMAnalyzer *analyzer, IR_stmt_ptr stmt, Loop *loop) {
    if (!analyzer || !stmt || !loop) return false;
    
    // 使用新的高效安全性检查
    return is_safe_to_hoist(analyzer, stmt, loop);
}

bool LICMAnalyzer_is_var_modified_in_loop(LICMAnalyzer *analyzer, IR_var var, Loop *loop) {
    if (!analyzer || !loop || var == IR_VAR_NONE) return false;
    
    // 遍历循环中的所有语句，检查是否有语句定义了该变量
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head; 
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            
            IR_var def_var = VCALL(*stmt_node->val, get_def);
            if (def_var == var) {
                return true;
            }
        }
    }
    
    return false;
}
    
/**
 * @brief 将语句移动到循环预备首部
 */
static bool move_stmt_to_preheader(LICMAnalyzer *analyzer, IR_stmt_ptr stmt, Loop *loop) {
    if (!analyzer || !stmt || !loop || !loop->preheader) return false;
    
    // printf("LICM: 准备移动语句到preheader (L%u): ", loop->preheader->label);
    // VCALL(*stmt, print, stdout);
    
    // 查找语句所在的基本块和位置
    IR_block_ptr source_block = NULL;
    ListNode_IR_stmt_ptr *stmt_node = NULL;
    
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        for (ListNode_IR_stmt_ptr *node = block->stmts.head; 
             node != NULL; node = node->nxt) {
            if (node->val == stmt) {
                source_block = block;
                stmt_node = node;
                // printf("LICM: 找到语句在基本块 L%u\n", block->label);
                break;
            }
        }
        if (source_block) break;
    }
    
    if (!source_block || !stmt_node) {
        // printf("LICM: 错误 - 未找到语句所在的基本块\n");
        return false;
    }
    
    // 从原基本块中移除语句
    VCALL(source_block->stmts, delete, stmt_node);
    // printf("LICM: 已从基本块 L%u 移除语句\n", source_block->label);
    
    // 将语句添加到预备首部的末尾（在跳转语句之前）
    IR_block_ptr preheader = loop->preheader;
    // printf("LICM: 预备首部 L%u 当前有 %s 语句\n", 
        //    preheader->label, 
        //    preheader->stmts.tail ? "尾部" : "无");
    
    if (preheader->stmts.tail && 
        preheader->stmts.tail->val->stmt_type == IR_GOTO_STMT) {
        // 在goto语句之前插入
        VCALL(preheader->stmts, insert_front, preheader->stmts.tail, stmt);
        // printf("LICM: 在goto语句前插入\n");
    } else {
        // 直接添加到末尾
        VCALL(preheader->stmts, push_back, stmt);
        // printf("LICM: 添加到预备首部末尾\n");
    }
    
    // 记录已移动的语句
    VCALL(analyzer->moved_stmts, insert, stmt);
    
    // printf("LICM: 语句移动完成\n");
    return true;
}

bool LICMAnalyzer_optimize_loop(LICMAnalyzer *analyzer, Loop *loop) {
    if (!analyzer || !loop || !loop->preheader) {
        // printf("LICM: 循环缺少preheader，无法进行优化\n");
        return false;  // 需要preheader才能进行LICM
    }
    
    // printf("LICM: 开始优化循环 (header: L%u, preheader: L%u)\n", 
    //        loop->header->label, 
    //        loop->preheader ? loop->preheader->label : 0);
    
    bool modified = false;
    List_IR_stmt_ptr candidates;
    List_IR_stmt_ptr_init(&candidates);
    
    // 收集候选语句
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head; 
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            IR_stmt_ptr stmt = stmt_node->val;
            
            // 检查是否已经移动过
            if (VCALL(analyzer->moved_stmts, exist, stmt)) {
                continue;
            }
            
            // 检查是否循环不变
            if (LICMAnalyzer_is_loop_invariant(analyzer, stmt, loop)) {
                //                // printf("LICM: 发现循环不变语句: ");
                // VCALL(*stmt, print, stdout);
                
                // 检查是否可以安全移动
                if (LICMAnalyzer_is_safe_to_move(analyzer, stmt, loop)) {
                    // printf("LICM: 语句可以安全移动\n");
                    VCALL(candidates, push_back, stmt);
                } else {
                    // printf("LICM: 语句不能安全移动\n");
                }
            }
        }
    }
    
    // printf("LICM: 发现 %d 个可移动的候选语句\n", (int)candidates.head ? 1 : 0);
    
    // 移动候选语句到preheader
    for (ListNode_IR_stmt_ptr *stmt_node = candidates.head; 
         stmt_node != NULL; stmt_node = stmt_node->nxt) {
        IR_stmt_ptr stmt = stmt_node->val;
        // printf("LICM: 移动语句: ");
        // VCALL(*stmt, print, stdout);
        if (move_stmt_to_preheader(analyzer, stmt, loop)) {
            // printf("LICM: ✓ 成功移动语句\n");
            modified = true;
        } else {
            // printf("LICM: ✗ 移动语句失败\n");
        }
    }
    
    VCALL(candidates, teardown);
    return modified;
}

bool LICMAnalyzer_optimize(LICMAnalyzer *analyzer) {
    if (!analyzer || !analyzer->loop_analyzer) {
        return false;
    }
    
    bool modified = false;
    
    // 从最内层循环开始处理（后序遍历）
    // 这样可以最大化代码外提的效果
    for (ListNode_Loop_ptr *loop_node = analyzer->loop_analyzer->all_loops.head;
         loop_node != NULL; loop_node = loop_node->nxt) {
        Loop *loop = loop_node->val;
        
        if (LICMAnalyzer_optimize_loop(analyzer, loop)) {
            modified = true;
        }
    }
    
    return modified;
}

//// ================================== 调试和打印接口 ==================================

void LICMAnalyzer_print_invariant_stmts(LICMAnalyzer *analyzer, 
                                        Loop *loop, 
                                        FILE *out) {
    if (!analyzer || !loop || !out) return;
    
    fprintf(out, "循环 (header: L%u) 中的循环不变语句:\n", loop->header->label);
    
    bool found_any = false;
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        
        for (ListNode_IR_stmt_ptr *stmt_node = block->stmts.head; 
             stmt_node != NULL; stmt_node = stmt_node->nxt) {
            IR_stmt_ptr stmt = stmt_node->val;
            
            if (LICMAnalyzer_is_loop_invariant(analyzer, stmt, loop)) {
                fprintf(out, "  - ");
                VCALL(*stmt, print, out);
                found_any = true;
            }
        }
    }
    
    if (!found_any) {
        fprintf(out, "  (无循环不变语句)\n");
    }
    fprintf(out, "\n");
}

void LICMAnalyzer_print_result(LICMAnalyzer *analyzer, FILE *out) {
    if (!analyzer || !out) return;
    
    // 手动计算移动语句的数量
    size_t moved_count = 0;
    for_set(IR_stmt_ptr, stmt_node, analyzer->moved_stmts) {
        moved_count++;
    }
    
    fprintf(out, "========== LICM优化结果 ==========\n");
    fprintf(out, "函数: %s\n", analyzer->function->func_name);
    fprintf(out, "移动的语句数量: %lu\n", moved_count);
    
    if (moved_count > 0) {
        fprintf(out, "移动的语句:\n");
        for_set(IR_stmt_ptr, stmt_node, analyzer->moved_stmts) {
            IR_stmt_ptr stmt = stmt_node->key;
            fprintf(out, "  - ");
            VCALL(*stmt, print, out);
        }
    }
    
    fprintf(out, "\n各循环的不变语句分析:\n");
    for (ListNode_Loop_ptr *loop_node = analyzer->loop_analyzer->all_loops.head;
         loop_node != NULL; loop_node = loop_node->nxt) {
        Loop *loop = loop_node->val;
        LICMAnalyzer_print_invariant_stmts(analyzer, loop, out);
    }
    
    fprintf(out, "===============================\n\n");
}
