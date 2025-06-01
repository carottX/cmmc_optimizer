//
// Fixed dominance analysis implementation
//

#include <dominance_analysis.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

//// ================================== 支配信息结构体操作 ==================================

void DominanceInfo_init(DominanceInfo *info, IR_block_ptr block) {
    info->block = block;
    Set_IR_block_ptr_init(&info->dominators);
    info->immediate_dominator = NULL;
    Set_IR_block_ptr_init(&info->dominated_blocks);
    List_IR_block_ptr_init(&info->children_in_dom_tree);
}

void DominanceInfo_teardown(DominanceInfo *info) {
    Set_IR_block_ptr_teardown(&info->dominators);
    Set_IR_block_ptr_teardown(&info->dominated_blocks);
    List_IR_block_ptr_teardown(&info->children_in_dom_tree);
}

//// ================================== 支配节点分析器操作 ==================================

void DominanceAnalyzer_init(DominanceAnalyzer *analyzer, IR_function *func) {
    analyzer->function = func;
    Map_IR_block_ptr_DominanceInfo_init(&analyzer->dom_info);
    
    // 找到入口基本块
    if (func->entry) {
        analyzer->entry_block = func->entry;
        // printf("Entry block found from func->entry: %p\n", analyzer->entry_block);
    } else if (func->blocks.head) {
        analyzer->entry_block = func->blocks.head->val;
        // printf("Entry block found from first block: %p\n", analyzer->entry_block);
    } else {
        analyzer->entry_block = NULL;
        // printf("Warning: No blocks found in function\n");
        return;
    }
    
    // 为每个基本块初始化支配信息
    for_list(IR_block_ptr, i, func->blocks) {
        DominanceInfo info;
        // printf("%d\n",i->val->label);
        DominanceInfo_init(&info, i->val);
        VCALL(analyzer->dom_info, insert, i->val, info);
    }
}

void DominanceAnalyzer_teardown(DominanceAnalyzer *analyzer) {
    // 析构每个基本块的支配信息
    for_map(IR_block_ptr, DominanceInfo, i, analyzer->dom_info) {
        DominanceInfo_teardown(&i->val);
    }
    Map_IR_block_ptr_DominanceInfo_teardown(&analyzer->dom_info);
}

//// ================================== 支配节点计算 - 修复版本 ==================================

/**
 * @brief 简化的支配节点计算算法
 * 使用正确的数据流方程：Dom(n) = {n} ∪ (∩ Dom(p) for all predecessors p of n)
 */
