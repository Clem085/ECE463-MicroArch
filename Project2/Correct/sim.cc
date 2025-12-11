#include "sim.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint8_t INIT_COUNTER = 2U;   // weakly taken
constexpr std::uint8_t INIT_CHOOSER = 1U;   // slight bias towards gshare

bool str_to_uint(const char *s, unsigned &value) {
    if (!s || *s == '\0') {
        return false;
    }
    char *endptr = nullptr;
    unsigned long parsed = std::strtoul(s, &endptr, 10);
    if (endptr == s || *endptr != '\0') {
        return false;
    }
    value = static_cast<unsigned>(parsed);
    return true;
}

bool is_taken(char outcome) {
    return outcome == 't' || outcome == 'T';
}

bool is_not_taken(char outcome) {
    return outcome == 'n' || outcome == 'N';
}

void print_stats(std::uint64_t predictions, std::uint64_t mispredictions) {
    double rate = predictions ? static_cast<double>(mispredictions) / static_cast<double>(predictions) : 0.0;
    std::printf("OUTPUT\n");
    std::printf(" number of predictions:    %llu\n", static_cast<unsigned long long>(predictions));
    std::printf(" number of mispredictions: %llu\n", static_cast<unsigned long long>(mispredictions));
    std::printf(" misprediction rate:       %.2f%%\n", rate * 100.0);
}

template <typename Container>
void print_table(const char *header, const Container &table) {
    std::printf("%s\n", header);
    for (std::size_t i = 0; i < table.size(); ++i) {
        std::printf(" %zu\t%u\n", i, static_cast<unsigned>(table[i]));
    }
}

} // namespace

BimodalPredictor::BimodalPredictor(unsigned m_bits)
    : m_bits_(m_bits),
      mask_((m_bits == 0) ? 0 : ((static_cast<std::size_t>(1) << m_bits) - 1)),
      counters_((m_bits == 0) ? 1 : (static_cast<std::size_t>(1) << m_bits), INIT_COUNTER) {
    if (m_bits_ > (sizeof(std::size_t) * 8 - 1)) {
        throw std::invalid_argument("m_bits too large for bimodal predictor");
    }
}

TableLookup BimodalPredictor::predict(std::uint64_t pc) const {
    std::size_t idx = index(pc);
    bool pred = counters_[idx] >= 2;
    return {pred, idx};
}

void BimodalPredictor::update(const TableLookup &info, bool taken) {
    if (taken) {
        if (counters_[info.index] < 3) {
            ++counters_[info.index];
        }
    } else {
        if (counters_[info.index] > 0) {
            --counters_[info.index];
        }
    }
}

std::size_t BimodalPredictor::index(std::uint64_t pc) const {
    if (m_bits_ == 0) {
        return 0;
    }
    return static_cast<std::size_t>((pc >> 2) & mask_);
}

GsharePredictor::GsharePredictor(unsigned m_bits, unsigned n_bits)
    : m_bits_(m_bits),
      n_bits_(n_bits),
      table_mask_((m_bits == 0) ? 0 : ((static_cast<std::size_t>(1) << m_bits) - 1)),
      lower_mask_((m_bits > n_bits) ? ((static_cast<std::size_t>(1) << (m_bits - n_bits)) - 1) : 0),
      history_mask_((n_bits == 0) ? 0 : ((static_cast<std::uint64_t>(1) << n_bits) - 1)),
      counters_((m_bits == 0) ? 1 : (static_cast<std::size_t>(1) << m_bits), INIT_COUNTER),
      ghr_(0) {
    if (n_bits_ > m_bits_) {
        throw std::invalid_argument("gshare requires n_bits <= m_bits");
    }
    if (m_bits_ > (sizeof(std::size_t) * 8 - 1)) {
        throw std::invalid_argument("m_bits too large for gshare predictor");
    }
}

