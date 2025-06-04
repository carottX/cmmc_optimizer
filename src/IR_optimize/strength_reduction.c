//
// Created by Assistant
// 强度削减优化 (Strength Reduction Optimization)
// 将派生归纳变量的乘法操作替换为增量操作
//

#include "include/induction_variable_analysis.h"
#include "include/loop_analysis.h"
#include "include/dominance_analysis.h"
#include <IR.h>
#include <stdio.h>
#include <stdlib.h>
#include <container/list.h>
#include <container/treap.h>

// 前向声明辅助函数
static void replace_variable_in_stmt(IR_stmt *stmt, IR_var old_var, IR_var new_var);
static void replace_variable_in_IR_val(IR_val *val, IR_var old_var, IR_var new_var);
static void remove_useless_derived_iv_definitions(LoopInductionVariables_ptr loop_ivs, List_StrengthReductionVariable_ptr sr_vars);

/**
 * @brief 替换派生变量的使用
 */
void replace_derived_variable_uses(
    DerivedInductionVariable_ptr derived_iv,
    StrengthReductionVariable_ptr sr_var,
    LoopInductionVariables_ptr loop_ivs) {
    
    if (!sr_var || !derived_iv || !loop_ivs) {
        // printf("Error: NULL parameter in replace_derived_variable_uses\n");
        return;
    }
    
    // printf("  Replacing uses of v%u with v%u in loop\n",
    //        derived_iv->variable,
    //        sr_var->new_variable);
    
    Loop_ptr loop = loop_ivs->loop;
    if (!loop) {
        // printf("Error: Loop is NULL in replace_derived_variable_uses\n");
        return;
    }
    
    // 遍历循环中的所有块和语句，替换对derived_iv->variable的使用
    for_set(IR_block_ptr, block_node, loop->blocks) {
        IR_block_ptr block = block_node->key;
        if (!block) {
            // printf("Warning: NULL block found in loop->blocks during replacement\n");
            continue;
        }
        
        // Additional safety check for block validity
        if (block == (IR_block_ptr)0xdeadbeef || 
            block == (IR_block_ptr)0xbaadf00d) {
            // printf("Warning: Invalid block pointer detected: %p\n", block);
            continue;
        }
        
        // 遍历基本块中的所有语句
        for_list(IR_stmt_ptr, stmt_i, block->stmts) {
            IR_stmt *stmt = stmt_i->val;
            
            // 跳过定义语句本身
            if (stmt == derived_iv->definition_stmt) {
                continue;
            }
            
            // 替换语句中对derived_iv->variable的使用
            replace_variable_in_stmt(stmt, derived_iv->variable, sr_var->new_variable);
        }
    }
    
    // printf("  Completed variable replacement\n");
}

/**
 * @brief 为派生归纳变量创建强度削减变量
 */
StrengthReductionVariable_ptr create_strength_reduction_variable(
    InductionVariableAnalyzer *analyzer,
    DerivedInductionVariable_ptr derived_iv,
    LoopInductionVariables_ptr loop_ivs) {
    
    if (!derived_iv || !derived_iv->basic_iv) return NULL;
    
    StrengthReductionVariable_ptr sr_var = malloc(sizeof(StrengthReductionVariable));
    if (!sr_var) return NULL;
    
    // 分配新的变量
    sr_var->new_variable = ir_var_generator();
    sr_var->original_derived_iv = derived_iv;
    sr_var->increment_value = derived_iv->coefficient * derived_iv->basic_iv->step;
    sr_var->initialization_stmt = NULL;
    sr_var->increment_stmt = NULL;
    sr_var->increment_block = NULL;
    
    // printf("Created strength reduction variable v%u for v%u (increment: %d)\n",
    //        sr_var->new_variable,
    //        derived_iv->variable,
    //        sr_var->increment_value);
    
    return sr_var;
}

/**
 * @brief 在循环前序块中创建初始化语句
 */
