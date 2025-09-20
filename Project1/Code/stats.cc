/***********************************************************************************
 * File:        stats.cc
 * Author:      Connor Savugot
 * Created:     2025-09-07
 * Updated:     2025-09-21
 * Version:     1.5
 *
 * Description: Implements statistics printing with labels/spacing/precision
 *              aligned to the provided validation files (letters a–q).
 *              - Inserts blank lines to match validator format (L2 case).
 *              - Uses L2 demand-only miss rate.
 ***********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <cassert>
#include <cmath>

#include <cstdint>
#include <memory>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

#include "stats.h"
#include "cache.h"

static double safe_rate(uint64_t miss, uint64_t total) {
    if (total == 0) return 0.0;
    return static_cast<double>(miss) / static_cast<double>(total);
}

void print_final_report(std::ostream& os,
                        const Cache& l1,
                        const Cache* l2_opt,
                        const AllStats& totals)
{
    // ----- Contents -----
    os << "===== L1 contents =====\n";
    l1.print_contents(os);

    if (l2_opt) {
        // Blank line between L1 and L2 contents (validator expects this)
        os << "\n";
        os << "===== L2 contents =====\n";
        l2_opt->print_contents(os);
    }

    // Blank line between contents and Measurements (validator expects this)
    os << "\n";

    const auto& A = totals.l1;
    const auto& B = totals.l2; // zeros if no L2

    // ----- Measurements (letters/labels; aligned columns; 4-decimal miss rates) -----
    const int label_w = 32;

    os << "===== Measurements =====\n";
    os << "a. L1 reads:"                  << std::setw(label_w - 11) << A.reads        << "\n";
    os << "b. L1 read misses:"            << std::setw(label_w - 18) << A.read_misses  << "\n";
    os << "c. L1 writes:"                 << std::setw(label_w - 12) << A.writes       << "\n";
    os << "d. L1 write misses:"           << std::setw(label_w - 19) << A.write_misses << "\n";
    os << "e. L1 miss rate:"              << std::setw(label_w - 16)
       << std::fixed << std::setprecision(4)
       << safe_rate(A.read_misses + A.write_misses, A.reads + A.writes) << "\n";
    os << std::setprecision(6); // restore default precision
    os << "f. L1 writebacks:"             << std::setw(label_w - 16) << A.writebacks   << "\n";

    // ECE463: no prefetching; still print L2/prefetch lines (zeros if unused)
    uint64_t l2_reads_demand      = B.reads;        // demand fills only (no prefetch in 463)
    uint64_t l2_read_miss_demand  = B.read_misses;  // demand read misses
    uint64_t l2_reads_prefetch    = 0;
    uint64_t l2_read_miss_pref    = 0;
    uint64_t l2_writes            = B.writes;
    uint64_t l2_write_misses      = B.write_misses;
    double   l2_miss_rate         = safe_rate(l2_read_miss_demand, l2_reads_demand); // demand-only
    uint64_t l2_writebacks        = B.writebacks;
    uint64_t l2_prefetches        = 0;

    os << "g. L1 prefetches:"             << std::setw(label_w - 16) << 0                  << "\n";
    os << "h. L2 reads (demand):"         << std::setw(label_w - 21) << l2_reads_demand    << "\n";
    os << "i. L2 read misses (demand):"   << std::setw(label_w - 28) << l2_read_miss_demand<< "\n";
    os << "j. L2 reads (prefetch):"       << std::setw(label_w - 23) << l2_reads_prefetch  << "\n";
    os << "k. L2 read misses (prefetch):" << std::setw(label_w - 30) << l2_read_miss_pref  << "\n";
    os << "l. L2 writes:"                 << std::setw(label_w - 12) << l2_writes          << "\n";
    os << "m. L2 write misses:"           << std::setw(label_w - 19) << l2_write_misses    << "\n";
    os << "n. L2 miss rate:"              << std::setw(label_w - 16)
       << std::fixed << std::setprecision(4) << l2_miss_rate          << "\n";
    os << std::setprecision(6);
    os << "o. L2 writebacks:"             << std::setw(label_w - 16) << l2_writebacks      << "\n";
    os << "p. L2 prefetches:"             << std::setw(label_w - 16) << l2_prefetches      << "\n";

    const uint64_t mem_traffic =
        (A.memory_reads + A.memory_writes + B.memory_reads + B.memory_writes);
    os << "q. memory traffic:"            << std::setw(label_w - 17) << mem_traffic        << "\n";
}