TableLookup GsharePredictor::predict(std::uint64_t pc) const {
    std::size_t idx = index(pc);
    bool pred = counters_[idx] >= 2;
    return {pred, idx};
}

void GsharePredictor::update(const TableLookup &info, bool taken, bool update_counter) {
    if (update_counter) {
        if (taken) {
            increment_counter(info.index);
        } else {
            decrement_counter(info.index);
        }
    }

    if (n_bits_ > 0) {
        ghr_ >>= 1;
        if (taken) {
            ghr_ |= (static_cast<std::uint64_t>(1) << (n_bits_ - 1));
        }
        ghr_ &= history_mask_;
    }
}

std::size_t GsharePredictor::index(std::uint64_t pc) const {
    if (m_bits_ == 0) {
        return 0;
    }

    std::size_t pc_index = static_cast<std::size_t>((pc >> 2) & table_mask_);
    if (n_bits_ == 0) {
        return pc_index;
    }

    std::size_t ghr_masked = static_cast<std::size_t>(ghr_ & history_mask_);
    std::size_t upper_bits = pc_index >> (m_bits_ - n_bits_);
    std::size_t xored = upper_bits ^ ghr_masked;
    std::size_t recombined = (xored << (m_bits_ - n_bits_)) | (pc_index & lower_mask_);
    return recombined;
}

void GsharePredictor::increment_counter(std::size_t idx) {
    if (counters_[idx] < 3) {
        ++counters_[idx];
    }
}

void GsharePredictor::decrement_counter(std::size_t idx) {
    if (counters_[idx] > 0) {
        --counters_[idx];
    }
}

HybridPredictor::HybridPredictor(unsigned k_bits, unsigned m1_bits, unsigned n_bits, unsigned m2_bits)
    : k_bits_(k_bits),
      chooser_mask_((k_bits == 0) ? 0 : ((static_cast<std::size_t>(1) << k_bits) - 1)),
      chooser_counters_((k_bits == 0) ? 1 : (static_cast<std::size_t>(1) << k_bits), INIT_CHOOSER),
      gshare_(m1_bits, n_bits),
      bimodal_(m2_bits) {
    if (k_bits_ > (sizeof(std::size_t) * 8 - 1)) {
        throw std::invalid_argument("k_bits too large for hybrid predictor");
    }
}

HybridPredictor::HybridInfo HybridPredictor::predict(std::uint64_t pc) {
    HybridInfo info;
    info.gshare_info = gshare_.predict(pc);
    info.bimodal_info = bimodal_.predict(pc);
    info.chooser_index = chooser_index(pc);
    info.use_gshare = chooser_counters_[info.chooser_index] >= 2;
    info.overall_prediction = info.use_gshare ? info.gshare_info.predicted_taken : info.bimodal_info.predicted_taken;
    return info;
}

void HybridPredictor::update(const HybridInfo &info, bool taken) {
    bool gshare_correct = (info.gshare_info.predicted_taken == taken);
    bool bimodal_correct = (info.bimodal_info.predicted_taken == taken);

    if (info.use_gshare) {
        gshare_.update(info.gshare_info, taken, true);
    } else {
        bimodal_.update(info.bimodal_info, taken);
        gshare_.update(info.gshare_info, taken, false);
    }

    if (gshare_correct && !bimodal_correct) {
        increment_chooser(info.chooser_index);
    } else if (bimodal_correct && !gshare_correct) {
        decrement_chooser(info.chooser_index);
    }
}

std::size_t HybridPredictor::chooser_index(std::uint64_t pc) const {
    if (k_bits_ == 0) {
        return 0;
    }
    return static_cast<std::size_t>((pc >> 2) & chooser_mask_);
}

void HybridPredictor::increment_chooser(std::size_t idx) {
    if (chooser_counters_[idx] < 3) {
        ++chooser_counters_[idx];
    }
}

