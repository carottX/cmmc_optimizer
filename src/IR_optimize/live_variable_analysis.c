//
// Created by hby on 22-12-2.
//

#include <live_variable_analysis.h> // 引入活跃变量分析的头文件

//// ============================ 数据流分析 (Dataflow Analysis) 实现 ============================

/**
 * @brief 活跃变量分析的析构函数。
 * 释放存储在mapInFact和mapOutFact中的所有变量集合。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 */
static void LiveVariableAnalysis_teardown(LiveVariableAnalysis *t) {
    // 遍历并删除所有InFact (变量集合)
    for_map(IR_block_ptr, Set_ptr_IR_var, i, t->mapInFact)
        DELETE(i->val); // DELETE是自定义的释放宏，会调用teardown并free
    // 遍历并删除所有OutFact (变量集合)
    for_map(IR_block_ptr, Set_ptr_IR_var, i, t->mapOutFact)
        DELETE(i->val);
    // 释放存储InFact和OutFact的映射本身
    Map_IR_block_ptr_Set_ptr_IR_var_teardown(&t->mapInFact);
    Map_IR_block_ptr_Set_ptr_IR_var_teardown(&t->mapOutFact);
}

/**
 * @brief 判断活跃变量分析是否为前向分析。
 * 活跃变量分析是典型的后向数据流分析。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @return 总是返回 false。
 */
static bool
LiveVariableAnalysis_isForward (LiveVariableAnalysis *t) {
    // TODO: return isForward?; 应返回 false
    TODO();
}

/**
 * @brief 创建并返回边界条件下的数据流事实。
 * 对于后向的活跃变量分析，边界是出口基本块(exit block)的IN集合，初始为空集。
 * 因为程序结束后，没有变量是活跃的。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param func 当前正在分析的函数。
 * @return 指向新创建的空变量集合 (Set_IR_var) 的指针。
 */
static Set_IR_var*
LiveVariableAnalysis_newBoundaryFact (LiveVariableAnalysis *t, IR_function *func) {
    return NEW(Set_IR_var); // 返回一个空的变量集合
}

/**
 * @brief 创建并返回数据流事实的初始值。
 * 对于活跃变量分析（May Analysis），初始值是空集（格中的Bottom元素），
 * 因为通过meet操作（并集）会不断加入新的活跃变量。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @return 指向新创建的空变量集合 (Set_IR_var) 的指针。
 */
static Set_IR_var*
LiveVariableAnalysis_newInitialFact (LiveVariableAnalysis *t) {
    return NEW(Set_IR_var); // May Analysis => Bottom, 即空集
}

/**
 * @brief 设置指定基本块的输入数据流事实 (IN fact)。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param blk 指向目标 IR_block 的指针。
 * @param fact 指向要设置的变量集合的指针。
 */
static void
LiveVariableAnalysis_setInFact (LiveVariableAnalysis *t,
                                IR_block *blk,
                                Set_IR_var *fact) {
    VCALL(t->mapInFact, set, blk, fact);
}

/**
 * @brief 设置指定基本块的输出数据流事实 (OUT fact)。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param blk 指向目标 IR_block 的指针。
 * @param fact 指向要设置的变量集合的指针。
 */
static void
LiveVariableAnalysis_setOutFact (LiveVariableAnalysis *t,
                                 IR_block *blk,
                                 Set_IR_var *fact) {
    VCALL(t->mapOutFact, set, blk, fact);
}

/**
 * @brief 获取指定基本块的输入数据流事实 (IN fact)。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param blk 指向目标 IR_block 的指针。
 * @return 指向获取到的变量集合的指针。
 */
static Set_IR_var*
LiveVariableAnalysis_getInFact (LiveVariableAnalysis *t, IR_block *blk) {
    return VCALL(t->mapInFact, get, blk);
}

/**
 * @brief 获取指定基本块的输出数据流事实 (OUT fact)。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param blk 指向目标 IR_block 的指针。
 * @return 指向获取到的变量集合的指针。
 */
static Set_IR_var*
LiveVariableAnalysis_getOutFact (LiveVariableAnalysis *t, IR_block *blk) {
    return VCALL(t->mapOutFact, get, blk);
}

/**
 * @brief 执行 meet 操作，将一个变量集合 (fact) 合并到另一个变量集合 (target)。
 * 对于活跃变量分析（May Analysis），meet 操作是并集。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param fact 指向源变量集合的指针。
 * @param target 指向目标变量集合的指针 (此集合会被修改)。
 * @return 如果 target 集合因 meet 操作发生改变则返回 true，否则返回 false。
 */
