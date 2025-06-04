//
// Created by hby on 22-11-8.
//

#include <IR.h>
#include <stdio.h>

//// =============================== IR print ===============================

// 检查标签是否被函数中的任何语句引用
bool is_label_referenced(IR_function *func, IR_label label) {
    for_list(IR_block_ptr, block_node, func->blocks) {
        IR_block *block = block_node->val;
        for_list(IR_stmt_ptr, stmt_node, block->stmts) {
            IR_stmt_ptr stmt = stmt_node->val;
            
            if (stmt->stmt_type == IR_IF_STMT) {
                IR_if_stmt *if_stmt = (IR_if_stmt*)stmt;
                if (if_stmt->false_label == label || if_stmt->true_label == label) {
                    return true;
                }
            } else if (stmt->stmt_type == IR_GOTO_STMT) {
                IR_goto_stmt *goto_stmt = (IR_goto_stmt*)stmt;
                if (goto_stmt->label == label) {
                    return true;
                }
            }
        }
    }
    return false;
}

void IR_block_print_with_context(IR_block *block, IR_function *func, FILE *out) {
    // 只有当标签存在且被引用时才打印
    if (block->label != IR_LABEL_NONE && is_label_referenced(func, block->label)) {
        fprintf(out, "LABEL L%u :\n", block->label);
    }
    for_list(IR_stmt_ptr, i, block->stmts)
        VCALL(*i->val, print, out);
}

void IR_block_print(IR_block *block, FILE *out) {
    // 只有当标签存在且被引用时才打印
    if (block->label != IR_LABEL_NONE) {
        fprintf(out, "LABEL L%u :\n", block->label);
    }
    for_list(IR_stmt_ptr, i, block->stmts)
        VCALL(*i->val, print, out);
}

void IR_function_print(IR_function *func, FILE *out) {
    fprintf(out, "FUNCTION %s :\n", func->func_name);
    for_vec(IR_var, var, func->params)
        fprintf(out, "PARAM v%u\n", *var);
    for_map(IR_var, IR_Dec, it, func->map_dec) {
        fprintf(out, "DEC v%u %u\n", it->key, it->val.dec_size);
        fprintf(out, "v%u := &v%u\n", it->val.dec_addr, it->key);
    }
    for_list(IR_block_ptr, i, func->blocks)
        IR_block_print_with_context(i->val, func, out);
    fprintf(out, "\n");
}

void IR_program_print(IR_program *program, FILE *out) {
    for_vec(IR_function *, func_ptr_ptr, program->functions) {
        IR_function *func = *func_ptr_ptr;
        IR_function_print(func, out);
    }
}