static void create_initialization_in_preheader(StrengthReductionVariable_ptr sr_var, 
                                               DerivedInductionVariable_ptr derived_iv, 
                                               Loop_ptr loop) {
    if (!sr_var || !derived_iv || !loop) {
        printf("Error: NULL parameter in create_initialization_in_preheader\n");
        return;
    }
    
    if (!loop->preheader) {
        printf("Error: Loop preheader is NULL\n");
        return;
    }
    
    // Additional safety check for preheader
    if (loop->preheader == (IR_block_ptr)0xdeadbeef || 
        loop->preheader == (IR_block_ptr)0xbaadf00d) {
        printf("Error: Loop preheader appears to be invalid pointer: %p\n", loop->preheader);
        return;
    }
    
    // 创建初始化赋值语句：sr_var = coefficient * basic_iv + constant
    // 如果 coefficient == 1，直接创建: sr_var = basic_iv + constant
    // 否则先创建: tmp = coefficient * basic_iv, 然后 sr_var = tmp + constant
    
    if (derived_iv->coefficient == 1) {
        // 简单情况：sr_var = basic_iv + constant
        if (derived_iv->constant == 0) {
            // sr_var = basic_iv
            IR_val basic_iv_val = {.is_const = false, .var = derived_iv->basic_iv->variable};
            IR_assign_stmt *assign_stmt = (IR_assign_stmt*)malloc(sizeof(IR_assign_stmt));
            IR_assign_stmt_init(assign_stmt, sr_var->new_variable, basic_iv_val);
            sr_var->initialization_stmt = (IR_stmt*)assign_stmt;
        } else {
            // sr_var = basic_iv + constant
            IR_val basic_iv_val = {.is_const = false, .var = derived_iv->basic_iv->variable};
            IR_val constant_val = {.is_const = true, .const_val = derived_iv->constant};
            IR_op_stmt *op_stmt = (IR_op_stmt*)malloc(sizeof(IR_op_stmt));
            IR_op_stmt_init(op_stmt, IR_OP_ADD, sr_var->new_variable, basic_iv_val, constant_val);
            sr_var->initialization_stmt = (IR_stmt*)op_stmt;
        }
    } else {
        // 复杂情况：需要先计算 coefficient * basic_iv
        IR_val coeff_val = {.is_const = true, .const_val = derived_iv->coefficient};
        IR_val basic_iv_val = {.is_const = false, .var = derived_iv->basic_iv->variable};
        IR_op_stmt *mul_stmt = (IR_op_stmt*)malloc(sizeof(IR_op_stmt));
        
        if (derived_iv->constant == 0) {
            // 直接在乘法语句中赋值给sr_var，避免中间变量
            IR_op_stmt_init(mul_stmt, IR_OP_MUL, sr_var->new_variable, coeff_val, basic_iv_val);
            sr_var->initialization_stmt = NULL; // 乘法语句已经添加，不需要额外的赋值
        } else {
            // 需要临时变量来处理常数项
            IR_var temp_var = ir_var_generator();
            IR_op_stmt_init(mul_stmt, IR_OP_MUL, temp_var, coeff_val, basic_iv_val);
            
            // sr_var = temp_var + constant
            IR_val temp_val = {.is_const = false, .var = temp_var};
            IR_val constant_val = {.is_const = true, .const_val = derived_iv->constant};
            IR_op_stmt *add_stmt = (IR_op_stmt*)malloc(sizeof(IR_op_stmt));
            IR_op_stmt_init(add_stmt, IR_OP_ADD, sr_var->new_variable, temp_val, constant_val);
            sr_var->initialization_stmt = (IR_stmt*)add_stmt;
        }
        
        // 将乘法语句添加到前序块
        VCALL(loop->preheader->stmts, push_back, (IR_stmt*)mul_stmt);
    }
    
    // 将初始化语句添加到前序块
    if (sr_var->initialization_stmt != NULL) {
        VCALL(loop->preheader->stmts, push_back, sr_var->initialization_stmt);
    }
    
    // printf("Added initialization for v%u in preheader\n", sr_var->new_variable);
}

/**
 * @brief 在循环内创建增量语句
 */
