#include <IR.h>

DEF_SET(IR_block_ptr) 
DEF_LIST(IR_block_ptr) // List_IR_block_ptr 已在 IR.h 中定义

typedef struct Loop {
    IR_block_ptr header;            // 循环头节点
    Set_IR_block_ptr blocks;        // 构成循环的所有基本块的集合
    List_IR_block_ptr back_edges_sources; // 指向头节点的回边的源节点列表 (即循环尾)
    // 可以添加其他信息，如 preheader, exit_blocks, 循环嵌套深度等
    // struct Loop* parent_loop; // 指向外层循环
    // List_Loop_ptr nested_loops; // 嵌套循环列表
} Loop, *Loop_ptr;

DEF_LIST(Loop_ptr) // List_Loop_ptr