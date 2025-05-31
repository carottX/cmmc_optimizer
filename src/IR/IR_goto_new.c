#include "IR.h"
#include <stdlib.h>

IR_stmt *IR_goto_new(IR_block *target) {
    IR_goto_stmt *stmt = (IR_goto_stmt*)malloc(sizeof(IR_goto_stmt));
    if (!stmt) return NULL;
    IR_goto_stmt_init(stmt, target->label);
    stmt->blk = target;
    return (IR_stmt*)stmt;
}
