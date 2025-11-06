#ifndef PROJECT2_SIM_H
#define PROJECT2_SIM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct TableLookup {
    bool predicted_taken = false;
    std::size_t index = 0;
};

class BimodalPredictor {
public:
    explicit BimodalPredictor(unsigned m_bits);

    TableLookup predict(std::uint64_t pc) const;
    void update(const TableLookup &info, bool taken);

    const std::vector<std::uint8_t> &table() const { return counters_; }

private:
    std::size_t index(std::uint64_t pc) const;

    unsigned m_bits_;
    std::size_t mask_;
    std::vector<std::uint8_t> counters_;
};

class GsharePredictor {
public:
    GsharePredictor(unsigned m_bits, unsigned n_bits);

    TableLookup predict(std::uint64_t pc) const;
    void update(const TableLookup &info, bool taken, bool update_counter);

    const std::vector<std::uint8_t> &table() const { return counters_; }

private:
    std::size_t index(std::uint64_t pc) const;
    void increment_counter(std::size_t idx);
    void decrement_counter(std::size_t idx);

    unsigned m_bits_;
    unsigned n_bits_;
    std::size_t table_mask_;
    std::size_t lower_mask_;
    std::uint64_t history_mask_;
    std::vector<std::uint8_t> counters_;
    std::uint64_t ghr_;
};

class HybridPredictor {
public:
    HybridPredictor(unsigned k_bits, unsigned m1_bits, unsigned n_bits, unsigned m2_bits);

    struct HybridInfo {
        TableLookup gshare_info;
        TableLookup bimodal_info;
        std::size_t chooser_index = 0;
        bool use_gshare = false;
        bool overall_prediction = false;
    };

    HybridInfo predict(std::uint64_t pc);
    void update(const HybridInfo &info, bool taken);

    const std::vector<std::uint8_t> &chooser_table() const { return chooser_counters_; }
    const GsharePredictor &gshare() const { return gshare_; }
    const BimodalPredictor &bimodal() const { return bimodal_; }

private:
    std::size_t chooser_index(std::uint64_t pc) const;
    void increment_chooser(std::size_t idx);
    void decrement_chooser(std::size_t idx);

    unsigned k_bits_;
    std::size_t chooser_mask_;
    std::vector<std::uint8_t> chooser_counters_;
    GsharePredictor gshare_;
    BimodalPredictor bimodal_;
};

struct SimulationConfig {
    enum class Mode {
        Bimodal,
        Gshare,
        Hybrid
    };

    Mode mode;
    unsigned k = 0;
    unsigned m1 = 0;
    unsigned m2 = 0;
    unsigned n = 0;
    std::string trace_file;
};

std::string build_command_line(int argc, char *argv[]);
void run_simulation(const SimulationConfig &config, const std::string &command_line);

#endif // PROJECT2_SIM_H
