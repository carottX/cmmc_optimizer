//
// Created by hby on 22-12-4.
//

#include <constant_propagation.h> // 引入常量传播分析的头文件

// 辅助函数，用于计算两个CPValue在数据流汇合点（meet point）的meet结果。
// CPValue代表变量在常量传播分析中的状态（UNDEF, CONST, NAC）。
static CPValue meetValue(CPValue v1, CPValue v2) {
    /* TODO
     * 计算不同数据流数据汇入后变量的CPValue的meet值
     * 要考虑 UNDEF/CONST/NAC 的不同情况
     *
     * Meet 操作规则:
     * UNDEF meet X = X
     * X meet UNDEF = X
     * CONST(c1) meet CONST(c1) = CONST(c1)
     * CONST(c1) meet CONST(c2) = NAC  (如果 c1 != c2)
     * NAC meet X = NAC
     * X meet NAC = NAC
     */
    TODO(); // 此处需要实现meet逻辑
}

// 辅助函数，用于计算二元运算结果的CPValue值。
// 例如，如果 v1 是 CONST(5)，v2 是 CONST(10)，操作是 IR_OP_ADD，则结果是 CONST(15)。
// 如果任一操作数是NAC，或结果无法确定为常量（如除以0），则结果是NAC或UNDEF。
static CPValue calculateValue(IR_OP_TYPE IR_op_type, CPValue v1, CPValue v2) {
    /* TODO
     * 计算二元运算结果的CPValue值
     * 要考虑 UNDEF/CONST/NAC 的不同情况
     * if(v1.kind == CONST && v2.kind == CONST) {
     * int v1_const = v1.const_val, v2_const = v2.const_val;
     * int res_const;
     * switch (IR_op_type) {
     * case IR_OP_ADD: res_const = v1_const + v2_const; break;
     * case IR_OP_SUB: res_const = v1_const - v2_const; break;
     * case IR_OP_MUL: res_const = v1_const * v2_const; break;
     * case IR_OP_DIV:
     * if(v2_const == 0) return get_UNDEF(); // 除以0，结果未定义
     * res_const = v1_const / v2_const; break;
     * default: assert(0);
     * }
     * return get_CONST(res_const);
     * } ... 其他情况 (例如，如果v1是NAC，则结果是NAC；如果v1是UNDEF，结果是UNDEF)
     */
    TODO(); // 此处需要实现二元运算的常量折叠逻辑
}

// UNDEF状态等价于在Map中不存在该Var的映射项。
// 此函数从数据流事实（Map_IR_var_CPValue）中获取指定IR变量的CPValue。
// 如果变量不在映射中，则认为其状态是UNDEF。
static CPValue
Fact_get_value_from_IR_var(Map_IR_var_CPValue *fact, IR_var var) {
    // 检查变量是否存在于fact映射中
    return VCALL(*fact, exist, var) ? VCALL(*fact, get, var) : get_UNDEF();
}

// 此函数从数据流事实（Map_IR_var_CPValue）中获取指定IR值（IR_val，可以是常量或变量）的CPValue。
// 如果IR_val本身是常量，则直接返回其CONST状态。
// 如果IR_val是变量，则调用Fact_get_value_from_IR_var获取其状态。
static CPValue
Fact_get_value_from_IR_val(Map_IR_var_CPValue *fact, IR_val val) {
    if(val.is_const) return get_CONST(val.const_val); // 如果是立即数，直接返回CONST
    else return Fact_get_value_from_IR_var(fact, val.var); // 如果是变量，从fact中查找
}

// 此函数更新数据流事实（Map_IR_var_CPValue）中指定IR变量的CPValue。
// 如果新的CPValue是UNDEF，则从映射中删除该变量（因为UNDEF等价于不存在）。
// 否则，在映射中设置（插入或更新）该变量的CPValue。
static void
Fact_update_value(Map_IR_var_CPValue *fact, IR_var var, CPValue val) {
    if (val.kind == UNDEF) VCALL(*fact, delete, var); // UNDEF状态则删除条目
    else VCALL(*fact, set, var, val); // 否则更新或插入条目
}

// 此函数尝试将给定的CPValue（val）与数据流事实（fact）中已有的对应变量的CPValue进行meet操作。
// 如果meet操作导致变量的CPValue发生变化，则更新fact并返回true。
// 否则，不修改fact并返回false。
static bool
Fact_meet_value(Map_IR_var_CPValue *fact, IR_var var, CPValue val) {
    CPValue old_val = Fact_get_value_from_IR_var(fact, var); // 获取旧值
    CPValue new_val = meetValue(old_val, val); // 计算meet后的新值
    // 比较旧值和新值是否相同
    if(old_val.kind == new_val.kind && old_val.const_val == new_val.const_val)
        return false; // 值未改变
    Fact_update_value(fact, var, new_val); // 更新为新值
    return true; // 值已改变
}


