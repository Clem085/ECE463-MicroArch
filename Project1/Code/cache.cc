/***********************************************************************************
 * File:        cache.cc
 * Author:      Connor Savugot
 * Created:     2025-09-07
 * Updated:     2025-09-21
 * Version:     1.1
 *
 * Description: Implements the Cache class with WBWA write policy, LRU replacement,
 *              allocation, and writeback logic. Used for both L1 and L2 caches.
 *              Includes contents printing to match validation formatting.
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

#include "cache.h"

static inline uint32_t ilog2_uint32(uint32_t x) {
    // precondition: x is a power of two
    assert(x && ((x & (x - 1)) == 0));
    uint32_t n = 0;
    while ((1u << n) < x) ++n;
    return n;
}

AccessStats::AccessStats()
: reads(0), read_misses(0), writes(0), write_misses(0),
  writebacks(0), memory_reads(0), memory_writes(0),
  pref_issued(0), pref_useful(0), pref_late(0) {}

Cache::Cache(const CacheConfig& cfg) : cfg_(cfg) {
    compute_geometry_();
    init_storage_();
}

void Cache::compute_geometry_() {
    assert(cfg_.block_bytes && ((cfg_.block_bytes & (cfg_.block_bytes - 1)) == 0));
    assert(cfg_.assoc > 0);
    assert(cfg_.size_bytes > 0);
    assert((cfg_.size_bytes % (cfg_.assoc * cfg_.block_bytes)) == 0);

    sets_     = cfg_.size_bytes / (cfg_.assoc * cfg_.block_bytes);
    off_bits_ = ilog2_uint32(static_cast<uint32_t>(cfg_.block_bytes));
    idx_bits_ = ilog2_uint32(static_cast<uint32_t>(sets_));
    idx_mask_ = (idx_bits_ == 64 ? ~0ULL : ((1ULL << idx_bits_) - 1ULL));
}

void Cache::init_storage_() {
    sets_vec_.assign(sets_, std::vector<Line>(cfg_.assoc));
    for (auto& set : sets_vec_) {
        for (uint32_t w = 0; w < set.size(); ++w) {
            set[w].lru_age = w; // larger -> older
        }
    }
}

uint64_t Cache::index_of(uint32_t addr) const {
    return (addr >> off_bits_) & idx_mask_;
}

uint64_t Cache::tag_of(uint32_t addr) const {
    return static_cast<uint64_t>(addr) >> (off_bits_ + idx_bits_);
}

int Cache::find_way(uint64_t set, uint64_t tag) const {
    const auto& lines = sets_vec_[set];
    for (int w = 0; w < static_cast<int>(lines.size()); ++w) {
        if (lines[w].valid && lines[w].tag == tag) {
            return w;
        }
    }
    return -1;
}

int Cache::choose_victim_way(uint64_t set) {
    const auto& lines = sets_vec_[set];
    int victim = 0;
    uint32_t max_age = 0;
    for (int w = 0; w < static_cast<int>(lines.size()); ++w) {
        if (!lines[w].valid) return w; // free slot preferred
        if (lines[w].lru_age >= max_age) {
            max_age = lines[w].lru_age;
            victim = w;
        }
    }
    return victim;
}

void Cache::touch_as_mru(uint64_t set, int way) {
    // Placeholder LRU update: bump all valid ages; selected line becomes MRU (0).
    auto& lines = sets_vec_[set];
    for (auto& ln : lines) { if (ln.valid) ++ln.lru_age; }
    lines[way].lru_age = 0;
}

void Cache::fill_line(uint64_t set, int way, uint64_t tag, bool dirty) {
    auto& ln = sets_vec_[set][way];
    ln.valid = true;
    ln.dirty = dirty;
    ln.tag   = tag;
    touch_as_mru(set, way);
}

void Cache::writeback_down(uint32_t victim_block_addr, Cache* next_level) {
    if (next_level) {
        next_level->access(Op::Write, victim_block_addr, nullptr);
    } else {
        stats_.memory_writes += 1;
    }
    stats_.writebacks += 1;
}

void Cache::allocate_on_miss(uint32_t addr, Cache* next_level, bool make_dirty) {
    const uint64_t set = index_of(addr);
    const uint64_t tag = tag_of(addr);
    int victim = choose_victim_way(set);

    if (sets_vec_[set][victim].valid) {
        if (sets_vec_[set][victim].dirty) {
            uint32_t victim_block_addr =
                static_cast<uint32_t>(
                    (sets_vec_[set][victim].tag << (idx_bits_ + off_bits_)) |
                    (static_cast<uint32_t>(set) << off_bits_)
                );
            writeback_down(victim_block_addr, next_level);
        }
    }

    if (next_level) {
        next_level->access(Op::Read, block_aligned(addr), nullptr);
    } else {
        stats_.memory_reads += 1;
    }

    fill_line(set, victim, tag, make_dirty);
}

bool Cache::access(Op op, uint32_t addr, Cache* next_level) {
    const uint64_t set = index_of(addr);
    const uint64_t tag = tag_of(addr);

    if (op == Op::Read) stats_.reads += 1;
    else                stats_.writes += 1;

    int way = find_way(set, tag);
    if (way >= 0) {
        if (op == Op::Write) {
            sets_vec_[set][way].dirty = true; // WBWA: write hits mark dirty
        }
        touch_as_mru(set, way);
        return true;
    }

    // Miss
    if (op == Op::Read) stats_.read_misses += 1;
    else                stats_.write_misses += 1;

    // WBWA + write-allocate: allocate on both read and write misses.
    const bool make_dirty = (op == Op::Write);
    allocate_on_miss(addr, next_level, make_dirty);
    return false;
}

void Cache::print_contents(std::ostream& os) const {
    for (std::size_t s = 0; s < sets_; ++s) {
        // Gather valid lines
        std::vector<Line> lines;
        lines.reserve(cfg_.assoc);
        for (const auto& ln : sets_vec_[s]) {
            if (ln.valid) lines.push_back(ln);
        }
        if (lines.empty()) continue;

        // Order MRU -> LRU (lru_age: 0 = MRU)
        std::sort(lines.begin(), lines.end(),
                  [](const Line& a, const Line& b){ return a.lru_age < b.lru_age; });

        // Exact expected format:  set______N:␠␠<tag> [D] ...
        os << "set " << std::setw(6) << s << ":   ";
        bool first = true;
        for (const auto& ln : lines) {
            if (!first) os << " ";
            os << std::hex << ln.tag << (ln.dirty ? " D" : "");
            first = false;
        }
        os << std::dec << "\n";
    }
}