static void create_increment_in_loop(StrengthReductionVariable_ptr sr_var, Loop_ptr loop) {
    if (!sr_var || !loop) {
        printf("Error: NULL sr_var or loop in create_increment_in_loop\n");
        return;
    }
    
    // 获取对应的基本归纳变量
    DerivedInductionVariable_ptr derived_iv = sr_var->original_derived_iv;
    if (!derived_iv || !derived_iv->basic_iv) {
        printf("Error: NULL derived_iv or basic_iv in create_increment_in_loop\n");
        return;
    }
    
    BasicInductionVariable_ptr basic_iv = derived_iv->basic_iv;
    IR_block_ptr target_block = basic_iv->increment_block;
    IR_stmt *basic_increment_stmt = basic_iv->increment_stmt;
    
    if (!target_block || !basic_increment_stmt) {
        printf("Error: NULL target_block or basic_increment_stmt\n");
        return;
    }
    
    sr_var->increment_block = target_block;
    
    // 创建直接增量语句，不引入临时变量
    IR_val sr_var_val = {.is_const = false, .var = sr_var->new_variable};
    IR_val increment_val = {.is_const = true, .const_val = sr_var->increment_value};
    
    // 直接使用 sr_var = sr_var + increment
    IR_op_stmt *inc_stmt = (IR_op_stmt*)malloc(sizeof(IR_op_stmt));
    IR_op_stmt_init(inc_stmt, IR_OP_ADD, sr_var->new_variable, sr_var_val, increment_val);
    sr_var->increment_stmt = (IR_stmt*)inc_stmt;
    
    // 在基本归纳变量更新语句之后插入派生归纳变量的增量语句
    // 首先找到基本归纳变量更新语句在链表中的位置
    ListNode_IR_stmt_ptr *target_node = NULL;
    for (ListNode_IR_stmt_ptr *node = target_block->stmts.head; node != NULL; node = node->nxt) {
        if (node->val == basic_increment_stmt) {
            target_node = node;
            break;
        }
    }
    
    if (!target_node) {
        printf("Error: Could not find basic increment statement in target block\n");
        return;
    }
    
    // 创建新的链表节点
    ListNode_IR_stmt_ptr *new_node = (ListNode_IR_stmt_ptr*)malloc(sizeof(ListNode_IR_stmt_ptr));
    new_node->val = sr_var->increment_stmt;
    
    // 将新节点插入到基本归纳变量更新语句之后
    new_node->nxt = target_node->nxt;
    new_node->pre = target_node;
    
    if (target_node->nxt) {
        target_node->nxt->pre = new_node;
    } else {
        // target_node是最后一个节点，更新tail
        target_block->stmts.tail = new_node;
    }
    
    target_node->nxt = new_node;
    
    // printf("Added increment for v%u after basic IV update in block B%u\n", 
    //        sr_var->new_variable, target_block->label);
}



/**
 * @brief 对循环执行强度削减优化
 */
List_StrengthReductionVariable_ptr perform_strength_reduction(
    InductionVariableAnalyzer *analyzer,
    LoopInductionVariables_ptr loop_ivs) {
    
    // printf("=== Starting Strength Reduction for Loop ===\n");
    
    if (!loop_ivs || !loop_ivs->derived_ivs.head) {
        // printf("No derived induction variables found for strength reduction\n");
        List_StrengthReductionVariable_ptr empty_list;
        List_StrengthReductionVariable_ptr_init(&empty_list);
        return empty_list;
    }
    
    Loop_ptr loop = loop_ivs->loop;
    if (!loop || !loop->preheader) {
        // printf("Loop or preheader not available for strength reduction %p\n", loop);
        List_StrengthReductionVariable_ptr empty_list;
        List_StrengthReductionVariable_ptr_init(&empty_list);
        return empty_list;
    }
    
    // Create list to store strength reduction variables
    List_StrengthReductionVariable_ptr sr_vars;
    List_StrengthReductionVariable_ptr_init(&sr_vars);
    
    // 为每个派生归纳变量创建强度削减变量
    for_list(DerivedInductionVariable_ptr, div_node, loop_ivs->derived_ivs){
        DerivedInductionVariable_ptr derived_iv = div_node->val;
        
        // 只对系数不为1的派生归纳变量进行强度削减
        if (derived_iv->coefficient == 1) {
            // printf("Skipping strength reduction for v%u (coefficient = 1)\n",
                //    derived_iv->variable);
            continue;
        }
        
        // printf("Processing derived induction variable v%u = %d * v%u + %d\n",
        //        derived_iv->variable,
        //        derived_iv->coefficient,
        //        derived_iv->basic_iv->variable,
        //        derived_iv->constant);
        
        // 创建强度削减变量
        StrengthReductionVariable_ptr sr_var = create_strength_reduction_variable(analyzer, derived_iv, loop_ivs);
        if (!sr_var) continue;
        
        // 在前序块中创建初始化
        create_initialization_in_preheader(sr_var, derived_iv, loop);
        
        // 在循环体中创建增量更新
        create_increment_in_loop(sr_var, loop);
        
        // 替换原变量的使用
        replace_derived_variable_uses(derived_iv, sr_var, loop_ivs);
        
        // 将强度削减变量添加到列表中
        VCALL(sr_vars, push_back, sr_var);
    }
    
    // 删除无用的派生归纳变量定义语句
    if (sr_vars.head != NULL) {
        remove_useless_derived_iv_definitions(loop_ivs, sr_vars);
    }
    
    // printf("=== Strength Reduction Complete ===\n\n");
    return sr_vars;
}

