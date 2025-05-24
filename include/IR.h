//
// Created by hby on 22-11-3.
//

#ifndef CODE_IR_H
#define CODE_IR_H

#include <stdbool.h>
#include <object.h>
#include <container/vector.h>
#include <container/list.h>
#include <container/treap.h>
#include <macro.h>
#include <stdio.h>

//// ================================== IR 变量 & 标签 ==================================

typedef unsigned IR_var;   // IR变量类型，用数字表示 v1, v2...
typedef unsigned IR_label; // IR标签类型，用数字表示 L1, L2...
typedef unsigned IR_DEC_size_t; // IR声明大小类型
enum {IR_VAR_NONE = 0}; // 表示无效或不存在的IR变量
enum {IR_LABEL_NONE = 0}; // 表示无效或不存在的IR标签

DEF_VECTOR(IR_var) // 定义 IR_var 类型的动态数组 (Vec_IR_var)

/**
 * @brief 生成一个新的唯一IR变量编号。
 * @return 返回一个新的IR变量编号。
 */
extern IR_var ir_var_generator();

/**
 * @brief 生成一个新的唯一IR标签编号。
 * @return 返回一个新的IR标签编号。
 */
extern IR_label ir_label_generator();

/**
 * @brief 回收一个IR变量编号，使其可被后续复用。
 * @param var 要回收的IR变量编号。
 */
extern void ir_func_var_recycle(IR_var var);

/**
 * @brief 回收一个IR标签编号，使其可被后续复用。
 * @param label 要回收的IR标签编号。
 */
extern void ir_label_recycle(IR_label label);

//// ================================== IR (中间表示) ==================================

//// IR_val (IR值)

/**
 * @brief 表示一个IR操作数，可以是常量或变量。
 */
typedef struct IR_val {
    bool is_const; // 标记是否为常量
    union {
        IR_var var;        // 如果是变量，则存储变量编号
        int const_val;  // 如果是常量，则存储常量值
    };
} IR_val;

//// IR_stmt (IR语句)

/**
 * @brief IR语句的类型枚举。
 */
typedef enum {
    IR_OP_STMT,         // 操作语句 (如：rd := rs1 op rs2)
    IR_ASSIGN_STMT,     // 赋值语句 (如：rd := rs)
    IR_LOAD_STMT,       // 加载语句 (如：rd := *rs_addr)
    IR_STORE_STMT,      // 存储语句 (如：*rd_addr = rs)
    IR_IF_STMT,         // 条件跳转语句
    IR_GOTO_STMT,       // 无条件跳转语句
    IR_CALL_STMT,       // 函数调用语句
    IR_RETURN_STMT,     // 返回语句
    IR_READ_STMT,       // 读语句
    IR_WRITE_STMT       // 写语句
} IR_stmt_type;

typedef struct IR_stmt IR_stmt, *IR_stmt_ptr; // IR语句结构体及其指针类型

/**
 * @brief 表示IR语句中使用的变量（use）。
 */
typedef struct IR_use {
    unsigned use_cnt; // use变量的个数
    IR_val *use_vec;  // use变量的数组
} IR_use;

/**
 * @brief IR语句的虚函数表结构体。
 * 用于实现类似面向对象的多态行为。
 */
struct IR_stmt_virtualTable {
    /**
     * @brief 析构（清理）IR语句占用的资源。
     * @param stmt 指向要析构的IR_stmt的指针。
     */
    void (*teardown) (IR_stmt *stmt);

    /**
     * @brief 打印IR语句到指定的输出流。
     * @param stmt 指向要打印的IR_stmt的指针。
     * @param out 输出文件流指针。
     */
    void (*print) (IR_stmt *stmt, FILE *out);

    /**
     * @brief 获取IR语句定义（def）的变量。
     * @param stmt 指向IR_stmt的指针。
     * @return 返回定义的变量编号；如果语句没有定义变量，则返回 IR_VAR_NONE。
     */
    IR_var (*get_def) (IR_stmt *stmt);