//// ============================ 数据流分析 (Dataflow Analysis) 实现 ============================

// 常量传播分析的析构函数。
// 释放存储在mapInFact和mapOutFact中的所有CPValue映射。
static void ConstantPropagation_teardown(ConstantPropagation *t) {
    // 遍历并删除所有InFact
    for_map(IR_block_ptr, Map_ptr_IR_var_CPValue, i, t->mapInFact)
        RDELETE(Map_IR_var_CPValue, i->val); // RDELETE是自定义的释放宏
    // 遍历并删除所有OutFact
    for_map(IR_block_ptr, Map_ptr_IR_var_CPValue, i, t->mapOutFact)
        RDELETE(Map_IR_var_CPValue, i->val);
    // 释放存储InFact和OutFact的映射本身
    Map_IR_block_ptr_Map_ptr_IR_var_CPValue_teardown(&t->mapInFact);
    Map_IR_block_ptr_Map_ptr_IR_var_CPValue_teardown(&t->mapOutFact);
}

// 判断常量传播是否为前向分析。
// 常量传播是典型的前向数据流分析。
static bool
ConstantPropagation_isForward (ConstantPropagation *t) {
    // TODO: return isForward?; 应返回 true
    TODO();
}

// 创建并返回边界条件下的数据流事实（通常是程序入口的OUT集合）。
// 对于常量传播，函数参数可以初始化为NAC（非常量）或UNDEF（未定义），
// 全局变量可能需要特殊处理（此处未明确处理全局变量）。
static Map_IR_var_CPValue*
ConstantPropagation_newBoundaryFact (ConstantPropagation *t, IR_function *func) {
    Map_IR_var_CPValue *fact = NEW(Map_IR_var_CPValue); // 创建新的CPValue映射
    /* TODO
     * 在Boundary(Entry的OutFact)中, 函数参数初始化为什么?
     * 例如，可以将所有函数参数初始化为NAC，因为它们的值在调用时才能确定。
     * for_vec(IR_var, param_ptr, func->params)
     * VCALL(*fact, insert, *param_ptr, get_NAC()); // 或 get_UNDEF()
     */
    TODO();
    return fact;
}

// 创建并返回数据流事实的初始值。
// 对于常量传播，通常所有变量的初始状态都是UNDEF（格中的Top元素），
// 因为meet操作（向下）会逐渐确定变量的值。
// 一个空的Map_IR_var_CPValue自然代表所有变量都是UNDEF。
static Map_IR_var_CPValue*
ConstantPropagation_newInitialFact (ConstantPropagation *t) {
    return NEW(Map_IR_var_CPValue); // 返回一个空的映射，代表所有变量为UNDEF
}

// 设置指定基本块的输入数据流事实 (IN fact)。
static void
ConstantPropagation_setInFact (ConstantPropagation *t,
                               IR_block *blk,
                               Map_IR_var_CPValue *fact) {
    VCALL(t->mapInFact, set, blk, fact); // 将fact存入mapInFact中，键为blk
}

// 设置指定基本块的输出数据流事实 (OUT fact)。
static void
ConstantPropagation_setOutFact (ConstantPropagation *t,
                            IR_block *blk,
                            Map_IR_var_CPValue *fact) {
    VCALL(t->mapOutFact, set, blk, fact); // 将fact存入mapOutFact中，键为blk
}

// 获取指定基本块的输入数据流事实 (IN fact)。
static Map_IR_var_CPValue*
ConstantPropagation_getInFact (ConstantPropagation *t, IR_block *blk) {
    return VCALL(t->mapInFact, get, blk); // 从mapInFact中获取blk对应的fact
}

// 获取指定基本块的输出数据流事实 (OUT fact)。
static Map_IR_var_CPValue*
ConstantPropagation_getOutFact (ConstantPropagation *t, IR_block *blk) {
    return VCALL(t->mapOutFact, get, blk); // 从mapOutFact中获取blk对应的fact
}

// 执行meet操作，将一个CPValue映射 (fact) 合并到另一个CPValue映射 (target)。
// 遍历源fact中的每个变量及其CPValue，并将其与target中对应变量的CPValue进行meet。
// 如果target因此发生改变，则返回true。
static bool
ConstantPropagation_meetInto (ConstantPropagation *t,
                              Map_IR_var_CPValue *fact, // 源fact
                              Map_IR_var_CPValue *target) { // 目标fact，会被修改
    bool updated = false;
    // 遍历源fact中的所有 (变量 -> CPValue) 对
    for_map(IR_var, CPValue, i, *fact)
        updated |= Fact_meet_value(target, i->key, i->val); // 对每个变量执行Fact_meet_value
    return updated; // 如果target有任何更新，则返回true
}

