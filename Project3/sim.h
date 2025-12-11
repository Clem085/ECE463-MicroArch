#ifndef SIM_H
#define SIM_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "sim_proc.h"

using ProcParams = proc_params;

enum Stage {
    FE = 0,
    DE,
    RN,
    RR,
    DI,
    IS,
    EX,
    WB,
    RT,
    STAGE_COUNT
};

struct StageInfo {
    long long begin = -1;
    long long duration = 0;
};

struct Operand {
    int reg = -1;
    int tag = -1;      // ROB index of producer, if any
    bool ready = true; // true if value available
};

struct Instruction {
    uint64_t pc = 0;
    int op_type = 0;
    int dest = -1;
    int src1 = -1;
    int src2 = -1;
    int seq_no = 0;
    int rob_index = -1;
    bool retired = false;
    bool rt_started = false;

    Operand src_ops[2];
    StageInfo stages[STAGE_COUNT];
};

class Simulator {
public:
    explicit Simulator(const ProcParams &params, const std::string &trace_file);
    ~Simulator();

    void run();

private:
    struct ROBEntry {
        bool valid = false;
        bool ready = false;
        int dest = -1;
        Instruction *inst = nullptr;
    };

    struct ExecEntry {
        Instruction *inst = nullptr;
        int remaining = 0;
    };

    // Stage helpers
    void retire_stage();
    void writeback_stage();
    void execute_stage();
    void issue_stage();
    void dispatch_stage();
    void regread_stage();
    void rename_stage();
    void decode_stage();
    void fetch_stage();
    void apply_broadcasts(const std::vector<int> &tags);

    // Utility helpers
    bool rob_has_space(std::size_t needed) const;
    int rob_allocate(Instruction *inst);
    void advance_cycle();
    bool pipeline_empty() const;
    void start_stage(Instruction *inst, Stage st, long long begin_cycle);
    int op_latency(int op_type) const;

    ProcParams params_;
    std::string trace_file_;
    FILE *trace_fp_ = nullptr;
    bool trace_depleted_ = false;

    long long cycle_ = 0;
    long long inst_retired_ = 0;
    int next_seq_no_ = 0;

    std::array<int, 67> rmt_{};
    std::vector<ROBEntry> rob_;
    int rob_head_ = 0;
    int rob_tail_ = 0;
    int rob_count_ = 0;

    // Pipeline storage (current)
    std::vector<Instruction *> de_, rn_, rr_, di_;
    std::vector<Instruction *> wb_;
    std::vector<Instruction *> iq_;
    std::vector<ExecEntry> exec_;

    // Next-cycle storage
    std::vector<Instruction *> de_next_, rn_next_, rr_next_, di_next_;
    std::vector<Instruction *> wb_next_;
    std::vector<Instruction *> iq_next_;
    std::vector<ExecEntry> exec_next_;

    std::vector<int> broadcast_tags_;
    std::vector<Instruction *> all_insts_;
};

#endif // SIM_H
