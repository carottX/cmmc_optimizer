//
// Created by hby on 22-12-2.
//

#include <dataflow_analysis.h> // 引入数据流分析的通用头文件

//// ============================ 前向分析 (Forward Analysis) ============================

/**
 * @brief 对前向数据流分析进行初始化。
 *
 * @param t 指向具体数据流分析实例的通用指针 (DataflowAnalysis *)。
 * @param func 当前正在分析的函数。
 */
static void initializeForward(DataflowAnalysis *t, IR_function *func) {
    // 遍历函数中的所有基本块
    for_list(IR_block_ptr, i, func->blocks) {
        // 为每个块的IN集合创建一个初始的数据流事实 (Fact)
        void *new_in_fact = VCALL(*t, newInitialFact);
        VCALL(*t, setInFact, i->val, new_in_fact);

        // 对入口基本块进行特殊处理
        if(i->val == func->entry) { // Entry为边界(Boundary)，需要特殊处理
            // 获取边界条件下的数据流事实，并设置为入口块的OUT集合
            void *entry_out_fact = VCALL(*t, newBoundaryFact, func);
            VCALL(*t, setOutFact, i->val, entry_out_fact);
        } else {
            // 为其他所有非入口块的OUT集合创建一个初始的数据流事实
            void *new_out_fact = VCALL(*t, newInitialFact);
            VCALL(*t, setOutFact, i->val, new_out_fact);
        }
    }
}

/**
 * @brief 使用迭代算法执行前向数据流分析。
 * 该算法会不断迭代，直到所有基本块的OUT集合不再发生变化为止。
 *
 * @param t 指向具体数据流分析实例的通用指针。
 * @param func 当前正在分析的函数。
 */
static void iterativeDoSolveForward(DataflowAnalysis *t, IR_function *func) {
    while(true) { // 持续迭代直到达到不动点
        bool updated = false; // 标记本轮迭代中是否有任何OUT集合发生变化
        // 遍历所有基本块
        for_list(IR_block_ptr, i, func->blocks) {
            IR_block *blk = i->val;
            // 获取当前块的 IN[blk] 与 OUT[blk] 集合
            Fact *in_fact = VCALL(*t, getInFact, blk), *out_fact = VCALL(*t, getOutFact, blk);

            // 1. Meet 操作: IN[blk] = meetAll(OUT[pred] for pred in AllPred[blk])
            // 遍历当前块的所有前驱(predecessor)基本块
            for_list(IR_block_ptr, j, *VCALL(func->blk_pred, get, blk)) {
                IR_block *pred = j->val;
                // 获取前驱块的OUT集合
                Fact *pred_out_fact = VCALL(*t, getOutFact, pred);
                // 将前驱块的OUT集合 meet 到当前块的IN集合中
                VCALL(*t, meetInto, pred_out_fact, in_fact);
            }

            // 2. Transfer 操作: OUT[blk] = transfer(IN[blk])
            // 调用传递函数，根据更新后的IN集合计算新的OUT集合
            // 如果OUT集合发生改变，则标记 updated 为 true
            if(VCALL(*t, transferBlock, blk, in_fact, out_fact))
                updated = true;
        }
        if(!updated) break; // 如果在一整轮迭代中没有任何OUT集合更新，则说明已达到不动点，退出循环
    }
}

/**
 * @brief 使用工作列表算法 (Worklist Algorithm) 执行前向数据流分析。
 * 这种算法只重新计算那些其前驱的OUT集合发生变化的块，通常比迭代法更高效。
 *
 * @param t 指向具体数据流分析实例的通用指针。
 * @param func 当前正在分析的函数。
 */
