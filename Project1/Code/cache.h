#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <ostream>


struct CacheConfig {
    std::string name;          
    std::size_t size_bytes;    
    std::size_t assoc;         
    std::size_t block_bytes;    
};

struct AccessStats {
    // Initialize all to 0 in constructor.
    uint64_t reads;
    uint64_t read_misses;
    uint64_t writes;
    uint64_t write_misses;
    uint64_t writebacks;       
    uint64_t memory_reads;     
    uint64_t memory_writes;    

  
    uint64_t pref_issued;
    uint64_t pref_useful;
    uint64_t pref_late;

    AccessStats();
};

class Cache {
public:
    enum class Op { Read, Write };

    Cache(const CacheConfig& cfg);

    bool access(Op op, uint32_t addr, Cache* next_level);

    void print_contents(std::ostream& os) const;
    const AccessStats& stats() const { return stats_; }
    const CacheConfig& config() const { return cfg_; }
    void reset();

private:
    struct Line {
        bool     valid = false;
        bool     dirty = false;
        uint64_t tag   = 0;
        uint32_t lru_age = 0;
    };

    CacheConfig cfg_;
    AccessStats stats_;

    std::size_t sets_        = 0;
    uint32_t    off_bits_    = 0; 
    uint32_t    idx_bits_    = 0; 
    uint64_t    idx_mask_    = 0; 

    std::vector<std::vector<Line>> sets_vec_;

private:
    // ---- Address helpers ----
    uint32_t offset_bits() const { return off_bits_; }
    uint32_t index_bits()  const { return idx_bits_; }
    uint64_t index_of(uint32_t addr) const;  
    uint64_t tag_of(uint32_t addr)   const; 

    // ---- Core operations you will implement ----
    int  find_way(uint64_t set, uint64_t tag) const;    
    int  choose_victim_way(uint64_t set);                

    void touch_as_mru(uint64_t set, int way);            
    void fill_line(uint64_t set, int way, uint64_t tag, bool dirty);

    // Miss path: allocate, handle eviction (WB if dirty).
    void allocate_on_miss(uint32_t addr, Cache* next_level, bool make_dirty);

    // Push a dirty victim to next level or to memory if next_level == nullptr.
    void writeback_down(uint32_t victim_block_addr, Cache* next_level);

    uint32_t block_aligned(uint32_t addr) const { return addr & ~((uint32_t)cfg_.block_bytes - 1U); }

    void compute_geometry_();
    void init_storage_();
};

#endif  // CACHE_H