    /**
     * @brief 获取IR语句使用（use）的变量向量。
     * @param stmt 指向IR_stmt的指针。
     * @return 返回一个 IR_use 结构体，包含使用的变量信息。
     */
    IR_use (*get_use_vec) (IR_stmt *stmt);
};

/**
 * @brief IR语句的基类结构体宏定义。
 * 包含虚函数表指针、语句类型和是否为死代码的标记。
 */
#define CLASS_IR_stmt struct { \
        struct IR_stmt_virtualTable const *vTable; \
        IR_stmt_type stmt_type; \
        bool dead; };

/**
 * @brief IR语句的通用结构体。
 */
struct IR_stmt {
    CLASS_IR_stmt
};

DEF_LIST(IR_stmt_ptr) // 定义 IR_stmt_ptr 类型的双向链表 (List_IR_stmt_ptr)

/**
 * @brief 析构（清理）IR语句，调用其虚函数表中的teardown方法。
 * @param stmt 指向要析构的IR_stmt的指针。
 */
extern void IR_stmt_teardown(IR_stmt *stmt);

//// IR_block (IR基本块)

/**
 * @brief 表示一个IR基本块。
 */
typedef struct {
    IR_label label;         // 基本块的标签，如果为 IR_LABEL_NONE 则表示无标签
    bool dead;              // 标记该基本块是否为死代码
    List_IR_stmt_ptr stmts; // 基本块内的语句列表
} IR_block, *IR_block_ptr;

DEF_LIST(IR_block_ptr) // 定义 IR_block_ptr 类型的双向链表 (List_IR_block_ptr)

/**
 * @brief 初始化一个IR基本块。
 * @param block 指向要初始化的IR_block的指针。
 * @param label 基本块的标签。
 */
extern void IR_block_init(IR_block *block, IR_label label);

/**
 * @brief 析构（清理）一个IR基本块及其包含的所有语句。
 * @param block 指向要析构的IR_block的指针。
 */
extern void IR_block_teardown(IR_block *block);

//// IR_function (IR函数)

/**
 * @brief 表示函数内的数组或结构体声明。
 */
typedef struct {
    IR_var dec_addr;        // 存储声明变量基地址的变量编号
    IR_DEC_size_t dec_size; // 声明的大小（字节数）
} IR_Dec;
DEF_MAP(IR_var, IR_Dec) // 定义从 IR_var (声明变量) 到 IR_Dec 的映射 (Map_IR_var_IR_Dec)

DEF_MAP(IR_label, IR_block_ptr) // 定义从 IR_label 到 IR_block_ptr 的映射 (Map_IR_label_IR_block_ptr)
typedef List_IR_block_ptr *List_ptr_IR_block_ptr; // 指向 List_IR_block_ptr 的指针类型
DEF_MAP(IR_block_ptr, List_ptr_IR_block_ptr) // 定义从 IR_block_ptr 到 List_ptr_IR_block_ptr 的映射

/**
 * @brief 表示一个IR函数。
 */
typedef struct IR_function{
    char *func_name;        // 函数名
    Vec_IR_var params;      // 函数参数列表 (Vec_IR_var类型)
    Map_IR_var_IR_Dec map_dec; // 函数内声明的变量及其信息 (dec_var => (addr_var, size))
    List_IR_block_ptr blocks; // 函数内的基本块列表

    // 控制流图 (Control Flow Graph)
    IR_block *entry, *exit; // 函数的入口和出口基本块
    Map_IR_label_IR_block_ptr map_blk_label; // 标签到基本块指针的映射
    Map_IR_block_ptr_List_ptr_IR_block_ptr blk_pred, blk_succ; // 基本块指针到其前驱/后继基本块列表指针的映射
} IR_function, *IR_function_ptr;

DEF_VECTOR(IR_function_ptr) // 定义 IR_function_ptr 类型的动态数组 (Vec_IR_function_ptr)

/**
 * @brief 初始化一个IR函数。
 * @param func 指向要初始化的IR_function的指针。
 * @param func_name 函数的名称。
 */
