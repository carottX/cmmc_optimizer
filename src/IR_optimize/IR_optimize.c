//
// Created by hby on 22-12-2.
//

#include <IR_optimize.h>
#include <IR.h>
#include <live_variable_analysis.h>
#include <constant_propagation.h>
#include <available_expressions_analysis.h>
#include <copy_propagation.h>
#include <dominance_analysis.h>
#include <loop_analysis.h>
#include <induction_variable_analysis.h>
#include <container/treap.h>

#include <licm.h>
#include <stdio.h>

void remove_dead_block(IR_function *func) {
    // remove
    for(ListNode_IR_block_ptr *i = func->blocks.head; i;) {
        IR_block *blk = i->val;
        if(blk->dead) { // remove dead block
            RDELETE(IR_block, blk); // IR_block_teardown(blk); free(blk);
            i = VCALL(func->blocks, delete, i);
        } else i = i->nxt;
    }
}

void remove_dead_stmt(IR_block *blk) {
    for(ListNode_IR_stmt_ptr *j = blk->stmts.head; j;) {
        IR_stmt *stmt = j->val;
        if(stmt->dead) { // remove dead stmt
            RDELETE(IR_stmt, stmt); // IR_stmt_teardown(stmt); free(stmt);
            j = VCALL(blk->stmts, delete, j);
            continue;
        } else j = j->nxt;
    }
}


void _IR_block_print(IR_block *block, FILE *out) {
    if(block->label != IR_LABEL_NONE)
        fprintf(out, "LABEL L%u :\n", block->label);
    for_list(IR_stmt_ptr, i, block->stmts)
        VCALL(*i->val, print, out);
}
bool is_single_use_in_block(IR_function *func, IR_var var, ListNode_IR_stmt_ptr *start_from) {
    #ifdef DEBUG
    printf("CHECKING for v%d\n", var);
    #endif
    int use_count = 0;
    for_list(IR_block_ptr, blk_ptr, func->blocks) {
        IR_block_ptr blk = blk_ptr->val;
        for_list(IR_stmt_ptr, i, blk->stmts) {
            if (i == start_from) continue; // 跳过定义点
            
            IR_stmt_ptr stmt = i->val;
            
            // 检查所有使用该变量的地方
            if (stmt->stmt_type == IR_ASSIGN_STMT) {
                IR_assign_stmt *assign = (IR_assign_stmt*)stmt;
                if (!assign->rs.is_const && assign->rs.var == var) {
                    use_count++;
                }
            } else if (stmt->stmt_type == IR_OP_STMT) {
                IR_op_stmt *op = (IR_op_stmt*)stmt;
                if (!op->rs1.is_const && op->rs1.var == var) use_count++;
                if (!op->rs2.is_const && op->rs2.var == var) use_count++;
            } else if (stmt->stmt_type == IR_IF_STMT) {
                IR_if_stmt *if_stmt = (IR_if_stmt*)stmt;
                if (!if_stmt->rs1.is_const && if_stmt->rs1.var == var) use_count++;
                if (!if_stmt->rs2.is_const && if_stmt->rs2.var == var) use_count++;
            } else if (stmt->stmt_type == IR_WRITE_STMT) {
                IR_write_stmt *write_stmt = (IR_write_stmt*)stmt;
                if (!write_stmt->rs.is_const && write_stmt->rs.var == var) use_count++;
            } else if (stmt->stmt_type == IR_RETURN_STMT) {
                IR_return_stmt *ret_stmt = (IR_return_stmt*)stmt;
                if (!ret_stmt->rs.is_const && ret_stmt->rs.var == var) use_count++;
            }
            
            // 如果使用次数超过1，可以提前返回false，提高效率
            if (use_count > 1) {
                return false;
            }
        }
        if (use_count > 1) {
            return false;
        }
    }
    #ifdef DEBUG
    printf("v%d:%d\n", var, use_count);
    #endif
    return use_count == 1;
}