/**
 * @brief 对单个函数的所有循环执行强度削减
 */
void perform_strength_reduction_for_function(IR_function_ptr function, LoopAnalyzer *loop_analyzer) {
    if (!function || !loop_analyzer) return;
    
    // printf("=== Starting Strength Reduction for Function: %s ===\n", function->func_name);
    
    // 执行归纳变量分析
    InductionVariableAnalyzer iv_analyzer;
    InductionVariableAnalyzer_init(&iv_analyzer, function, loop_analyzer);
    InductionVariableAnalyzer_analyze(&iv_analyzer);
    
    // 对每个循环执行强度削减
    for_list(Loop_ptr, loop_node, loop_analyzer->all_loops) {
        Loop_ptr loop = loop_node->val;
        
        // 获取循环的归纳变量信息
        LoopInductionVariables_ptr loop_ivs = InductionVariableAnalyzer_get_loop_ivs(&iv_analyzer, loop);
        if (!loop_ivs || !loop_ivs->derived_ivs.head) {
            // printf("No derived induction variables found in loop %p\n", loop);
            continue;
        }
        
        // printf("Performing strength reduction for loop %p\n", loop);
        
        // 执行强度削减
        List_StrengthReductionVariable_ptr sr_vars = perform_strength_reduction(&iv_analyzer, loop_ivs);
        
        // 可以选择保存结果或进一步处理
        if (sr_vars.head != NULL) {
            // 计算列表大小
            int count = 0;
            for_list(StrengthReductionVariable_ptr, sr_node, sr_vars) {
                count++;
            }
            // printf("Created %d strength reduction variables for this loop\n", count);
            // 暂时清理内存
            List_StrengthReductionVariable_ptr_teardown(&sr_vars);
        }
    }
    
    InductionVariableAnalyzer_teardown(&iv_analyzer);
    // printf("=== Strength Reduction Complete for Function: %s ===\n", function->func_name);
}
/**
 * @brief 打印强度削减变量信息
 */
void StrengthReductionVariable_print(StrengthReductionVariable_ptr sr_var, FILE *out) {

    if (!sr_var || !out) return;
    
    fprintf(out, "Strength Reduction Variable:\n");
    fprintf(out, "  New Variable: v%u\n", sr_var->new_variable);
    fprintf(out, "  Original Derived IV: v%u\n", sr_var->original_derived_iv->variable);
    fprintf(out, "  Increment Value: %d\n", sr_var->increment_value);
    fprintf(out, "  Initialization: %s\n", sr_var->initialization_stmt ? "Yes" : "No");
    fprintf(out, "  Increment: %s\n", sr_var->increment_stmt ? "Yes" : "No");
}

/**
 * @brief 在语句中替换变量使用
 */
static void replace_variable_in_stmt(IR_stmt *stmt, IR_var old_var, IR_var new_var) {
    if (!stmt) return;
    
    switch (stmt->stmt_type) {
        case IR_OP_STMT: {
            IR_op_stmt *op_stmt = (IR_op_stmt*)stmt;
            replace_variable_in_IR_val(&op_stmt->rs1, old_var, new_var);
            replace_variable_in_IR_val(&op_stmt->rs2, old_var, new_var);
            break;
        }
        case IR_ASSIGN_STMT: {
            IR_assign_stmt *assign_stmt = (IR_assign_stmt*)stmt;
            replace_variable_in_IR_val(&assign_stmt->rs, old_var, new_var);
            break;
        }
        case IR_LOAD_STMT: {
            IR_load_stmt *load_stmt = (IR_load_stmt*)stmt;
            replace_variable_in_IR_val(&load_stmt->rs_addr, old_var, new_var);
            break;
        }
        case IR_STORE_STMT: {
            IR_store_stmt *store_stmt = (IR_store_stmt*)stmt;
            replace_variable_in_IR_val(&store_stmt->rd_addr, old_var, new_var);
            replace_variable_in_IR_val(&store_stmt->rs, old_var, new_var);
            break;
        }
        case IR_IF_STMT: {
            IR_if_stmt *if_stmt = (IR_if_stmt*)stmt;
            replace_variable_in_IR_val(&if_stmt->rs1, old_var, new_var);
            replace_variable_in_IR_val(&if_stmt->rs2, old_var, new_var);
            break;
        }
        case IR_RETURN_STMT: {
            IR_return_stmt *return_stmt = (IR_return_stmt*)stmt;
            replace_variable_in_IR_val(&return_stmt->rs, old_var, new_var);
            break;
        }
        case IR_CALL_STMT: {
            IR_call_stmt *call_stmt = (IR_call_stmt*)stmt;
            for (unsigned i = 0; i < call_stmt->argc; i++) {
                replace_variable_in_IR_val(&call_stmt->argv[i], old_var, new_var);
            }
            break;
        }
        case IR_WRITE_STMT: {
            IR_write_stmt *write_stmt = (IR_write_stmt*)stmt;
            replace_variable_in_IR_val(&write_stmt->rs, old_var, new_var);
            break;
        }
        default:
            break;
    }
}