// （辅助函数）处理单条IR语句对CPValue映射（数据流事实）的影响。
// 根据语句的类型（赋值、操作等）更新定义变量的CPValue。
void ConstantPropagation_transferStmt (ConstantPropagation *t,
                                       IR_stmt *stmt,      // 当前处理的语句
                                       Map_IR_var_CPValue *fact) { // 当前语句执行前的数据流事实，会被修改以反映语句执行后的状态
    if(stmt->stmt_type == IR_ASSIGN_STMT) { // 处理赋值语句: rd := rs
        IR_assign_stmt *assign_stmt = (IR_assign_stmt*)stmt;
        IR_var def = assign_stmt->rd; // 定义的变量 rd
        CPValue use_val = Fact_get_value_from_IR_val(fact, assign_stmt->rs); // 获取源操作数 rs 的CPValue
        /* TODO: solve IR_ASSIGN_STMT
         * rd 的新状态就是 rs 的状态。
         * Fact_update_value(fact, def, use_val);
         */
        TODO();
    } else if(stmt->stmt_type == IR_OP_STMT) { // 处理操作语句: rd := rs1 op rs2
        IR_op_stmt *op_stmt = (IR_op_stmt*)stmt;
        IR_OP_TYPE IR_op_type = op_stmt->op; // 操作类型
        IR_var def = op_stmt->rd; // 定义的变量 rd
        CPValue rs1_val = Fact_get_value_from_IR_val(fact, op_stmt->rs1); // 获取 rs1 的CPValue
        CPValue rs2_val = Fact_get_value_from_IR_val(fact, op_stmt->rs2); // 获取 rs2 的CPValue
        /* TODO: solve IR_OP_STMT
         * 计算 rs1 op rs2 的结果状态。
         * CPValue result_val = calculateValue(IR_op_type, rs1_val, rs2_val);
         * Fact_update_value(fact, def, result_val);
         */
        TODO();
    } else { // 处理其他可能定义新变量的语句（如READ, CALL, LOAD）
        IR_var def = VCALL(*stmt, get_def); // 获取语句定义的变量
        if(def != IR_VAR_NONE) { // 如果语句确实定义了一个变量
            /* TODO: solve stmt with new_def
             * 对于READ语句，def的结果是NAC。
             * 对于CALL语句，如果函数返回值不能确定为常量，则def的结果是NAC。
             * 对于LOAD语句 (*p)，如果p指向的内存位置的值不能确定为常量，则def的结果是NAC。
             * 通常，这些操作会使变量变为NAC，除非有更复杂的分析（如过程间分析或指针分析）。
             * Fact_update_value(fact, def, get_NAC());
             */
            TODO();
        }
    }
}

// （核心传递函数）根据基本块的输入数据流事实 (in_fact) 计算其输出数据流事实 (out_fact)。
// 它首先将in_fact复制到临时的new_out_fact，然后按顺序模拟块内每条语句的执行，
// 更新new_out_fact，最后将new_out_fact与原始的out_fact进行meet。
// 如果原始的out_fact因此发生改变，则返回true。
bool ConstantPropagation_transferBlock (ConstantPropagation *t,
                                        IR_block *block,            // 当前处理的基本块
                                        Map_IR_var_CPValue *in_fact, // 输入到该块的fact
                                        Map_IR_var_CPValue *out_fact) { // 该块当前的输出fact，会被更新
    // 创建一个新的CPValue映射作为临时的out_fact，并用in_fact初始化它
    Map_IR_var_CPValue *new_out_fact = ConstantPropagation_newInitialFact(t);
    ConstantPropagation_meetInto(t, in_fact, new_out_fact); // new_out_fact = in_fact

    // 遍历基本块中的所有语句
    for_list(IR_stmt_ptr, i, block->stmts) {
        IR_stmt *stmt = i->val;
        ConstantPropagation_transferStmt(t, stmt, new_out_fact); // 应用语句的传递函数，修改new_out_fact
    }

    // 将计算得到的new_out_fact与原有的out_fact进行meet
    // 如果out_fact发生变化，updated会是true
    bool updated = ConstantPropagation_meetInto(t, new_out_fact, out_fact);
    RDELETE(Map_IR_var_CPValue, new_out_fact); // 释放临时的new_out_fact
    return updated; // 返回out_fact是否被更新
}

