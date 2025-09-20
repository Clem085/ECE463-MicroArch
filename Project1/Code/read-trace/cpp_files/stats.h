#ifndef STATS_H
#define STATS_H

#include "cache.h"
#include <ostream>

struct AllStats {
    AccessStats l1;
    AccessStats l2; // will remain zeroed if L2_SIZE == 0
};

// Print the final report (config block, contents, and measurements).
// Implement the exact formatting your grader expects here.
void print_final_report(std::ostream& os,
                        const Cache& l1,
                        const Cache* l2_opt,
                        const AllStats& totals);

#endif // STATS_H
