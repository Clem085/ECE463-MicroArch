#ifndef STATS_H
#define STATS_H

#include "cache.h"
#include <ostream>

struct AllStats {
    AccessStats l1;
    AccessStats l2; // 0 if L2_SIZE == 0
};

void print_final_report(std::ostream& os,
                        const Cache& l1,
                        const Cache* l2_opt,
                        const AllStats& totals);

#endif // STATS_H