void DominanceAnalyzer_compute_dominators(DominanceAnalyzer *analyzer) {
    IR_function *func = analyzer->function;
    bool changed = true;
    int iteration = 0;
    const int MAX_ITERATIONS = 100; // 降低最大迭代次数进行测试
    
    // printf("Starting simplified dominance analysis for function: %s\n", func->func_name);
    
    // 步骤1：初始化支配集合
    for_list(IR_block_ptr, i, func->blocks) {
        DominanceInfo info = VCALL(analyzer->dom_info, get, i->val);
        
        if (i->val == analyzer->entry_block) {
            // 入口节点只被自己支配
            VCALL(info.dominators, insert, i->val);
            // printf("Entry block %p: dominators = {self}\n", i->val);
        } else {
            // 其他节点初始化为被所有节点支配
            for_list(IR_block_ptr, j, func->blocks) {
                VCALL(info.dominators, insert, j->val);
            }
            // printf("Block %p: dominators = {all blocks}\n", i->val);
        }
        
        VCALL(analyzer->dom_info, set, i->val, info);
    }
    
    // 步骤2：迭代计算直到收敛
    while (changed && iteration < MAX_ITERATIONS) {
        changed = false;
        iteration++;
        // printf("\n=== Iteration %d ===\n", iteration);
        
        for_list(IR_block_ptr, i, func->blocks) {
            if (i->val == analyzer->entry_block) {
                continue; // 跳过入口节点
            }
            
            // printf("Processing block %p:\n", i->val);
            
            DominanceInfo current_info = VCALL(analyzer->dom_info, get, i->val);
            
            // 创建新的支配集合，开始时只包含节点本身
            Set_IR_block_ptr new_dominators;
            Set_IR_block_ptr_init(&new_dominators);
            VCALL(new_dominators, insert, i->val);
            // printf("INSERTED:%p", i->val);
            
            // 获取前驱节点
            List_IR_block_ptr *predecessors = VCALL(func->blk_pred, get, i->val);
            
            if (predecessors && predecessors->head) {
                // printf("  Has predecessors, computing intersection...\n");
                
                // 初始化交集为第一个前驱的支配集合
                bool first_pred = true;
                for_list(IR_block_ptr, pred, *predecessors) {
                    DominanceInfo pred_info = VCALL(analyzer->dom_info, get, pred->val);
                    
                    if (first_pred) {
                        // 第一个前驱：复制其支配集合
                        for_set(IR_block_ptr, dom_block, pred_info.dominators) {
                            VCALL(new_dominators, insert, dom_block->key);
                            // printf("INSERTED:%p\n", dom_block->key);
                        }
                        first_pred = false;
                        // printf("    Initialized with pred %p dominators\n", pred->val);
                    } else {
                        // 后续前驱：计算交集
                        Set_IR_block_ptr intersection;
                        Set_IR_block_ptr_init(&intersection);
                        
                        for_set(IR_block_ptr, dom_block, new_dominators) {
                            if (VCALL(pred_info.dominators, exist, dom_block->key)) {
                                VCALL(intersection, insert, dom_block->key);
                            }
                        }
                        
                        Set_IR_block_ptr_teardown(&new_dominators);
                        new_dominators = intersection;
                        // printf("    Intersected with pred %p dominators\n", pred->val);
                    }
                }
                
                // 确保包含节点本身
                VCALL(new_dominators, insert, i->val);
            } else {
                // printf("  No predecessors found\n");
            }
            
            // 检查是否有变化
            bool sets_equal = true;
            
            // 计算两个集合的大小（手动计数）
            int old_count = 0, new_count = 0;
            for_set(IR_block_ptr, block, current_info.dominators) { old_count++; }
            for_set(IR_block_ptr, block, new_dominators) { new_count++; }
            
            if (old_count != new_count) {
                sets_equal = false;
            } else {
                // 大小相等，检查内容
                for_set(IR_block_ptr, block, new_dominators) {
                    if(!VCALL(current_info.dominators, exist, block->key)){
                        sets_equal = false;
                        break;
                    }                
                }
            }
            
            if (!sets_equal) {
                changed = true;
                // printf("  Dominance set changed (old_size=%d, new_size=%d)\n", old_count, new_count);
                
                // 更新支配集合
                Set_IR_block_ptr_teardown(&current_info.dominators);
                current_info.dominators = new_dominators;
                VCALL(analyzer->dom_info, set, i->val, current_info);
            } else {
                // printf("  Dominance set unchanged (size=%d)\n", old_count);
                Set_IR_block_ptr_teardown(&new_dominators);
            }
        }
        // 打印每次迭代后的支配集合
        // printf("当前各基本块的支配集合：\n");
        // for_list(IR_block_ptr, blk, func->blocks) {
        //     DominanceInfo info = VCALL(analyzer->dom_info, get, blk->val);
        //     printf("  Block %p [L%u] in SET: { ", blk->val, blk->val->label);
        //     for_set(IR_block_ptr, dom, info.dominators) {
        //         printf("%p ", dom->key);
        //     }
        //     printf("}\n");
        // }
    }
    
    // printf("\nDominance analysis completed after %d iterations\n", iteration);
    // if (iteration >= MAX_ITERATIONS) {
    //     printf("WARNING: Reached maximum iterations - may not have converged\n");
    // }
}

// ================================== 支配关系查询接口 ==================================


/**
 * @brief 找到最接近的支配节点（直接支配节点）
 * 直接支配节点是支配集合中除了节点本身外，不被其他支配节点支配的节点
 */
static IR_block_ptr find_immediate_dominator(DominanceAnalyzer *analyzer, IR_block_ptr block) {
    if (block == analyzer->entry_block) {
        return NULL; // 入口节点没有直接支配节点
    }
    
    DominanceInfo info = VCALL(analyzer->dom_info, get, block);
    IR_block_ptr immediate_dom = NULL;
    
    // 在支配集合中找到直接支配节点
    for_set(IR_block_ptr, dom_block, info.dominators) {
        if (dom_block->key == block) {
            continue; // 跳过节点本身
        }
        
        // 检查这个支配节点是否是直接支配节点
        bool is_immediate = true;
        for_set(IR_block_ptr, other_dom, info.dominators) {
            if (other_dom->key == block || other_dom->key == dom_block->key) {
                continue;
            }
            
            // 如果存在另一个支配节点支配当前候选节点，那么当前候选节点不是直接支配节点
            DominanceInfo other_info = VCALL(analyzer->dom_info, get, other_dom->key);
            if (VCALL(other_info.dominators, exist, dom_block->key)) {
                is_immediate = false;
                break;
            }
        }
        
        if (is_immediate) {
            immediate_dom = dom_block->key;
            break;
        }
    }
    
    return immediate_dom;
}

