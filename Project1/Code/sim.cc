/***********************************************************************************
 * File:        sim.cc
 * Author:      Connor Savugot
 * Created:     2025-09-07
 * Updated:     2025-09-21
 * Version:     1.1
 *
 * Description: Entry point for the simulator. Parses command-line arguments,
 *              opens the trace file, builds cache hierarchy, processes requests,
 *              and prints final simulator outputs (format aligned to val files).
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

#include "sim.h"
#include "cache.h"
#include "stats.h"

// Return the final path component (no directories).
static const char* basename_c(const char* path) {
    if (!path) return "";
    const char* s = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') s = p + 1;
    }
    return s;
}

/*  Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
*/
int main (int argc, char *argv[]) {
   FILE *fp;                 // File pointer.
   char *trace_file;         // Trace file name.
   cache_params_t params;    // See sim.h
   char rw;                  // 'r' or 'w'
   uint32_t addr;            // 32-bit address from trace

   // Expect exactly 8 command-line arguments (argc == 9 including program name).
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      printf("Usage: %s BLOCKSIZE L1_SIZE L1_ASSOC L2_SIZE L2_ASSOC PREF_N PREF_M TRACE_FILE\n", argv[0]);
      exit(EXIT_FAILURE);
   }

   // Parse CLI
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);  // ECE463: parse but not used
   params.PREF_M    = (uint32_t) atoi(argv[7]);  // ECE463: parse but not used
   trace_file       = argv[8];

   // Open trace
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }

   // Print simulator configuration (trace file printed as basename only).
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n\n", basename_c(trace_file));

   // Build cache hierarchy (ECE463: no prefetch logic)
   CacheConfig l1_cfg {
       "L1",
       (std::size_t)params.L1_SIZE,
       (std::size_t)params.L1_ASSOC,
       (std::size_t)params.BLOCKSIZE
   };
   Cache l1(l1_cfg);

   const bool has_l2 = (params.L2_SIZE > 0 && params.L2_ASSOC > 0);
   std::unique_ptr<Cache> l2;
   if (has_l2) {
       CacheConfig l2_cfg {
           "L2",
           (std::size_t)params.L2_SIZE,
           (std::size_t)params.L2_ASSOC,
           (std::size_t)params.BLOCKSIZE
       };
       l2 = std::make_unique<Cache>(l2_cfg);
   }

   // Read requests from the trace.
   // NOTE: Leading space before %c skips whitespace/newlines between lines.
   while (fscanf(fp, " %c %x", &rw, &addr) == 2) {
      Cache::Op op;
      if (rw == 'r' || rw == 'R')      op = Cache::Op::Read;
      else if (rw == 'w' || rw == 'W') op = Cache::Op::Write;
      else {
         printf("Error: Unknown request type %c.\n", rw);
         fclose(fp);
         exit(EXIT_FAILURE);
      }

      l1.access(op, (uint32_t)addr, has_l2 ? l2.get() : nullptr);
   }

   fclose(fp);

   // Final reporting (format aligns with provided validation files)
   AllStats totals;
   totals.l1 = l1.stats();
   if (has_l2) totals.l2 = l2->stats();

   print_final_report(std::cout, l1, has_l2 ? l2.get() : nullptr, totals);
   return 0;
}
