#pragma once
#include "IRtype.h"
#include <string>
#include <vector>
#include <unordered_map>

/* Some basic definitions */
namespace dark::IR {

/* A definition can be variable / literal / temporary */
struct definition {
    template <std::derived_from <definition> _Tp>
    _Tp *as() { return dynamic_cast <_Tp *> (this); }
    virtual std::string data()          const = 0;
    virtual typeinfo get_value_type()   const = 0;
    virtual ~definition() = default;
    typeinfo get_point_type() const { return --get_value_type(); }
};

/* A special type whose value is undefined.  */
struct undefined final: definition {
    typeinfo type;
    explicit undefined(typeinfo __type) : type(__type) {}
    typeinfo get_value_type()   const override;
    std::string        data()   const override;
    ~undefined() override = default;
};
struct node;
/* Literal constants. */
struct literal;
/* Non literal value. (Variable / Temporary) */
struct non_literal : definition {
    typeinfo    type; /* Type wrapper. */
    std::string name; /* Unique name.  */
    typeinfo get_value_type()   const override final { return type; }
    std::string        data()   const override final { return name; }
};
/* Temporary values. */
struct temporary final : non_literal {
    node *def {}; /* The statement that defines this temporary. */
};
/* Variables are pointers to value. */
struct variable : non_literal {};
/* Function parameters. */
struct argument         final  : variable {};
struct local_variable   final  : variable {};
struct global_variable  final  : variable {
    const literal *init {}; // Initial value of the global variable.
    bool    is_constant {}; // If true, the global variable's value is a constant.
};

/* Literal constants. */
struct literal : definition {
    using ssize_t = std::make_signed_t <size_t>;
    /* Type that is used to be print out in global variable declaration. */
    virtual std::string IRtype() const = 0;
    /* Return the inner integer representative. */
    virtual ssize_t to_integer() const = 0;
};

/* String literal. */
struct string_constant final : literal {
    std::string     context;
    cstring_type    stype;
    explicit string_constant(std::string __ctx) :
        context(std::move(__ctx)), stype(context.size()) {}

    std::string IRtype() const override;
    ssize_t to_integer() const override;
    std::string  data()  const override;
    typeinfo get_value_type() const override;
    std::string     ASMdata() const;
};

/* Integer literal. */
struct integer_constant final : literal {
    const int value;
    explicit integer_constant(int __val) : value(__val) {}
    std::string IRtype() const override;
    ssize_t to_integer() const override;
    std::string   data() const override;
    typeinfo get_value_type() const override;
};

/* Boolean literal */
struct boolean_constant final : literal {
    const bool value;
    explicit boolean_constant(bool __val) : value(__val) {}
    std::string IRtype() const override;
    ssize_t to_integer() const override;
    std::string   data() const override;
    typeinfo get_value_type() const override;
};

/**
 * Pointer constant.
 * 
 * If (.var != nullptr) :
 * global_variable @str -> string_constant.
 * pointer_constant     -> global_variable @str.
 * 
 * Else, it represents just "nullptr"
*/
struct pointer_constant final : literal {
    const global_variable *const var;
    explicit pointer_constant(const global_variable * __var)
        : var(__var) {}
    std::string IRtype() const override;
    ssize_t to_integer() const override;
    std::string   data() const override;
    typeinfo get_value_type() const override;
};


} /* namespace dark::IR */


/* IR visitor part. */
namespace dark::IR {

struct block;
struct function;
struct IRbase;
struct IRbuilder;

struct node : hidden_impl {
    using _Def_List = std::vector <definition *>;
    /* Short form for dynamic cast. */
    template <typename _Tp> requires std::is_base_of_v <node, _Tp>
    _Tp *as() { return dynamic_cast <_Tp *> (this); }
    virtual void accept(IRbase * __v) = 0;
    /* Return the string form IR. */
    virtual std::string data() const = 0;
    /* Return the temporary this statment defines. */
    virtual temporary *get_def() const = 0;
    /* Return all the usages of the node. */
    virtual _Def_List  get_use() const = 0;
    /* Update an old value with a new one. */
    virtual void update(definition *, definition *) = 0;
    /* To avoid memory leak. */
    virtual ~node() = default;
};

struct compare_stmt;
struct binary_stmt;
struct jump_stmt;
struct branch_stmt;
struct call_stmt;
struct load_stmt;
struct store_stmt;
struct return_stmt;
struct alloca_stmt;
struct get_stmt;
struct phi_stmt;
struct unreachable_stmt;

using  statement = node;
struct flow_statement : statement {
    /* Return the temporary this statment defines. */
    temporary *get_def() const override final { return nullptr; }
};
struct memory_statement : statement {};

struct IRbase {
    void visit(node * __n) { return __n->accept(this); }

