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
    }
}
