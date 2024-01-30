#pragma once
#include "IRbase.h"

namespace dark::IR {

using  statement = node;
struct flow_statement : statement {};
struct memory_statement : statement {};
struct function;
struct block;

struct compare_stmt final : statement {
    temporary  *dest;
    definition *lval;
    definition *rval;
    enum compare_op {
        EQ, NE, LT, LE, GT, GE
    } op;

    void accept(IRbase *v)  override { v->visitCompare(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct binary_stmt final : statement {
    temporary  *dest;
    definition *lval;
    definition *rval;
    enum binary_op {
        ADD, SUB, MUL, DIV, MOD, SHL, SHR, AND, OR, XOR
    } op;

    void accept(IRbase *v)  override { v->visitBinary(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct jump_stmt final : flow_statement {
    block *dest;    // Next block

    void accept(IRbase *v)  override { v->visitJump(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct branch_stmt final : flow_statement {
    definition *cond;   // Condition
    block *branch[2];   // br[0] false, br[1] true

    void accept(IRbase *v)  override { v->visitBranch(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct call_stmt final : statement {
    temporary *dest {};     // Return value
    function  *func {};     // Function to call.
    std::vector <definition *> args;    // Arguments

    void accept(IRbase *v)  override { v->visitCall(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct load_stmt final : memory_statement {
    temporary   *dest;    // Loaded value
    definition  *addr;    // Source address

    void accept(IRbase *v)  override { v->visitLoad(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct store_stmt final : memory_statement {
    definition  *addr;    // Destination address
    definition  *src_;    // Stored value

    void accept(IRbase *v)  override { v->visitStore(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct return_stmt final : flow_statement {
    definition *retval {};  // Return value.
    function   *func {};    // Function to end.

    void accept(IRbase *v)  override { v->visitReturn(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct alloca_stmt final : statement {
    local_variable *dest;   // Allocated variable

    void accept(IRbase *v)  override { v->visitAlloca(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

/* Get elementptr. */
struct get_stmt final : statement {
    static constexpr std::size_t NPOS = -1;
    temporary  *dest;   // Result
    definition *addr;   // Source address

    definition *index   {};     // Index of the array
    std::size_t member  {NPOS}; // Member index

    void accept(IRbase *v)  override { v->visitGet(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

struct unreachable_stmt final : flow_statement {
    void accept(IRbase *v)  override { v->visitUnreachable(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

/* Phi function. Which is not a "real" statement. */
struct phi_stmt final : statement {
    struct entry {
        block      *from;
        definition *init;
    };
    using _Phi_List = std::vector <entry>;

    temporary *dest;   // Result
    _Phi_List  list;    // Phi entries

    void accept(IRbase *v)  override { v->visitPhi(this); }
    std::string data()      const override;
    temporary *get_def()    const override;
    _Def_List  get_use()    const override;
};

/**
 * @brief A block consists of:
 * 1. Phi functions
 * 2. Statements
 * 3. Control flow statement
 * 
 * Hidden impl is intended to provide future
 * support for loop or other info.
 */
struct block final : std::vector <statement*> , hidden_impl {
    std::vector <phi_stmt *> phi;   // All phi functions
    flow_statement *flow {};        // Control flow statement
    std::string     name;           // Label name

    std::vector  <block *>      prev;   // Predecessor blocks
    fixed_vector <block *, 2>   next;   // Successor blocks

    void print(std::ostream &) const;   // Print the block data
    bool is_unreachable() const;        // Is this block unreachable?
};

struct function final : hidden_impl {
  private:
    std::size_t loop_count {};  // Count of for loops
    std::size_t cond_count {};  // Count of branches

  public:
    std::string name;       // Function name
    typeinfo    type;       // Return type
    std::vector <argument *>    args;    // Arguments
    std::vector  <block  *>     blocks;  // All blocks

    temporary *create_temporary(typeinfo, std::string_view = "");

    void push_back(block *);
    void push_back(statement *);
    void print(std::ostream &) const;   // Print the function data
    bool is_unreachable() const;        // Is this function unreachable?
};

struct initialization {
    variable *dst;
    literal  *src;
    void print(std::ostream &) const;   // Print the initialization data
};


} // namespace dark::IR