void DominanceAnalyzer_build_dominator_tree(DominanceAnalyzer *analyzer) {
    IR_function *func = analyzer->function;
    
    // 为每个节点找到直接支配节点
    for_list(IR_block_ptr, i, func->blocks) {
        DominanceInfo info = VCALL(analyzer->dom_info, get, i->val);
        info.immediate_dominator = find_immediate_dominator(analyzer, i->val);
        
        // 如果有直接支配节点，建立父子关系
        if (info.immediate_dominator) {
            DominanceInfo parent_info = VCALL(analyzer->dom_info, get, info.immediate_dominator);
            VCALL(parent_info.children_in_dom_tree, push_back, i->val);
            VCALL(parent_info.dominated_blocks, insert, i->val);
            VCALL(analyzer->dom_info, set, info.immediate_dominator, parent_info);
        }
        
        VCALL(analyzer->dom_info, set, i->val, info);
    }
}

//// ================================== 查询接口 ==================================

bool DominanceAnalyzer_dominates(DominanceAnalyzer *analyzer, 
                                 IR_block_ptr dominator, 
                                 IR_block_ptr dominated) {
    DominanceInfo info = VCALL(analyzer->dom_info, get, dominated);
    return VCALL(info.dominators, exist, dominator);
}

IR_block_ptr DominanceAnalyzer_get_immediate_dominator(DominanceAnalyzer *analyzer, 
                                                       IR_block_ptr block) {
    DominanceInfo info = VCALL(analyzer->dom_info, get, block);
    return info.immediate_dominator;
}

Set_IR_block_ptr* DominanceAnalyzer_get_dominators(DominanceAnalyzer *analyzer, 
                                                   IR_block_ptr block) {
    // Note: This function returns a pointer to the internal set, which is problematic
    // with the current Map API. For now, we'll have to work around this limitation.
    // A better approach would be to copy the set or use a different API design.
    static DominanceInfo temp_info;
    temp_info = VCALL(analyzer->dom_info, get, block);
    return &temp_info.dominators;
}

Set_IR_block_ptr* DominanceAnalyzer_get_dominated_blocks(DominanceAnalyzer *analyzer, 
                                                        IR_block_ptr block) {
    // Same issue as above - returning pointer to internal data
    static DominanceInfo temp_info;
    temp_info = VCALL(analyzer->dom_info, get, block);
    return &temp_info.dominated_blocks;
}

//// ================================== 结果输出 ==================================

void DominanceAnalyzer_print_result(DominanceAnalyzer *analyzer, FILE *out) {
    fprintf(out, "========== 支配节点分析结果 ==========\n");
    fprintf(out, "函数: %s\n\n", analyzer->function->func_name);
    
    for_list(IR_block_ptr, i, analyzer->function->blocks) {
        DominanceInfo info = VCALL(analyzer->dom_info, get, i->val);
        
        fprintf(out, "基本块 %d", i->val->label);
        if (i->val == analyzer->entry_block) {
            fprintf(out, " (入口)");
        }
        if (i->val == analyzer->function->exit) {
            fprintf(out, " (出口)");
        }
        if (i->val->label != IR_LABEL_NONE) {
            fprintf(out, " [L%u]", i->val->label);
        }
        fprintf(out, ":\n");
        
        // 打印支配集合（只显示label）
        fprintf(out, "  支配节点: { ");
        for_set(IR_block_ptr, dom, info.dominators) {
            fprintf(out, "L%u ", dom->key->label);
        }
        fprintf(out, "}\n");
        
        // 打印直接支配节点
        if (info.immediate_dominator) {
            fprintf(out, "  直接支配节点: %p", info.immediate_dominator);
            if (info.immediate_dominator->label != IR_LABEL_NONE) {
                fprintf(out, "[L%u]", info.immediate_dominator->label);
            }
            fprintf(out, "\n");
        } else {
            fprintf(out, "  直接支配节点: 无 (入口节点)\n");
        }
        
        // 打印被支配的节点
        bool has_dominated = false;
        for_set(IR_block_ptr, dominated, info.dominated_blocks) {
            if (!has_dominated) {
                fprintf(out, "  支配的节点: { ");
                has_dominated = true;
            }
            fprintf(out, "%p", dominated->key);
            if (dominated->key->label != IR_LABEL_NONE) {
                fprintf(out, "[L%u]", dominated->key->label);
            }
            fprintf(out, " ");
        }
        if (has_dominated) {
            fprintf(out, "}\n");
        }
        
        fprintf(out, "\n");
    }
}