/**
 * @brief 在IR_val中替换变量使用
 */
static void replace_variable_in_IR_val(IR_val *val, IR_var old_var, IR_var new_var) {
    if (!val || val->is_const) return;
    
    if (val->var == old_var) {
        val->var = new_var;
        // printf("    Replaced variable use: v%u -> v%u\n", old_var, new_var);
    }
}

/**
 * @brief 删除无用的派生归纳变量定义语句
 */
static void remove_useless_derived_iv_definitions(
    LoopInductionVariables_ptr loop_ivs,
    List_StrengthReductionVariable_ptr sr_vars) {
    
    if (!loop_ivs || !sr_vars.head) {
        printf("Warning: NULL loop_ivs or empty sr_vars in remove_useless_derived_iv_definitions\n");
        return;
    }
    
    // printf("  Removing useless derived induction variable definitions...\n");
    
    Loop_ptr loop = loop_ivs->loop;
    if (!loop) {
        printf("Error: NULL loop in remove_useless_derived_iv_definitions\n");
        return;
    }
    
    // 遍历所有已进行强度削减的派生归纳变量
    for_list(StrengthReductionVariable_ptr, sr_node, sr_vars) {
        StrengthReductionVariable_ptr sr_var = sr_node->val;
        if (!sr_var) {
            printf("Warning: NULL sr_var in remove_useless_derived_iv_definitions\n");
            continue;
        }
        
        DerivedInductionVariable_ptr derived_iv = sr_var->original_derived_iv;
        
        if (!derived_iv || !derived_iv->definition_stmt) {
            printf("Warning: NULL derived_iv or definition_stmt\n");
            continue;
        }
        
        // printf("    Removing definition of v%u (replaced by v%u)\n", 
            //    derived_iv->variable, sr_var->new_variable);
        
        // 找到包含该定义语句的基本块
        IR_block *def_block = NULL;
        for_set(IR_block_ptr, block_node, loop->blocks) {
            IR_block_ptr block = block_node->key;
            if (!block) {
                printf("Warning: NULL block in loop->blocks during removal\n");
                continue;
            }
            
            // 在该块中查找定义语句
            for_list(IR_stmt_ptr, stmt_node, block->stmts) {
                if (stmt_node->val == derived_iv->definition_stmt) {
                    def_block = block;
                    break;
                }
            }
            if (def_block) break;
        }
        
        if (!def_block) {
            // printf("    Warning: Could not find definition block for v%u\n", derived_iv->variable);
            continue;
        }
        
        // 从基本块的语句列表中删除该语句
        ListNode_IR_stmt_ptr *prev = NULL;
        ListNode_IR_stmt_ptr *current = def_block->stmts.head;
        
        while (current) {
            if (current->val == derived_iv->definition_stmt) {
                // 找到了要删除的语句
                if (prev) {
                    prev->nxt = current->nxt;
                } else {
                    def_block->stmts.head = current->nxt;
                }
                
                if (current->nxt) {
                    current->nxt->pre = prev;
                } else {
                    def_block->stmts.tail = prev;
                }
                
                // 释放语句节点的内存
                free(current);
                // printf("    Successfully removed definition statement for v%u\n", derived_iv->variable);
                break;
            }
            prev = current;
            current = current->nxt;
        }
    }
    
    // printf("  Completed removal of useless definitions\n");
}