void HybridPredictor::decrement_chooser(std::size_t idx) {
    if (chooser_counters_[idx] > 0) {
        --chooser_counters_[idx];
    }
}

std::string build_command_line(int argc, char *argv[]) {
    std::ostringstream oss;
    oss << " ";
    for (int i = 0; i < argc; ++i) {
        if (i > 0) {
            oss << ' ';
        }
        oss << argv[i];
    }
    return oss.str();
}

void run_bimodal(const SimulationConfig &config, const std::string &command_line) {
    std::ifstream trace(config.trace_file);
    if (!trace) {
        std::fprintf(stderr, "Error: Unable to open file %s\n", config.trace_file.c_str());
        std::exit(EXIT_FAILURE);
    }

    BimodalPredictor predictor(config.m2);
    std::uint64_t predictions = 0;
    std::uint64_t misses = 0;

    std::string addr_token;
    char outcome = '\0';
    while (trace >> addr_token >> outcome) {
        std::uint64_t pc = std::stoull(addr_token, nullptr, 16);
        if (!is_taken(outcome) && !is_not_taken(outcome)) {
            std::fprintf(stderr, "Error: Invalid outcome '%c'\n", outcome);
            std::exit(EXIT_FAILURE);
        }
        bool taken = is_taken(outcome);
        TableLookup info = predictor.predict(pc);
        ++predictions;
        if (info.predicted_taken != taken) {
            ++misses;
        }
        predictor.update(info, taken);
    }

    std::printf("COMMAND\n%s\n", command_line.c_str());
    print_stats(predictions, misses);
    print_table("FINAL BIMODAL CONTENTS", predictor.table());
}

void run_gshare(const SimulationConfig &config, const std::string &command_line) {
    std::ifstream trace(config.trace_file);
    if (!trace) {
        std::fprintf(stderr, "Error: Unable to open file %s\n", config.trace_file.c_str());
        std::exit(EXIT_FAILURE);
    }

    GsharePredictor predictor(config.m1, config.n);
    std::uint64_t predictions = 0;
    std::uint64_t misses = 0;

    std::string addr_token;
    char outcome = '\0';
    while (trace >> addr_token >> outcome) {
        std::uint64_t pc = std::stoull(addr_token, nullptr, 16);
        if (!is_taken(outcome) && !is_not_taken(outcome)) {
            std::fprintf(stderr, "Error: Invalid outcome '%c'\n", outcome);
            std::exit(EXIT_FAILURE);
        }
        bool taken = is_taken(outcome);
        TableLookup info = predictor.predict(pc);
        ++predictions;
        if (info.predicted_taken != taken) {
            ++misses;
        }
        predictor.update(info, taken, true);
    }

    std::printf("COMMAND\n%s\n", command_line.c_str());
    print_stats(predictions, misses);
    print_table("FINAL GSHARE CONTENTS", predictor.table());
}

void run_hybrid(const SimulationConfig &config, const std::string &command_line) {
    std::ifstream trace(config.trace_file);
    if (!trace) {
        std::fprintf(stderr, "Error: Unable to open file %s\n", config.trace_file.c_str());
        std::exit(EXIT_FAILURE);
    }

    HybridPredictor predictor(config.k, config.m1, config.n, config.m2);
    std::uint64_t predictions = 0;
    std::uint64_t misses = 0;

    std::string addr_token;
    char outcome = '\0';
    while (trace >> addr_token >> outcome) {
        std::uint64_t pc = std::stoull(addr_token, nullptr, 16);
        if (!is_taken(outcome) && !is_not_taken(outcome)) {
            std::fprintf(stderr, "Error: Invalid outcome '%c'\n", outcome);
            std::exit(EXIT_FAILURE);
        }
        bool taken = is_taken(outcome);
        HybridPredictor::HybridInfo info = predictor.predict(pc);
        ++predictions;
        if (info.overall_prediction != taken) {
            ++misses;
        }
        predictor.update(info, taken);
    }

    std::printf("COMMAND\n%s\n", command_line.c_str());
    print_stats(predictions, misses);
    print_table("FINAL CHOOSER CONTENTS", predictor.chooser_table());
    print_table("FINAL GSHARE CONTENTS", predictor.gshare().table());
    print_table("FINAL BIMODAL CONTENTS", predictor.bimodal().table());
}