// 在 IR_optimize.c 中添加
void eliminate_single_use_temps(IR_function *func) {
    for_list(IR_block_ptr, i, func->blocks) {
        IR_block *blk = i->val;
        for_list(IR_stmt_ptr, j, blk->stmts) {
            IR_stmt_ptr stmt = j->val;
            if (stmt->stmt_type == IR_OP_STMT) {
                IR_op_stmt *op_stmt = (IR_op_stmt*)stmt;
                IR_var temp_var = op_stmt->rd;

                // printf("TEMP VAR:v%d\n", temp_var);
                
                // 查找下一个语句是否是对这个临时变量的简单赋值
                if (j->nxt && j->nxt->val->stmt_type == IR_ASSIGN_STMT) {
                    IR_assign_stmt *assign = (IR_assign_stmt*)j->nxt->val;
                    if (!assign->rs.is_const && assign->rs.var == temp_var) {
                        if (is_single_use_in_block(func, temp_var, j)) {
                            // 直接修改op_stmt的目标变量
                            op_stmt->rd = assign->rd;
                            assign->dead = true;
                        }
                    }
                }
            }
        }
        remove_dead_stmt(blk);
    }
}


void IR_optimize() {
    if (!ir_program_global) {
        printf("错误: 全局IR程序为空\n");
        return;
    }
    
    ConstantPropagation *constantPropagation;
    AvailableExpressionsAnalysis *availableExpressionsAnalysis;
    CopyPropagation *copyPropagation;
    LiveVariableAnalysis *liveVariableAnalysis;

    for_vec(IR_function_ptr, i, ir_program_global->functions) {
        IR_function *func = *i;

        DominanceAnalyzer dom_analyzer;
        DominanceAnalyzer_init(&dom_analyzer, func);
        DominanceAnalyzer_compute_dominators(&dom_analyzer);

        LoopAnalyzer loop_analyzer;
        LoopAnalyzer_init(&loop_analyzer, func, &dom_analyzer);
        LoopAnalyzer_detect_loops(&loop_analyzer);
        LoopAnalyzer_build_loop_hierarchy(&loop_analyzer);
        
        LoopAnalyzer_create_preheaders(&loop_analyzer);
        
        // LICMAnalyzer licm_analyzer;
        // LICMAnalyzer_init(&licm_analyzer, func, &loop_analyzer, &dom_analyzer);
        // bool licm_modified = LICMAnalyzer_optimize(&licm_analyzer);
        // LICMAnalyzer_teardown(&licm_analyzer);
            
        perform_strength_reduction_for_function(func, &loop_analyzer);
        
        LoopAnalyzer_teardown(&loop_analyzer);
        DominanceAnalyzer_teardown(&dom_analyzer);


        {
            //// Constant Propagation

            constantPropagation = NEW(ConstantPropagation);
            worklist_solver((DataflowAnalysis*)constantPropagation, func);
            // VCALL(*constantPropagation, printResult, func);
            ConstantPropagation_constant_folding(constantPropagation, func);
            DELETE(constantPropagation);

            //// Available Expressions Analysis

            availableExpressionsAnalysis = NEW(AvailableExpressionsAnalysis);
            AvailableExpressionsAnalysis_merge_common_expr(availableExpressionsAnalysis, func);
            worklist_solver((DataflowAnalysis*)availableExpressionsAnalysis, func); // 将子类强制转化为父类
            // VCALL(*availableExpressionsAnalysis, printResult, func);
            AvailableExpressionsAnalysis_remove_available_expr_def(availableExpressionsAnalysis, func);
            DELETE(availableExpressionsAnalysis);

            //// Copy Propagation

            copyPropagation = NEW(CopyPropagation);
            worklist_solver((DataflowAnalysis*)copyPropagation, func);
            // VCALL(*copyPropagation, printResult, func);
            CopyPropagation_replace_available_use_copy(copyPropagation, func);
            DELETE(copyPropagation);
        }
        

        //// Constant Propagation (2nd)

        constantPropagation = NEW(ConstantPropagation);
        worklist_solver((DataflowAnalysis*)constantPropagation, func);
        // VCALL(*constantPropagation, printResult, func);
        ConstantPropagation_constant_folding(constantPropagation, func);
        DELETE(constantPropagation);

        //// Live Variable Analysis

        while(true) {
            liveVariableAnalysis = NEW(LiveVariableAnalysis);
            worklist_solver((DataflowAnalysis*)liveVariableAnalysis, func); // 将子类强制转化为父类
            // VCALL(*liveVariableAnalysis, printResult, func);
            bool updated = LiveVariableAnalysis_remove_dead_def(liveVariableAnalysis, func);
            DELETE(liveVariableAnalysis);
            if(!updated) break;
        }
        eliminate_single_use_temps(func);
    }

}