static void worklistDoSolveForward(DataflowAnalysis *t, IR_function *func) {
    // 创建一个工作列表，用于存放需要重新计算的基本块
    List_IR_block_ptr worklist;
    List_IR_block_ptr_init(&worklist);

    // 初始化时，将所有基本块都加入工作列表
    for_list(IR_block_ptr, i, func->blocks)
        VCALL(worklist, push_back, i->val);

    // 当工作列表不为空时，持续处理
    while(worklist.head != NULL) {
        // 从工作列表的头部取出一个基本块
        IR_block *blk = worklist.head->val;
        VCALL(worklist, pop_front);

        // 获取当前块的 IN[blk] 与 OUT[blk] 集合
        Fact *in_fact = VCALL(*t, getInFact, blk), *out_fact = VCALL(*t, getOutFact, blk);

        // 1. Meet 操作: IN[blk] = meetAll(OUT[pred] for pred in AllPred[blk])
        // 遍历当前块的所有前驱基本块
        for_list(IR_block_ptr, i, *VCALL(func->blk_pred, get, blk)) {
            IR_block *pred = i->val;
            // 获取前驱块的OUT集合
            Fact *pred_out_fact = VCALL(*t, getOutFact, pred);
            // 将前驱块的OUT集合 meet 到当前块的IN集合中
            VCALL(*t, meetInto, pred_out_fact, in_fact);
        }

        // 2. Transfer 操作: OUT[blk] = transfer(IN[blk])
        // 如果当前块的OUT集合在应用传递函数后发生了改变
        if(VCALL(*t, transferBlock, blk, in_fact, out_fact))
            // 则将其所有后继(successor)基本块加入工作列表，因为它们也需要被重新计算
            for_list(IR_block_ptr, i, *VCALL(func->blk_succ, get, blk))
                VCALL(worklist, push_back, i->val);
    }
    // 清理工作列表
    List_IR_block_ptr_teardown(&worklist);
}

//// ============================ 后向分析 (Backward Analysis) ============================

/* TODO:
 * 根据前向求解器的实现，完成后向求解器的实现
 * 后向分析与前向分析的逻辑非常相似，主要区别在于：
 * 1. 初始化时，边界条件在出口块(exit block)的IN集合，其他块的IN集合被初始化。
 * 2. 迭代时，数据流从后继(successor)流向前驱(predecessor)。
 * - OUT[B] = meetAll(IN[S] for S in succ(B))
 * - IN[B] = transfer(OUT[B])
 */

/**
 * @brief 对后向数据流分析进行初始化。
 * @param t 指向具体数据流分析实例的通用指针。
 * @param func 当前正在分析的函数。
 */
static void initializeBackward(DataflowAnalysis *t, IR_function *func) {
    // 此处需要实现后向分析的初始化逻辑
    TODO();
}

/**
 * @brief 使用迭代算法执行后向数据流分析。
 * @param t 指向具体数据流分析实例的通用指针。
 * @param func 当前正在分析的函数。
 */
static void iterativeDoSolveBackward(DataflowAnalysis *t, IR_function *func) {
    // 此处需要实现后向分析的迭代求解逻辑
    TODO();
}

/**
 * @brief 使用工作列表算法执行后向数据流分析。
 * @param t 指向具体数据流分析实例的通用指针。
 * @param func 当前正在分析的函数。
 */
static void worklistDoSolveBackward(DataflowAnalysis *t, IR_function *func) {
    // 此处需要实现后向分析的工作列表求解逻辑
    TODO();
}

//// ============================ 求解器入口 (Solver Entry) ============================

/**
 * @brief 工作列表求解器的总入口。
 * 根据分析类型（前向/后向）调用相应的初始化和求解函数。
 *
 * @param t 指向具体数据流分析实例的通用指针。
 * @param func 当前正在分析的函数。
 */
void worklist_solver(DataflowAnalysis *t, IR_function *func) {
    // 通过虚函数表调用isForward来判断分析类型
    if(VCALL(*t, isForward)) {
        initializeForward(t, func);
        worklistDoSolveForward(t, func);
    } else {
        initializeBackward(t, func);
        worklistDoSolveBackward(t, func);
    }
}

/**
 * @brief 迭代求解器的总入口。
 * 根据分析类型（前向/后向）调用相应的初始化和求解函数。
 *
 * @param t 指向具体数据流分析实例的通用指针。
 * @param func 当前正在分析的函数。
 */
void iterative_solver(DataflowAnalysis *t, IR_function *func) {
    // 通过虚函数表调用isForward来判断分析类型
    if(VCALL(*t, isForward)) {
        initializeForward(t, func);
        iterativeDoSolveForward(t, func);
    } else {
        initializeBackward(t, func);
        iterativeDoSolveBackward(t, func);
    }
}