// 打印常量传播分析的结果，用于调试。
// 会打印每个基本块的IN和OUT集合中的变量及其CPValue状态。
void ConstantPropagation_print_result (ConstantPropagation *t, IR_function *func) {
    printf("Function %s: Constant Propagation Result\n", func->func_name);
    for_list(IR_block_ptr, i, func->blocks) { // 遍历函数中的所有基本块
        IR_block *blk = i->val;
        printf("=================\n");
        printf("{Block%s %p}\n", blk == func->entry ? "(Entry)" : // 标记入口/出口块
                                 blk == func->exit ? "(Exit)" : "",
               blk);
        IR_block_print(blk, stdout); // 打印基本块的内容
        Map_IR_var_CPValue *in_fact = VCALL(*t, getInFact, blk), // 获取IN fact
                *out_fact = VCALL(*t, getOutFact, blk); // 获取OUT fact
        printf("[In]:  ");
        for_map(IR_var, CPValue , j, *in_fact) { // 遍历IN fact中的变量
            printf("{v%u: ", j->key);
            if(j->val.kind == NAC)printf("NAC} ");
            else if(j->val.kind == CONST) printf("#%d} ", j->val.const_val);
            else printf("UNDEF} "); // 理论上UNDEF不会显式存储，但为了完整性
        }
        printf("\n");
        printf("[Out]: ");
        for_map(IR_var, CPValue , j, *out_fact) { // 遍历OUT fact中的变量
            printf("{v%u: ", j->key);
            if(j->val.kind == NAC)printf("NAC} ");
            else if(j->val.kind == CONST) printf("#%d} ", j->val.const_val);
            else printf("UNDEF} ");
        }
        printf("\n");
        printf("=================\n");
    }
}

// 初始化常量传播分析实例。
// 设置虚函数表，并初始化存储IN/OUT事实的映射表。
void ConstantPropagation_init(ConstantPropagation *t) {
    const static struct ConstantPropagation_virtualTable vTable = {
            .teardown        = ConstantPropagation_teardown,
            .isForward       = ConstantPropagation_isForward,
            .newBoundaryFact = ConstantPropagation_newBoundaryFact,
            .newInitialFact  = ConstantPropagation_newInitialFact,
            .setInFact       = ConstantPropagation_setInFact,
            .setOutFact      = ConstantPropagation_setOutFact,
            .getInFact       = ConstantPropagation_getInFact,
            .getOutFact      = ConstantPropagation_getOutFact,
            .meetInto        = ConstantPropagation_meetInto,
            .transferBlock   = ConstantPropagation_transferBlock,
            .printResult     = ConstantPropagation_print_result
    };
    t->vTable = &vTable; // 设置虚函数表
    // 初始化存储IN和OUT fact的映射
    Map_IR_block_ptr_Map_ptr_IR_var_CPValue_init(&t->mapInFact);
    Map_IR_block_ptr_Map_ptr_IR_var_CPValue_init(&t->mapOutFact);
}

//// ============================ 优化 (Optimize) ============================

// （辅助函数）对单个基本块执行常量折叠。
// 遍历块内语句，如果语句使用的变量在其执行点是常量，则将变量替换为常量值。
// 注意：此函数修改的是语句本身，而不是数据流事实。它依赖于已经计算好的数据流事实。
static void block_constant_folding (ConstantPropagation *t, IR_block *blk) {
    Map_IR_var_CPValue *blk_in_fact = VCALL(*t, getInFact, blk); // 获取块的IN fact
    // 创建一个临时的fact，模拟语句在块内执行时fact的演变
    Map_IR_var_CPValue *current_fact_for_folding = ConstantPropagation_newInitialFact(t);
    ConstantPropagation_meetInto(t, blk_in_fact, current_fact_for_folding); // 初始化为块的IN fact

    for_list(IR_stmt_ptr, i, blk->stmts) { // 遍历块内所有语句
        IR_stmt *stmt = i->val;
        IR_use use = VCALL(*stmt, get_use_vec); // 获取语句使用的变量列表

        // 对每个use变量进行常量替换
        for(int j = 0; j < use.use_cnt; j++) {
            if(!use.use_vec[j].is_const) { // 只处理变量类型的use
                IR_var use_var = use.use_vec[j].var;
                // 从当前演变的fact中获取use变量的CPValue
                CPValue use_CPVal = Fact_get_value_from_IR_var(current_fact_for_folding, use_var);
                if(use_CPVal.kind == CONST) { // 如果是常量
                    use.use_vec[j].is_const = true; // 将IR_val标记为常量
                    use.use_vec[j].const_val = use_CPVal.const_val; // 设置常量值
                }
            }
        }
        // 语句执行后，更新演变的fact，以供下一条语句使用
        ConstantPropagation_transferStmt(t, stmt, current_fact_for_folding);
    }
    RDELETE(Map_IR_var_CPValue, current_fact_for_folding); // 释放临时的fact
}

// 对整个函数执行常量折叠优化。
// 遍历函数中的所有基本块，并对每个块调用block_constant_folding。
// 此函数需要在常量传播数据流分析求解完毕后调用。
void ConstantPropagation_constant_folding (ConstantPropagation *t, IR_function *func) {
    for_list(IR_block_ptr, j, func->blocks) { // 遍历所有基本块
        IR_block *blk = j->val;
        block_constant_folding(t, blk); // 对每个块执行常量折叠
    }
}
