#include "IR.h"
#include <assert.h>
#include <stddef.h>

// 替换pred块中的某个后继（old_succ）为新的后继（new_succ）
// 假设pred的最后一条语句是跳转（GOTO/IF），修改其目标即可
void replace_successor(IR_block *pred, IR_block *old_succ, IR_block *new_succ) {
    if (!pred || !old_succ || !new_succ) return;
    if (!pred->stmts.tail) return;
    IR_stmt *last = pred->stmts.tail->val;
    switch (last->stmt_type) {
        case IR_GOTO_STMT: {
            IR_goto_stmt *goto_stmt = (IR_goto_stmt*)last;
            if (goto_stmt->blk == old_succ || goto_stmt->label == old_succ->label) {
                goto_stmt->blk = new_succ;
                goto_stmt->label = new_succ->label;
            }
            break;
        }
        case IR_IF_STMT: {
            IR_if_stmt *if_stmt = (IR_if_stmt*)last;
            if (if_stmt->true_label == old_succ->label) {
                if_stmt->true_label = new_succ->label;
                // 可选: if_stmt->true_blk = new_succ;
            }
            if (if_stmt->false_label == old_succ->label) {
                if_stmt->false_label = new_succ->label;
                // 可选: if_stmt->false_blk = new_succ;
            }
            break;
        }
        default:
            // 其他类型暂不处理
            break;
    }
}
