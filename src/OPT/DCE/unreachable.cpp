#include "DCE/unreachable.h"
#include "IRnode.h"
#include "CFGbuilder.h"
#include <ranges>
#include <algorithm>

namespace dark::IR {

unreachableRemover::unreachableRemover(function *__func) {
    if (!checkProperty(__func)) return;

    /* Set UB block as unreachable. */
    for (auto *__p : __func->data) markUB(__p);
    CFGbuilder {__func};

    /* Dfs bidirectionally */
    dfs0(__func->data[0]);
    for (auto *__p : __func->data)
        if (__p->flow->as <return_stmt> ()) dfs1(__p);

    removeBlock(__func);

    for (auto *__p : __func->data) recordCFG(__p);
    for (auto *__p : __func->data) updatePhi(__p);
    for (auto *__p : __func->data) updateCFG(__p);

    /* The CFG is broken now. */
    CFGbuilder {__func};

    // Set the function property
    setProperty(__func);
}

void unreachableRemover::dfs0(block *__p) {
    if (visit0.insert(__p).second)
        for (auto *__q : __p->next) dfs0(__q);
}

void unreachableRemover::dfs1(block *__p) {
    if (visit1.insert(__p).second)
        for (auto *__q : __p->prev) dfs1(__q);
}

/**
 * @brief Remove all unreachable blocks from the function.
 */
void unreachableRemover::removeBlock(function *__func) {
    auto __beg = visit0.begin();
    auto __end = visit0.end();
    while (__beg != __end) { // Intersection
        visit1.count(*__beg) ? ++__beg : __beg = visit0.erase(__beg);
    }

    auto &&__range = __func->data |
        std::views::filter([this](block *__p) -> bool {
            bool __tmp = visit0.count(__p);
            if (!__tmp) IRpool::deallocate(__p);
            return __tmp;
        });
    auto __first = __func->data.begin();
    __func->data.resize(std::ranges::copy(__range, __first).out - __first);
}

/**
 * @brief This function detects all potential UB and turn
 * blocks with UB into unreachable.
 * 
 * Note that this function also transform the control flow.
 * If a block has constant branch, it will be transformed
 * into a jump statement.
 */
void unreachableRemover::markUB(block *__p) {
    // Hard undefined behavior:
    // - Load/Store of null pointer
    // - Division by zero
    // - Shift by negative value
    // - Signed integer overflow
    auto &&__criteria = [](statement *__node) -> const char * {
        if (auto *__load = __node->as <load_stmt> ()) {
            if (__load->addr == IRpool::__null__) return "null pointer dereference";
            if (__load->addr->as <undefined> ())  return "unknown memory access";
        } else if (auto *__store = __node->as <store_stmt> ()) {
            if (__store->addr == IRpool::__null__) return "null pointer dereference";
            if (__store->addr->as <undefined> ())  return "unknown memory access";
        } else if (auto *__get = __node->as <get_stmt> ()) {
            if (__get->addr == IRpool::__null__) return "null pointer dereference";
            if (__get->addr->as <undefined> ())  return "unknown memory access";
        } else if (auto *__bin = __node->as <binary_stmt> ()) {
            switch (__bin->op) {
                case __bin->DIV: case __bin->MOD:
                    if (__bin->rval == IRpool::__zero__) return "division by zero";
                    break; 
                case __bin->SHL: case __bin->SHR:
                    if (auto *__val = __bin->rval->as <integer_constant> ())
                        if (__val->value < 0) return "shift by negative value";
                    break;
            }
        }
        return nullptr;
    };

    for (auto __node : __p->data) {
        if (auto __msg = __criteria(__node)) {
            warning(std::format("Undefined behavior: {}", __msg));
            __p->phi.clear();
            __p->data.clear();
            __p->flow = IRpool::allocate <unreachable_stmt> ();
            return;
        }
    }

    if (auto *__br = __p->flow->as <branch_stmt> ()) {
        if (__br->cond->as <undefined> ()) {
            __p->phi.clear();
            __p->data.clear();
            __p->flow = IRpool::allocate <unreachable_stmt> ();
            return;
        }
        if (auto *__val = __br->cond->as <boolean_constant> ()) {
            __p->flow = IRpool::allocate <jump_stmt> (
                __br->branch[__val->value]);
        }
    }
}

/**
 * @brief Remove all phi entries branches that are not reachable.
 * After removing some blocks, some phi entries from those
 * unreachable blocks should be removed.
 */
void unreachableRemover::updatePhi(block *__p) {
    for (auto *__phi : __p->phi) {
        auto &&__range = __phi->list |
            std::views::filter([this, __p](phi_stmt::entry __e) -> bool {
                return edges.count({__e.from, __p});
            });
        auto __first = __phi->list.begin();
        __phi->list.resize(std::ranges::copy(__range, __first).out - __first);
    }
}

/**
 * @brief Update the control flow of the block.
 * 
 * Note that only branches will be updated.
 * That's because for jumps, the target block is always
 * reachable, or this block will have been removed.
 */
void unreachableRemover::updateCFG(block *__p) {
    if (auto *__br = __p->flow->as <branch_stmt> ()) {
        if (__br->branch[0] == __br->branch[1])
            __p->flow = IRpool::allocate <jump_stmt> (__br->branch[0]);
        else if (!visit0.count(__br->branch[0]))
            __p->flow = IRpool::allocate <jump_stmt> (__br->branch[1]);
        else if (!visit0.count(__br->branch[1]))
            __p->flow = IRpool::allocate <jump_stmt> (__br->branch[0]);
    }
}

/**
 * @brief Record all edges in the CFG.
 */
void unreachableRemover::recordCFG(block *__p) {
    for (auto __next : __p->next)
        if (visit0.count(__next)) edges.insert({__p, __next});
}

void unreachableRemover::setProperty(function *__func) {
    __func->has_rpo = false;
    __func->has_cfg = true;
    __func->has_dom = false;
    __func->has_fro = false;
    __func->rpo.clear();
}

bool unreachableRemover::checkProperty(function *__func) {
    return !__func->is_unreachable();
}

} // namespace dark::IR