extern void IR_function_init(IR_function *func, const char *func_name);

/**
 * @brief 在函数中插入一个变量声明。
 * @param func 指向IR_function的指针。
 * @param var 要声明的变量的编号。
 * @param dec_size 声明的大小。
 * @return 返回存储声明变量基地址的变量编号。
 */
extern IR_var IR_function_insert_dec(IR_function *func, IR_var var, IR_DEC_size_t dec_size);

/**
 * @brief 构建函数的控制流图 (CFG)。
 * @param func 指向IR_function的指针。
 */
extern void IR_function_build_graph(IR_function *func);

/**
 * @brief 完成函数的构建，例如添加统一的入口和出口块，并构建CFG。
 * @param func 指向IR_function的指针。
 */
extern void IR_function_closure(IR_function *func);

//// IR_program (IR程序)

/**
 * @brief 表示整个IR程序，即一系列函数的集合。
 */
typedef struct IR_program {
    Vec_IR_function_ptr functions; // 程序中的函数列表
} IR_program;

extern IR_program *ir_program_global; // 全局IR程序实例指针

/**
 * @brief 初始化一个IR程序。
 * @param program 指向要初始化的IR_program的指针。
 */
extern void IR_program_init(IR_program *program);

/**
 * @brief 析构（清理）整个IR程序及其包含的所有函数和基本块。
 * @param ptr 指向要析构的IR_program的指针 (void* 类型以匹配通用析构宏)。
 */
extern void IR_program_teardown(void *ptr);

/**
 * @brief 向当前函数的最后一个基本块中追加一条IR语句。
 * 如果最后一个基本块的最后一条语句是跳转相关的，则会创建一个新的基本块来存放新语句。
 * @param func 指向IR_function的指针。
 * @param stmt 指向要追加的IR_stmt的指针。
 */
extern void IR_function_push_stmt(IR_function *func, IR_stmt *stmt);

/**
 * @brief 向当前函数中追加一个新的基本块，并以指定的标签开始。
 * 会处理前一个基本块末尾可能存在的冗余GOTO语句。
 * @param func 指向IR_function的指针。
 * @param label 新基本块的标签。
 */
extern void IR_function_push_label(IR_function *func, IR_label label);

//// ================================== IR 打印 API ==================================

/**
 * @brief 将整个IR程序输出到指定的路径。如果路径为NULL，则输出到stdout。
 * @param output_IR_path 输出文件的路径字符串。
 */
extern void IR_output(const char *output_IR_path);

/**
 * @brief 将一个IR基本块打印到指定的输出流。
 * @param block 指向要打印的IR_block的指针。
 * @param out 输出文件流指针。
 */
extern void IR_block_print(IR_block *block, FILE *out);

/**
 * @brief 将一个IR函数打印到指定的输出流。
 * @param func 指向要打印的IR_function的指针。
 * @param out 输出文件流指针。
 */
extern void IR_function_print(IR_function *func, FILE *out);

/**
 * @brief 将整个IR程序打印到指定的输出流。
 * @param program 指向要打印的IR_program的指针。
 * @param out 输出文件流指针。
 */
extern void IR_program_print(IR_program *program, FILE *out);

//// ================================== Stmt (具体语句类型定义及初始化函数) ==================================

/**
 * @brief IR二元操作的类型枚举。
 */
typedef enum {
    IR_OP_ADD, // 加法
    IR_OP_SUB, // 减法
    IR_OP_MUL, // 乘法
    IR_OP_DIV  // 除法
} IR_OP_TYPE;

/**
 * @brief IR操作语句 (rd := rs1 op rs2)。
 */
typedef struct {
    CLASS_IR_stmt       // 继承自IR_stmt的通用字段
    IR_OP_TYPE op;      // 操作类型
    IR_var rd;          // 目标寄存器（定义的变量）
    union {
        IR_val use_vec[2]; // use数组，方便通用处理
        struct { IR_val rs1, rs2; }; // 源操作数1和源操作数2
    };
} IR_op_stmt;

