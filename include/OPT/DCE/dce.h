#pragma once
#include "queue.h"
#include "IRbase.h"

namespace dark::IR {

struct DeadCodeEliminator {
    DeadCodeEliminator(function *);
  private:
    work_queue <statement *> workList;
    bool checkProperty(function *);
    void setProperty(function *);
};

} // namespace dark::IR