void run_simulation(const SimulationConfig &config, const std::string &command_line) {
    switch (config.mode) {
        case SimulationConfig::Mode::Bimodal:
            run_bimodal(config, command_line);
            break;
        case SimulationConfig::Mode::Gshare:
            run_gshare(config, command_line);
            break;
        case SimulationConfig::Mode::Hybrid:
            run_hybrid(config, command_line);
            break;
    }
}

static SimulationConfig parse_arguments(int argc, char *argv[]) {
    if (!(argc == 4 || argc == 5 || argc == 7)) {
        std::fprintf(stderr, "Error: Wrong number of inputs:%d\n", argc - 1);
        std::exit(EXIT_FAILURE);
    }

    SimulationConfig config{};
    std::string mode = argv[1];

    if (mode == "bimodal") {
        if (argc != 4) {
            std::fprintf(stderr, "Error: %s wrong number of inputs:%d\n", mode.c_str(), argc - 1);
            std::exit(EXIT_FAILURE);
        }
        unsigned m2 = 0;
        if (!str_to_uint(argv[2], m2)) {
            std::fprintf(stderr, "Error: invalid M2 value\n");
            std::exit(EXIT_FAILURE);
        }
        config.mode = SimulationConfig::Mode::Bimodal;
        config.m2 = m2;
        config.trace_file = argv[3];
    } else if (mode == "gshare") {
        if (argc != 5) {
            std::fprintf(stderr, "Error: %s wrong number of inputs:%d\n", mode.c_str(), argc - 1);
            std::exit(EXIT_FAILURE);
        }

        unsigned m1 = 0;
        unsigned n = 0;
        if (!str_to_uint(argv[2], m1) || !str_to_uint(argv[3], n)) {
            std::fprintf(stderr, "Error: invalid M1 or N value\n");
            std::exit(EXIT_FAILURE);
        }
        if (n > m1) {
            std::fprintf(stderr, "Error: gshare requires N <= M1\n");
            std::exit(EXIT_FAILURE);
        }
        config.mode = SimulationConfig::Mode::Gshare;
        config.m1 = m1;
        config.n = n;
        config.trace_file = argv[4];
    } else if (mode == "hybrid") {
        if (argc != 7) {
            std::fprintf(stderr, "Error: %s wrong number of inputs:%d\n", mode.c_str(), argc - 1);
            std::exit(EXIT_FAILURE);
        }
        unsigned k = 0, m1 = 0, n = 0, m2 = 0;
        if (!str_to_uint(argv[2], k) || !str_to_uint(argv[3], m1) || !str_to_uint(argv[4], n) || !str_to_uint(argv[5], m2)) {
            std::fprintf(stderr, "Error: invalid hybrid parameter\n");
            std::exit(EXIT_FAILURE);
        }
        if (n > m1) {
            std::fprintf(stderr, "Error: hybrid requires N <= M1\n");
            std::exit(EXIT_FAILURE);
        }
        config.mode = SimulationConfig::Mode::Hybrid;
        config.k = k;
        config.m1 = m1;
        config.n = n;
        config.m2 = m2;
        config.trace_file = argv[6];
    } else {
        std::fprintf(stderr, "Error: Wrong branch predictor name:%s\n", argv[1]);
        std::exit(EXIT_FAILURE);
    }

    return config;
}

int main(int argc, char *argv[]) {
    SimulationConfig config = parse_arguments(argc, argv);
    std::string command_line = build_command_line(argc, argv);
    run_simulation(config, command_line);
    return 0;
}