static bool
LiveVariableAnalysis_meetInto (LiveVariableAnalysis *t,
                               Set_IR_var *fact,
                               Set_IR_var *target) {
    /* TODO:
     * meet: union/intersect?
     * 对于活跃变量分析，meet操作是求并集 (union)。
     * return VCALL(*target, union_with, fact);
     */
    TODO();
}

/**
 * @brief （辅助函数）处理单条语句对活跃变量集合的影响。
 * 这是后向分析的核心转换逻辑： new_fact = use[stmt] U (old_fact - def[stmt])
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param stmt 指向当前处理的 IR_stmt 的指针。
 * @param fact 指向当前活跃变量集合的指针 (此集合会被修改，代表从语句后到语句前的状态变化)。
 */
void LiveVariableAnalysis_transferStmt (LiveVariableAnalysis *t,
                                        IR_stmt *stmt,
                                        Set_IR_var *fact) {
    IR_var def = VCALL(*stmt, get_def); // 获取语句定义的变量 (def)
    IR_use use = VCALL(*stmt, get_use_vec); // 获取语句使用的变量列表 (use)
    /* TODO:
     * kill/gen?
     * 对于后向分析，应该先处理def（kill），再处理use（gen）。
     *
     * 1. kill: 如果语句定义了一个变量 def，那么这个变量在语句执行前是否活跃，
     * 取决于这条语句之后它是否被使用，而不是更早之前的状态。所以先从fact中移除def。
     * if(def != IR_VAR_NONE) {
     * VCALL(*fact, delete, def);
     * }
     *
     * 2. gen: 如果语句使用了变量 use，那么这些变量在语句执行前一定是活跃的。
     * 所以将所有use变量加入fact。
     * for(unsigned i = 0; i < use.use_cnt; i ++) {
     * IR_val use_val = use.use_vec[i];
     * if(!use_val.is_const) { // 只处理变量，忽略常量
     * VCALL(*fact, insert, use_val.var);
     * }
     * }
     */
    TODO();
}

/**
 * @brief （核心传递函数）根据基本块的 OUT 集合计算其 IN 集合。
 * 它首先将out_fact复制到临时的new_in_fact，然后按相反的顺序模拟块内每条语句的执行，
 * 更新new_in_fact，最后将new_in_fact与原始的in_fact进行meet。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param block 指向当前处理的基本块的指针。
 * @param in_fact 指向该块当前 IN 集合的指针 (会被更新)。
 * @param out_fact 指向该块 OUT 集合的指针 (作为输入)。
 * @return 如果 in_fact 发生改变则返回 true，否则返回 false。
 */
bool LiveVariableAnalysis_transferBlock (LiveVariableAnalysis *t,
                                         IR_block *block,
                                         Set_IR_var *in_fact,
                                         Set_IR_var *out_fact) {
    // 创建一个新的变量集合作为临时的in_fact，并用out_fact初始化它
    Set_IR_var *new_in_fact = LiveVariableAnalysis_newInitialFact(t);
    LiveVariableAnalysis_meetInto(t, out_fact, new_in_fact); // new_in_fact = out_fact

    // 因为是后向分析，所以需要从后向前遍历基本块中的所有语句
    rfor_list(IR_stmt_ptr, i, block->stmts) { // rfor_list 是反向遍历链表的宏
        IR_stmt *stmt = i->val;
        LiveVariableAnalysis_transferStmt(t, stmt, new_in_fact); // 应用语句的传递函数
    }

    // 将计算得到的new_in_fact与原有的in_fact进行meet
    bool updated = LiveVariableAnalysis_meetInto(t, new_in_fact, in_fact);
    DELETE(new_in_fact); // 释放临时的new_in_fact
    return updated; // 返回in_fact是否被更新
}

/**
 * @brief 打印活跃变量分析的结果，用于调试。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param func 当前分析的函数。
 */
void LiveVariableAnalysis_print_result(LiveVariableAnalysis *t, IR_function *func) {
    printf("Function %s: Live Variable Analysis Result\n", func->func_name);
    for_list(IR_block_ptr, i, func->blocks) { // 遍历所有基本块
        IR_block *blk = i->val;
        printf("=================\n");
        printf("{Block%s %p}\n", blk == func->entry ? "(Entry)" :
                                      blk == func->exit ? "(Exit)" : "",
                                      blk);
        IR_block_print(blk, stdout); // 打印基本块内容
        Set_IR_var *in_fact = VCALL(*t, getInFact, blk),
                   *out_fact = VCALL(*t, getOutFact, blk);
        printf("[In]:  ");
        for_set(IR_var, var, *in_fact) // 遍历并打印IN集合中的活跃变量
            printf("v%u ", var->key);
        printf("\n");
        printf("[Out]: ");
        for_set(IR_var, var, *out_fact) // 遍历并打印OUT集合中的活跃变量
            printf("v%u ", var->key);
        printf("\n");
        printf("=================\n");
    }
}

