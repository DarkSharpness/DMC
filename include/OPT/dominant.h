#pragma once
#include "IRbase.h"
#include <unordered_set>

namespace dark::IR {

struct dominantMaker {
  protected:
    inline static constexpr block &dummy = IRpool::__dummy__;
    std::vector         <block *> rpo;
    std::unordered_set  <block *> visited;

    static void initEdge(function *);
    void makePostOrder(block *);
    void iterate(block *);
    bool update(block *);
    void buildDomTree();
    void removeDummy();

    void setProperty(function *, bool);
    bool checkProperty(function *);
  public:

    dominantMaker(function *, bool = false);
    static void clean(function *);
};


} // namespace dark::IR