/**
 * @brief 递归打印支配树
 */
static void print_dominator_tree_recursive(DominanceAnalyzer *analyzer, 
                                          IR_block_ptr node, 
                                          int depth, 
                                          FILE *out) {
    // 打印缩进
    for (int i = 0; i < depth; i++) {
        fprintf(out, "  ");
    }
    
    // 打印当前节点
    fprintf(out, "- %p", node);
    if (node->label != IR_LABEL_NONE) {
        fprintf(out, "[L%u]", node->label);
    }
    if (node == analyzer->entry_block) {
        fprintf(out, " (入口)");
    }
    if (node == analyzer->function->exit) {
        fprintf(out, " (出口)");
    }
    fprintf(out, "\n");
    
    // 递归打印子节点
    DominanceInfo info = VCALL(analyzer->dom_info, get, node);
    for_list(IR_block_ptr, child, info.children_in_dom_tree) {
        print_dominator_tree_recursive(analyzer, child->val, depth + 1, out);
    }
}

void DominanceAnalyzer_print_dominator_tree(DominanceAnalyzer *analyzer, FILE *out) {
    fprintf(out, "========== 支配树结构 ==========\n");
    fprintf(out, "函数: %s\n\n", analyzer->function->func_name);
    
    if (analyzer->entry_block) {
        print_dominator_tree_recursive(analyzer, analyzer->entry_block, 0, out);
    } else {
        fprintf(out, "错误: 没有找到入口节点\n");
    }
    fprintf(out, "\n");
}

//// ================================== 测试和使用示例 ==================================

/**
 * @brief 对指定函数执行支配节点分析并输出结果
 * @param func 要分析的IR函数
 */
void perform_dominance_analysis(IR_function *func) {
    if (!func) {
        printf("错误: 传入的函数指针为空\n");
        return;
    }
    
    printf("函数名: %s\n", func->func_name);
    printf("基本块数量: (checking...)\n");
    printf("入口块: %p\n", (void*)func->entry);
    
    // Count blocks manually since List doesn't have length
    int block_count = 0;
    for_list(IR_block_ptr, i, func->blocks) {
        block_count++;
    }
    printf("实际基本块数量: %d\n", block_count);
    
    if (block_count == 0) {
        printf("警告: 函数没有基本块\n");
        return;
    }
    
    // 创建支配节点分析器
    DominanceAnalyzer analyzer;
    DominanceAnalyzer_init(&analyzer, func);
    
    // 执行支配节点计算
    DominanceAnalyzer_compute_dominators(&analyzer);
    
    // 构建支配树
    DominanceAnalyzer_build_dominator_tree(&analyzer);
    
    // 输出分析结果
    DominanceAnalyzer_print_result(&analyzer, stdout);
    
    // 输出支配树结构
    DominanceAnalyzer_print_dominator_tree(&analyzer, stdout);
    
    // 示例查询操作
    printf("========== 支配关系查询示例 ==========\n");
    
    if (func->blocks.head && func->blocks.head->nxt) {
        IR_block_ptr first_block = func->blocks.head->val;
        IR_block_ptr second_block = func->blocks.head->nxt->val;
        
        printf("查询: 第一个基本块是否支配第二个基本块?\n");
        printf("结果: %s\n", 
               DominanceAnalyzer_dominates(&analyzer, first_block, second_block) ? 
               "是" : "否");
        
        IR_block_ptr idom = DominanceAnalyzer_get_immediate_dominator(&analyzer, second_block);
        if (idom) {
            printf("第二个基本块的直接支配节点: %p", idom);
            if (idom->label != IR_LABEL_NONE) {
                printf("[L%u]", idom->label);
            }
            printf("\n");
        } else {
            printf("第二个基本块没有直接支配节点（可能是入口节点）\n");
        }
    }
    
    printf("\n");
    
    // 清理资源
    DominanceAnalyzer_teardown(&analyzer);
    printf("支配节点分析完成。\n\n");
}

/**
 * @brief 对所有函数执行支配节点分析
 */
void analyze_all_functions_dominance() {
    if (!ir_program_global) {
        printf("错误: 全局IR程序为空\n");
        return;
    }
    
    printf("===============================================\n");
    printf("开始执行支配节点分析\n");
    printf("===============================================\n\n");
    
    // 对程序中的每个函数执行支配节点分析
    for_vec(IR_function_ptr, func_ptr, ir_program_global->functions) {
        IR_function *func = *func_ptr;
        perform_dominance_analysis(func);
        printf("-----------------------------------------------\n\n");
    }
    
    printf("所有函数的支配节点分析完成。\n");
}