/**
 * @brief 初始化活跃变量分析实例。
 * 设置虚函数表，并初始化存储IN/OUT事实的映射表。
 * @param t 指向要初始化的 LiveVariableAnalysis 实例的指针。
 */
void LiveVariableAnalysis_init(LiveVariableAnalysis *t) {
    const static struct LiveVariableAnalysis_virtualTable vTable = {
            .teardown        = LiveVariableAnalysis_teardown,
            .isForward       = LiveVariableAnalysis_isForward,
            .newBoundaryFact = LiveVariableAnalysis_newBoundaryFact,
            .newInitialFact  = LiveVariableAnalysis_newInitialFact,
            .setInFact       = LiveVariableAnalysis_setInFact,
            .setOutFact      = LiveVariableAnalysis_setOutFact,
            .getInFact       = LiveVariableAnalysis_getInFact,
            .getOutFact      = LiveVariableAnalysis_getOutFact,
            .meetInto        = LiveVariableAnalysis_meetInto,
            .transferBlock   = LiveVariableAnalysis_transferBlock,
            .printResult     = LiveVariableAnalysis_print_result
    };
    t->vTable = &vTable; // 设置虚函数表
    // 初始化存储IN和OUT fact的映射
    Map_IR_block_ptr_Set_ptr_IR_var_init(&t->mapInFact);
    Map_IR_block_ptr_Set_ptr_IR_var_init(&t->mapOutFact);
}

//// ============================ 优化 (Optimize) ============================

/**
 * @brief （辅助函数）对单个基本块执行死代码消除。
 * 遍历块内语句，如果一条语句定义了一个变量，而这个变量在该语句执行后立即变为不活跃，
 * 那么这条定义就是死代码。
 * @param t 指向 LiveVariableAnalysis 实例的指针 (应已包含分析结果)。
 * @param blk 指向要优化的基本块的指针。
 * @return 如果成功移除了任何死代码则返回 true，否则返回 false。
 */
static bool block_remove_dead_def (LiveVariableAnalysis *t, IR_block *blk) {
    bool updated = false;
    Set_IR_var *blk_out_fact = VCALL(*t, getOutFact, blk); // 获取块的OUT fact
    // 创建一个临时的fact，模拟语句在块内反向执行时活跃变量集合的演变
    Set_IR_var *current_live_fact = LiveVariableAnalysis_newInitialFact(t);
    LiveVariableAnalysis_meetInto(t, blk_out_fact, current_live_fact); // 初始化为块的OUT fact

    // 从后向前遍历块内所有语句
    rfor_list(IR_stmt_ptr, i, blk->stmts) {
        IR_stmt *stmt = i->val;
        // 只考虑有纯粹赋值作用且无副作用的语句
        if(stmt->stmt_type == IR_OP_STMT || stmt->stmt_type == IR_ASSIGN_STMT) {
            IR_var def = VCALL(*stmt, get_def); // 获取定义的变量
            if(def == IR_VAR_NONE) continue; // 如果没有定义变量，则跳过
            /* TODO:
             * def具有什么性质可以被标记为死代码?
             * 如果一个变量 def 在它被定义后，立即变得不活跃，那么这个定义就是死代码。
             * 也就是说，在执行这条语句之前，current_live_fact 中不包含 def。
             *
             * if(!VCALL(*current_live_fact, exist, def)) { // 检查def是否在当前活跃集合中
             * stmt->dead = true; // 标记为死代码
             * updated = true;
             * }
             */
            TODO();
        }
        // 在检查完当前语句后，更新活跃变量集合以反映到上一条语句的状态
        LiveVariableAnalysis_transferStmt(t, stmt, current_live_fact);
    }
    DELETE(current_live_fact); // 释放临时的fact
    remove_dead_stmt(blk); // 统一删除块内所有标记为 dead 的语句
    return updated;
}

/**
 * @brief 对整个函数执行死代码消除优化。
 * 遍历函数中的所有基本块，并对每个块调用 block_remove_dead_def。
 * 此函数需要在活跃变量分析求解完毕后调用。
 * @param t 指向 LiveVariableAnalysis 实例的指针。
 * @param func 指向要优化的函数。
 * @return 如果在任何块中移除了死代码，则返回 true。
 */
bool LiveVariableAnalysis_remove_dead_def (LiveVariableAnalysis *t, IR_function *func) {
    bool updated = false;
    for_list(IR_block_ptr, j, func->blocks) { // 遍历所有基本块
        IR_block *blk = j->val;
        updated |= block_remove_dead_def(t, blk); // 对每个块执行死代码消除
    }
    return updated;
}