    virtual void visitCompare(compare_stmt *) = 0;
    virtual void visitBinary(binary_stmt *) = 0;
    virtual void visitJump(jump_stmt *) = 0;
    virtual void visitBranch(branch_stmt *) = 0;
    virtual void visitCall(call_stmt *) = 0;
    virtual void visitLoad(load_stmt *) = 0;
    virtual void visitStore(store_stmt *) = 0;
    virtual void visitReturn(return_stmt *) = 0;
    virtual void visitGet(get_stmt *) = 0;
    virtual void visitPhi(phi_stmt *) = 0;
    virtual void visitUnreachable(unreachable_stmt *) = 0;

    virtual ~IRbase() = default;
};

struct loopInfo;

/**
 * @brief A block consists of:
 * 1. Phi functions
 * 2. Statements
 * 3. Control flow statement
 * 
 * Hidden impl is intended to provide future
 * support for loop or other info.
 */
struct block final : hidden_impl {
    std::vector <phi_stmt *> phi;   // All phi functions
    flow_statement *flow {};        // Control flow statement
    std::string     name;           // Label name

    std::vector <statement*>    data;   // All normal statements
    std::vector  <block *>      prev;   // Predecessor blocks
    std::vector  <block *>      next;   // Successor blocks
    std::vector  <block *>      dom;    // Be dominated by these block
    std::vector  <block *>      fro;    // Has these block as frontier

    block *idom {};                 // Immediate dominator
    loopInfo *loop {};              // Pointer to loop info.
    std::string comments;           // Comments for this block

    void push_phi(phi_stmt *);          // Push back a phi function
    void push_back(statement *);        // Push back a statement
    void print(std::ostream &) const;   // Print the block data
    bool is_unreachable() const;        // Is this block unreachable?
};

struct function final : hidden_impl {
  private:
    std::size_t loop_count {};  // Count of for loops
    std::size_t cond_count {};  // Count of branches
    std::unordered_map <std::string, std::size_t> temp_count; // Count of temporaries

  public:
    typeinfo    type;       // Return type
    std::string name;       // Function name
    std::vector  <block  *>  data;   // All blocks
    std::vector <argument *> args;   // Arguments
    std::vector <local_variable *> locals; // Local variables

    /* Some basic function meta data. */
    struct {
        unsigned is_builtin : 1 {}; // Whether this function is builtin
        unsigned has_input  : 1 {}; // Whether this function has input
        unsigned has_output : 1 {}; // Whether this function has output
        unsigned has_rpo    : 1 {}; // Whether rpo is available
        unsigned has_cfg    : 1 {}; // Whether CFG is built
        unsigned has_dom    : 1 {}; // Whether dom tree is built
        unsigned has_fro    : 1 {}; // Whether frontier is made
        unsigned is_post    : 1 {}; // Whether post dom or not
    };

    std::vector <block *> rpo;  // Reverse post order

    temporary *create_temporary(typeinfo, const std::string &);
    std::string register_temporary(const std::string &);

    void push_back(block *);
    void print(std::ostream &) const;   // Print the function data
    bool is_unreachable() const;        // Is this function unreachable?
    bool is_side_effective() const;     // Is this function side effective?
};

/* A global memory pool for IR. */
struct IRpool {
  private:
    IRpool() = delete;
    inline static central_allocator <node>        pool1 {};
    inline static central_allocator <non_literal> pool2 {};
    using _Function_Ptr = function *;

  public:
    inline static block             __dummy__   {};
    inline static pointer_constant *__null__    {};
    inline static integer_constant *__zero__    {};
    inline static integer_constant *__pos1__    {};
    inline static integer_constant *__neg1__    {};
    inline static boolean_constant *__true__    {};
    inline static boolean_constant *__false__   {};

    static const _Function_Ptr builtin_function;

    /* Pool from string to global_variable initialized by string. */
    inline static std::unordered_map <std::string, global_variable> str_pool {};

    /* Initialize the pool. */
    static void init_pool();
    /* Allocate one node. */
    template <typename _Tp, typename... _Args>
    requires 
        (!std::same_as <unreachable_stmt, _Tp>
    &&  std::constructible_from <_Tp, _Args...>
    &&  std::is_base_of_v <statement, _Tp>)
    static _Tp *allocate(_Args &&...__args) {
        return pool1.allocate <_Tp> (std::forward <_Args> (__args)...);
    }

    /* Allocate one non_literal. */
    template <std::derived_from <non_literal> _Tp>
    static _Tp *allocate() { return pool2.allocate <_Tp> (); }

    /* Allocate unreachable. */
    template <std::same_as <unreachable_stmt> _Tp>
    static unreachable_stmt *allocate();

    /* Allocate one block. */
    template <std::same_as <block> _Tp>
    static block *allocate();

    static integer_constant *create_integer(int);
    static boolean_constant *create_boolean(bool);
    static pointer_constant *create_pointer(const global_variable *);
    static global_variable  *create_string(const std::string &);
    static undefined *create_undefined(typeinfo, int = 0);

    static void print_builtin(std::ostream &);
    static void deallocate(block *);
};


} // namespace dark::IR