/**
 * @brief 初始化一个IR操作语句。
 * @param op_stmt 指向要初始化的IR_op_stmt的指针。
 * @param op 操作类型。
 * @param rd 目标变量。
 * @param rs1 源操作数1。
 * @param rs2 源操作数2。
 */
extern void IR_op_stmt_init(IR_op_stmt *op_stmt, IR_OP_TYPE op,
                            IR_var rd, IR_val rs1, IR_val rs2);

/**
 * @brief IR赋值语句 (rd := rs)。
 */
typedef struct {
    CLASS_IR_stmt       // 继承自IR_stmt的通用字段
    IR_var rd;          // 目标寄存器（定义的变量）
    union {
        IR_val use_vec[1]; // use数组
        struct { IR_val rs; }; // 源操作数
    };
} IR_assign_stmt;

/**
 * @brief 初始化一个IR赋值语句。
 * @param assign_stmt 指向要初始化的IR_assign_stmt的指针。
 * @param rd 目标变量。
 * @param rs 源操作数。
 */
extern void IR_assign_stmt_init(IR_assign_stmt *assign_stmt, IR_var rd, IR_val rs);

/**
 * @brief IR加载语句 (rd := *rs_addr)。
 */
typedef struct {
    CLASS_IR_stmt       // 继承自IR_stmt的通用字段
    IR_var rd;          // 目标寄存器（定义的变量）
    union {
        IR_val use_vec[1]; // use数组
        struct { IR_val rs_addr; }; // 源地址操作数
    };
} IR_load_stmt;

/**
 * @brief 初始化一个IR加载语句。
 * @param load_stmt 指向要初始化的IR_load_stmt的指针。
 * @param rd 目标变量。
 * @param rs_addr 源地址操作数。
 */
extern void IR_load_stmt_init(IR_load_stmt *load_stmt, IR_var rd, IR_val rs_addr);

/**
 * @brief IR存储语句 (*rd_addr = rs)。
 */
typedef struct {
    CLASS_IR_stmt       // 继承自IR_stmt的通用字段
    union {
        IR_val use_vec[2]; // use数组
        struct { IR_val rd_addr, rs; }; // 目标地址操作数和源操作数
    };
} IR_store_stmt;

/**
 * @brief 初始化一个IR存储语句。
 * @param store_stmt 指向要初始化的IR_store_stmt的指针。
 * @param rd_addr 目标地址操作数。
 * @param rs 源操作数。
 */
extern void IR_store_stmt_init(IR_store_stmt *store_stmt, IR_val rd_addr, IR_val rs);

/**
 * @brief IR关系操作的类型枚举。
 */
typedef enum {
    IR_RELOP_EQ, // 等于 (==)
    IR_RELOP_NE, // 不等于 (!=)
    IR_RELOP_GT, // 大于 (>)
    IR_RELOP_GE, // 大于等于 (>=)
    IR_RELOP_LT, // 小于 (<)
    IR_RELOP_LE  // 小于等于 (<=)
} IR_RELOP_TYPE;

/**
 * @brief IR条件跳转语句 (IF rs1 relop rs2 GOTO true_label ELSE GOTO false_label)。
 */
typedef struct {
    CLASS_IR_stmt           // 继承自IR_stmt的通用字段
    IR_RELOP_TYPE relop;    // 关系操作类型
    union {
        IR_val use_vec[2];     // use数组
        struct { IR_val rs1, rs2; }; // 比较的两个操作数
    };
    IR_label true_label, false_label; // 条件为真/假时跳转的标签
    IR_block *true_blk, *false_blk;   // 条件为真/假时跳转的目标基本块指针 (在CFG构建后填充)
} IR_if_stmt;

/**
 * @brief 初始化一个IR条件跳转语句。
 * @param if_stmt 指向要初始化的IR_if_stmt的指针。
 * @param relop 关系操作类型。
 * @param rs1 操作数1。
 * @param rs2 操作数2。
 * @param true_label 条件为真时跳转的标签。
 * @param false_label 条件为假时跳转的标签 (如果为 IR_LABEL_NONE，则表示顺序执行到下一个块或无else分支)。
 */
