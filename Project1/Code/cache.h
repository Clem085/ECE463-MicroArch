#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <ostream>

// ECE463: Implement a generic set-associative cache with LRU and WBWA.
// Use this same class for L1 and L2 by passing different params.

struct CacheConfig {
    std::string name;           // "L1" or "L2" for printing
    std::size_t size_bytes;     // total capacity
    std::size_t assoc;          // ways
    std::size_t block_bytes;    // line size
};

struct AccessStats {
    // Required stats (fill according to your spec’s output list).
    // Initialize all to 0 in constructor.
    uint64_t reads;
    uint64_t read_misses;
    uint64_t writes;
    uint64_t write_misses;
    uint64_t writebacks;        // evicting dirty lines downwards
    uint64_t memory_reads;      // demand fills that go to "memory"
    uint64_t memory_writes;     // writebacks that reach "memory"

    // For ECE463, prefetch counters should remain zero.
    uint64_t pref_issued;
    uint64_t pref_useful;
    uint64_t pref_late;

    AccessStats();
};

class Cache {
public:
    enum class Op { Read, Write };

    Cache(const CacheConfig& cfg);

    // Top-level API: access 'addr'. If next_level != nullptr, forward misses to it.
    // Return true on hit in THIS level; false if miss (even if served by lower level).
    bool access(Op op, uint32_t addr, Cache* next_level);

    // Print per-set contents in MRU->LRU order as your spec requires.
    void print_contents(std::ostream& os) const;

    // Expose stats for final report.
    const AccessStats& stats() const { return stats_; }
    const CacheConfig& config() const { return cfg_; }

    // Clear/initialize all state (optional utility when testing).
    void reset();

private:
    struct Line {
        bool     valid = false;
        bool     dirty = false;
        uint64_t tag   = 0;
        // Implement LRU however you like (counter, age, stack position).
        // Here we store an integer "age" where smaller == more recent, for example.
        uint32_t lru_age = 0;
    };

    CacheConfig cfg_;
    AccessStats stats_;

    // Derived geometry
    std::size_t sets_        = 0; // number of indexable sets
    uint32_t    off_bits_    = 0; // log2(block_bytes)
    uint32_t    idx_bits_    = 0; // log2(sets)
    uint64_t    idx_mask_    = 0; // mask for index

    // sets_[set_index][way]
    std::vector<std::vector<Line>> sets_vec_;

private:
    // ---- Address helpers ----
    uint32_t offset_bits() const { return off_bits_; }
    uint32_t index_bits()  const { return idx_bits_; }
    uint64_t index_of(uint32_t addr) const;  // (addr >> off_bits_) & idx_mask_
    uint64_t tag_of(uint32_t addr)   const;  // addr >> (off_bits_ + idx_bits_)

    // ---- Core operations you will implement ----
    int  find_way(uint64_t set, uint64_t tag) const;     // return way or -1
    int  choose_victim_way(uint64_t set);                // LRU selection

    void touch_as_mru(uint64_t set, int way);            // update LRU metadata
    void fill_line(uint64_t set, int way, uint64_t tag, bool dirty);

    // Miss path: allocate, handle eviction (writeback if dirty), and interact with next level.
    void allocate_on_miss(uint32_t addr, Cache* next_level, bool make_dirty);

    // Push a dirty victim to next level or to memory if next_level == nullptr.
    void writeback_down(uint32_t victim_block_addr, Cache* next_level);

    // Turn a block-aligned address (addr with offset=0) into the exact same form at lower level.
    uint32_t block_aligned(uint32_t addr) const { return addr & ~((uint32_t)cfg_.block_bytes - 1U); }

    // Helpers to compute geometry and initialize sets.
    void compute_geometry_();
    void init_storage_();
};

#endif // CACHE_H
