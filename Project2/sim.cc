#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include "sim_bp.h"

namespace {

std::string build_command_line(int argc, char* argv[]) {
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

std::size_t mask_bits(unsigned bits) {
    if (bits == 0) {
        return 0;
    }
    return (static_cast<std::size_t>(1) << bits) - 1;
}

void increment_counter(int &value) {
    if (value < 3) {
        ++value;
    }
}

void decrement_counter(int &value) {
    if (value > 0) {
        --value;
    }
}

void print_stats(std::uint64_t predictions, std::uint64_t mispredictions) {
    double rate = predictions ? static_cast<double>(mispredictions) / static_cast<double>(predictions) : 0.0;
    std::printf("OUTPUT\n");
    std::printf(" number of predictions:    %llu\n", static_cast<unsigned long long>(predictions));
    std::printf(" number of mispredictions: %llu\n", static_cast<unsigned long long>(mispredictions));
    std::printf(" misprediction rate:       %.2f%%\n", rate * 100.0);
}

void print_table(const char *header, const std::vector<int> &table) {
    std::printf("%s\n", header);
    for (std::size_t i = 0; i < table.size(); ++i) {
        std::printf(" %zu\t%d\n", i, table[i]);
    }
}

std::size_t bimodal_index(std::uint64_t addr, unsigned bits) {
    if (bits == 0) {
        return 0;
    }
    return static_cast<std::size_t>((addr >> 2) & mask_bits(bits));
}

std::size_t gshare_index(std::uint64_t addr, unsigned m_bits, unsigned n_bits, std::uint64_t ghr) {
    if (m_bits == 0) {
        return 0;
    }

    std::size_t table_mask = mask_bits(m_bits);
    std::size_t pc_index = static_cast<std::size_t>((addr >> 2) & table_mask);
    if (n_bits == 0) {
        return pc_index;
    }

    std::size_t upper = pc_index >> (m_bits - n_bits);
    std::size_t lower_mask = (m_bits > n_bits) ? mask_bits(m_bits - n_bits) : 0;
    std::size_t ghr_bits = static_cast<std::size_t>(ghr & mask_bits(n_bits));
    std::size_t xored = upper ^ ghr_bits;
    return (xored << (m_bits - n_bits)) | (pc_index & lower_mask);
}

void update_ghr(std::uint64_t &ghr, unsigned n_bits, bool taken) {
    if (n_bits == 0) {
        return;
    }
    ghr >>= 1;
    if (taken) {
        ghr |= (static_cast<std::uint64_t>(1) << (n_bits - 1));
    }
    ghr &= mask_bits(n_bits);
}

void run_bimodal(const bp_params &params, const char *trace_file, const std::string &command_line) {
    FILE *FP = fopen(trace_file, "r");
    if (FP == NULL) {
        std::printf("Error: Unable to open file %s\n", trace_file);
        std::exit(EXIT_FAILURE);
    }

    std::size_t entries = (params.M2 == 0) ? 1 : (static_cast<std::size_t>(1) << params.M2);
    std::vector<int> table(entries, 2);
    std::uint64_t predictions = 0;
    std::uint64_t mispredictions = 0;

    unsigned long long addr;
    char str[2];
    while (fscanf(FP, "%llx %s", &addr, str) != EOF) {
        bool taken = (str[0] == 't' || str[0] == 'T');
        std::size_t idx = bimodal_index(addr, params.M2);
        bool prediction = table[idx] >= 2;
        ++predictions;
        if (prediction != taken) {
            ++mispredictions;
        }

        if (taken) {
            increment_counter(table[idx]);
        } else {
            decrement_counter(table[idx]);
        }
    }
    fclose(FP);

    std::printf("COMMAND\n%s\n", command_line.c_str());
    print_stats(predictions, mispredictions);
    print_table("FINAL BIMODAL CONTENTS", table);
}

void run_gshare(const bp_params &params, const char *trace_file, const std::string &command_line) {
    FILE *FP = fopen(trace_file, "r");
    if (FP == NULL) {
        std::printf("Error: Unable to open file %s\n", trace_file);
        std::exit(EXIT_FAILURE);
    }

    std::size_t entries = (params.M1 == 0) ? 1 : (static_cast<std::size_t>(1) << params.M1);
    std::vector<int> table(entries, 2);
    std::uint64_t predictions = 0;
    std::uint64_t mispredictions = 0;
    std::uint64_t ghr = 0;

    unsigned long long addr;
    char str[2];
    while (fscanf(FP, "%llx %s", &addr, str) != EOF) {
        bool taken = (str[0] == 't' || str[0] == 'T');
        std::size_t idx = gshare_index(addr, params.M1, params.N, ghr);
        bool prediction = table[idx] >= 2;
        ++predictions;
        if (prediction != taken) {
            ++mispredictions;
        }

        if (taken) {
            increment_counter(table[idx]);
        } else {
            decrement_counter(table[idx]);
        }
        update_ghr(ghr, params.N, taken);
    }
    fclose(FP);

    std::printf("COMMAND\n%s\n", command_line.c_str());
    print_stats(predictions, mispredictions);
    print_table("FINAL GSHARE CONTENTS", table);
}

void run_hybrid(const bp_params &params, const char *trace_file, const std::string &command_line) {
    FILE *FP = fopen(trace_file, "r");
    if (FP == NULL) {
        std::printf("Error: Unable to open file %s\n", trace_file);
        std::exit(EXIT_FAILURE);
    }

    std::size_t chooser_entries = (params.K == 0) ? 1 : (static_cast<std::size_t>(1) << params.K);
    std::size_t gshare_entries = (params.M1 == 0) ? 1 : (static_cast<std::size_t>(1) << params.M1);
    std::size_t bimodal_entries = (params.M2 == 0) ? 1 : (static_cast<std::size_t>(1) << params.M2);

    std::vector<int> chooser_table(chooser_entries, 1);
    std::vector<int> gshare_table(gshare_entries, 2);
    std::vector<int> bimodal_table(bimodal_entries, 2);

    std::uint64_t predictions = 0;
    std::uint64_t mispredictions = 0;
    std::uint64_t ghr = 0;

    unsigned long long addr;
    char str[2];
    while (fscanf(FP, "%llx %s", &addr, str) != EOF) {
        bool taken = (str[0] == 't' || str[0] == 'T');

        std::size_t chooser_idx = (params.K == 0) ? 0 : static_cast<std::size_t>((addr >> 2) & mask_bits(params.K));
        std::size_t g_idx = gshare_index(addr, params.M1, params.N, ghr);
        std::size_t b_idx = bimodal_index(addr, params.M2);

        bool gshare_prediction = gshare_table[g_idx] >= 2;
        bool bimodal_prediction = bimodal_table[b_idx] >= 2;
        bool use_gshare = chooser_table[chooser_idx] >= 2;
        bool final_prediction = use_gshare ? gshare_prediction : bimodal_prediction;

        ++predictions;
        if (final_prediction != taken) {
            ++mispredictions;
        }

        if (use_gshare) {
            if (taken) {
                increment_counter(gshare_table[g_idx]);
            } else {
                decrement_counter(gshare_table[g_idx]);
            }
        } else {
            if (taken) {
                increment_counter(bimodal_table[b_idx]);
            } else {
                decrement_counter(bimodal_table[b_idx]);
            }
        }

        bool gshare_correct = (gshare_prediction == taken);
        bool bimodal_correct = (bimodal_prediction == taken);
        if (gshare_correct && !bimodal_correct) {
            increment_counter(chooser_table[chooser_idx]);
        } else if (bimodal_correct && !gshare_correct) {
            decrement_counter(chooser_table[chooser_idx]);
        }

        update_ghr(ghr, params.N, taken);
    }
    fclose(FP);

    std::printf("COMMAND\n%s\n", command_line.c_str());
    print_stats(predictions, mispredictions);
    print_table("FINAL CHOOSER CONTENTS", chooser_table);
    print_table("FINAL GSHARE CONTENTS", gshare_table);
    print_table("FINAL BIMODAL CONTENTS", bimodal_table);
}

} // namespace

int main (int argc, char* argv[])
{
    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    bp_params params;       // look at sim_bp.h header file for the the definition of struct bp_params
    char outcome;           // Variable holds branch outcome
    unsigned long int addr; // Variable holds the address read from input file
    
    if (!(argc == 4 || argc == 5 || argc == 7))
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    
    params.bp_name  = argv[1];
    
    // strtoul() converts char* to unsigned long. It is included in <stdlib.h>
    if(strcmp(params.bp_name, "bimodal") == 0)              // Bimodal
    {
        if(argc != 4)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M2       = strtoul(argv[2], NULL, 10);
        trace_file      = argv[3];
        printf("COMMAND\n%s %s %lu %s\n", argv[0], params.bp_name, params.M2, trace_file);
        run_bimodal(params, trace_file, build_command_line(argc, argv));
        return 0;
    }
    else if(strcmp(params.bp_name, "gshare") == 0)          // Gshare
    {
        if(argc != 5)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M1       = strtoul(argv[2], NULL, 10);
        params.N        = strtoul(argv[3], NULL, 10);
        trace_file      = argv[4];
        if (params.N > params.M1) {
            printf("Error: gshare requires N <= M1\n");
            exit(EXIT_FAILURE);
        }
        printf("COMMAND\n%s %s %lu %lu %s\n", argv[0], params.bp_name, params.M1, params.N, trace_file);
        run_gshare(params, trace_file, build_command_line(argc, argv));
        return 0;
    }
    else if(strcmp(params.bp_name, "hybrid") == 0)          // Hybrid
    {
        if(argc != 7)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.K        = strtoul(argv[2], NULL, 10);
        params.M1       = strtoul(argv[3], NULL, 10);
        params.N        = strtoul(argv[4], NULL, 10);
        params.M2       = strtoul(argv[5], NULL, 10);
        trace_file      = argv[6];
        if (params.N > params.M1) {
            printf("Error: hybrid requires N <= M1\n");
            exit(EXIT_FAILURE);
        }
        printf("COMMAND\n%s %s %lu %lu %lu %lu %s\n", argv[0], params.bp_name, params.K, params.M1, params.N, params.M2, trace_file);
        run_hybrid(params, trace_file, build_command_line(argc, argv));
        return 0;
    }
    else
    {
        printf("Error: Wrong branch predictor name:%s\n", params.bp_name);
        exit(EXIT_FAILURE);
    }
    
    return 0;
}