extern void IR_if_stmt_init(IR_if_stmt *if_stmt, IR_RELOP_TYPE relop,
                            IR_val rs1, IR_val rs2,
                            IR_label true_label, IR_label false_label);
/**
 * @brief 翻转IF语句的条件和跳转目标。
 * 例如，将 "IF a < b GOTO L1 ELSE GOTO L2" 变为 "IF a >= b GOTO L2 ELSE GOTO L1"。
 * @param if_stmt 指向要翻转的IR_if_stmt的指针。
 */
extern void IR_if_stmt_flip(IR_if_stmt *if_stmt);

/**
 * @brief IR无条件跳转语句 (GOTO label)。
 */
typedef struct {
    CLASS_IR_stmt   // 继承自IR_stmt的通用字段
    IR_label label; // 跳转的目标标签
    IR_block *blk;  // 跳转的目标基本块指针 (在CFG构建后填充)
} IR_goto_stmt;

/**
 * @brief 初始化一个IR无条件跳转语句。
 * @param goto_stmt 指向要初始化的IR_goto_stmt的指针。
 * @param label 跳转的目标标签。
 */
extern void IR_goto_stmt_init(IR_goto_stmt *goto_stmt, IR_label label);

/**
 * @brief IR返回语句 (RETURN rs)。
 */
typedef struct {
    CLASS_IR_stmt   // 继承自IR_stmt的通用字段
    union {
        IR_val use_vec[1]; // use数组
        struct { IR_val rs; }; // 返回值
    };
} IR_return_stmt;

/**
 * @brief 初始化一个IR返回语句。
 * @param return_stmt 指向要初始化的IR_return_stmt的指针。
 * @param ret_val 返回值。
 */
extern void IR_return_stmt_init(IR_return_stmt *return_stmt, IR_val ret_val);

/**
 * @brief IR函数调用语句 (rd := CALL func_name(argv))。
 * 参数通过之前的ARG语句传递（此框架中直接包含在argv中）。
 */
typedef struct {
    CLASS_IR_stmt       // 继承自IR_stmt的通用字段
    IR_var rd;          // 存储调用结果的目标变量 (如果函数无返回值，可能为 IR_VAR_NONE)
    char *func_name;    // 被调用函数的名称
    unsigned argc;      // 参数个数
    IR_val *argv;       // 参数值数组 (动态分配)
} IR_call_stmt;

/**
 * @brief 初始化一个IR函数调用语句。
 * @param call_stmt 指向要初始化的IR_call_stmt的指针。
 * @param rd 目标变量。
 * @param func_name 被调用函数的名称。
 * @param argc 参数个数。
 * @param argv 参数值数组 (此函数会复制argv的内容)。
 */
extern void IR_call_stmt_init(IR_call_stmt *call_stmt, IR_var rd, const char *func_name,
                              unsigned argc, IR_val *argv);

/**
 * @brief IR读语句 (READ rd)。
 */
typedef struct {
    CLASS_IR_stmt   // 继承自IR_stmt的通用字段
    IR_var rd;      // 读取输入并存储到的目标变量
} IR_read_stmt;

/**
 * @brief 初始化一个IR读语句。
 * @param read_stmt 指向要初始化的IR_read_stmt的指针。
 * @param rd 目标变量。
 */
extern void IR_read_stmt_init(IR_read_stmt *read_stmt, IR_var rd);

/**
 * @brief IR写语句 (WRITE rs)。
 */
typedef struct {
    CLASS_IR_stmt   // 继承自IR_stmt的通用字段
    union {
        IR_val use_vec[1]; // use数组
        struct { IR_val rs; }; // 要写入（打印）的值
    };
} IR_write_stmt;

/**
 * @brief 初始化一个IR写语句。
 * @param write_stmt 指向要初始化的IR_write_stmt的指针。
 * @param rs 要写入的值。
 */
extern void IR_write_stmt_init(IR_write_stmt *write_stmt, IR_val rs);

#endif //CODE_IR_H
