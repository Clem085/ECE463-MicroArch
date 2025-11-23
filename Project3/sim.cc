#include "sim.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

Simulator::Simulator(const ProcParams &params) : params_(params) {
    trace_fp_ = fopen(params_.trace_file.c_str(), "r");
    if (!trace_fp_) {
        std::fprintf(stderr, "Error: Unable to open file %s\n", params_.trace_file.c_str());
        std::exit(EXIT_FAILURE);
    }

    rmt_.fill(-1);
    rob_.resize(params_.rob_size);
}

Simulator::~Simulator() {
    if (trace_fp_) {
        fclose(trace_fp_);
    }
}

int Simulator::op_latency(int op_type) const {
    switch (op_type) {
    case 0:
        return 1;
    case 1:
        return 2;
    case 2:
    default:
        return 5;
    }
}

void Simulator::start_stage(Instruction *inst, Stage st, long long begin_cycle) {
    if (inst->stages[st].begin == -1) {
        inst->stages[st].begin = begin_cycle;
        inst->stages[st].duration = 0;
    }
}

bool Simulator::rob_has_space(std::size_t needed) const {
    return needed <= (params_.rob_size - static_cast<std::size_t>(rob_count_));
}

int Simulator::rob_allocate(Instruction *inst) {
    int idx = rob_tail_;
    rob_[idx].valid = true;
    rob_[idx].ready = false;
    rob_[idx].dest = inst->dest;
    rob_[idx].inst = inst;

    rob_tail_ = (rob_tail_ + 1) % static_cast<int>(params_.rob_size);
    ++rob_count_;
    return idx;
}

void Simulator::retire_stage() {
    // Instructions that have entered RT stage accumulate duration each cycle until retirement.
    int idx = rob_head_;
    for (int i = 0; i < rob_count_; ++i) {
        ROBEntry &entry = rob_[idx];
        if (entry.valid && entry.inst->rt_started && !entry.inst->retired) {
            entry.inst->stages[RT].duration++;
        }
        idx = (idx + 1) % static_cast<int>(params_.rob_size);
    }

    int retired_this_cycle = 0;
    while (retired_this_cycle < static_cast<int>(params_.width) && rob_count_ > 0) {
        ROBEntry &head = rob_[rob_head_];
        if (!head.valid || !head.ready) {
            break;
        }

        Instruction *inst = head.inst;
        inst->retired = true;
        ++inst_retired_;

        if (inst->dest != -1 && rmt_[inst->dest] == rob_head_) {
            rmt_[inst->dest] = -1;
        }

        head.valid = false;
        head.ready = false;
        head.inst = nullptr;
        rob_head_ = (rob_head_ + 1) % static_cast<int>(params_.rob_size);
        --rob_count_;
        ++retired_this_cycle;
    }
}

void Simulator::writeback_stage() {
    for (Instruction *inst : wb_) {
        inst->stages[WB].duration++;
        rob_[inst->rob_index].ready = true;

        if (!inst->rt_started) {
            inst->rt_started = true;
            start_stage(inst, RT, cycle_ + 1);
        }
    }
    wb_.clear();
}

void Simulator::execute_stage() {
    broadcast_tags_.clear();

    // Gather finishing instructions first so we can broadcast to current-cycle consumers.
    std::vector<Instruction *> finishing;
    std::vector<int> wake_tags_now;

    for (auto &entry : exec_) {
        Instruction *inst = entry.inst;
        inst->stages[EX].duration++;

        if (entry.remaining == 1) {
            wake_tags_now.push_back(inst->rob_index);
        }

        entry.remaining--;

        if (entry.remaining == 0) {
            finishing.push_back(inst);
            start_stage(inst, WB, cycle_ + 1);
            wb_next_.push_back(inst);
            broadcast_tags_.push_back(inst->rob_index);
        } else {
            exec_next_.push_back({inst, entry.remaining});
        }
    }

    // Wakeup dependents in current-cycle structures so Issue can observe the readiness.
    auto apply_now = [&](std::vector<Instruction *> &list) {
        for (Instruction *inst : list) {
            for (int i = 0; i < 2; ++i) {
                if (!inst->src_ops[i].ready && inst->src_ops[i].tag != -1) {
                    for (int tag : wake_tags_now) {
                        if (inst->src_ops[i].tag == tag) {
                            inst->src_ops[i].ready = true;
                            break;
                        }
                    }
                }
            }
        }
    };

    apply_now(rr_);
    apply_now(di_);
    apply_now(iq_);

#ifdef DEBUG_SIM
    if (!wake_tags_now.empty()) {
        std::fprintf(stderr, "[cycle %lld] wake tags:", cycle_);
        for (int t : wake_tags_now) {
            std::fprintf(stderr, " %d", t);
        }
        std::fprintf(stderr, "\n");
    }
#endif

    exec_.clear();
}

void Simulator::issue_stage() {
    // Increment IS duration for all IQ residents.
    for (Instruction *inst : iq_) {
        inst->stages[IS].duration++;
    }

    // Pick up to WIDTH oldest ready instructions.
    std::vector<Instruction *> ready_list;
    for (Instruction *inst : iq_) {
        if (inst->src_ops[0].ready && inst->src_ops[1].ready) {
            ready_list.push_back(inst);
        }
    }
    std::sort(ready_list.begin(), ready_list.end(),
              [](Instruction *a, Instruction *b) { return a->seq_no < b->seq_no; });

    int can_issue = static_cast<int>(params_.width);
    std::vector<int> issued_indices;
    for (Instruction *inst : ready_list) {
        if (can_issue == 0) {
            break;
        }
        // record index in iq_ to skip when building iq_next_
        for (std::size_t i = 0; i < iq_.size(); ++i) {
            if (iq_[i] == inst) {
                issued_indices.push_back(static_cast<int>(i));
                break;
            }
        }

        int lat = op_latency(inst->op_type);
        start_stage(inst, EX, cycle_ + 1);
        exec_next_.push_back({inst, lat});
        --can_issue;

#ifdef DEBUG_SIM
        std::fprintf(stderr, "[cycle %lld] issue seq %d (lat %d)\n", cycle_, inst->seq_no, lat);
#endif
    }

    // Build next IQ without issued instructions.
    std::sort(issued_indices.begin(), issued_indices.end());
    std::size_t skip_idx = 0;
    for (std::size_t i = 0; i < iq_.size(); ++i) {
        if (skip_idx < issued_indices.size() && static_cast<int>(i) == issued_indices[skip_idx]) {
            ++skip_idx;
            continue;
        }
        iq_next_.push_back(iq_[i]);
    }
    iq_.clear();
}

void Simulator::dispatch_stage() {
    if (di_.empty()) {
        return;
    }

    // All DI residents spend this cycle in DI.
    for (Instruction *inst : di_) {
        inst->stages[DI].duration++;
    }

    std::size_t free_iq = params_.iq_size > iq_next_.size() ? params_.iq_size - iq_next_.size() : 0;
    if (free_iq >= di_.size()) {
        for (Instruction *inst : di_) {
            start_stage(inst, IS, cycle_ + 1);
            iq_next_.push_back(inst);
        }
        di_.clear();
    } else {
        // Stall entire dispatch bundle
        di_next_ = di_;
        di_.clear();
    }
}

void Simulator::regread_stage() {
    if (rr_.empty()) {
        return;
    }

    for (Instruction *inst : rr_) {
        inst->stages[RR].duration++;
    }

    if (!di_.empty() || !di_next_.empty()) {
        rr_next_ = rr_;
        rr_.clear();
        return;
    }

    for (Instruction *inst : rr_) {
        start_stage(inst, DI, cycle_ + 1);
        di_next_.push_back(inst);
    }
    rr_.clear();
}

void Simulator::rename_stage() {
    if (rn_.empty()) {
        return;
    }

    for (Instruction *inst : rn_) {
        inst->stages[RN].duration++;
    }

    if (!rr_.empty() || !rr_next_.empty() || !rob_has_space(rn_.size())) {
        rn_next_ = rn_;
        rn_.clear();
        return;
    }

    for (Instruction *inst : rn_) {
        // Allocate ROB entry
        int rob_idx = rob_allocate(inst);
        inst->rob_index = rob_idx;

        // Source operands
        int srcs[2] = {inst->src1, inst->src2};
        for (int i = 0; i < 2; ++i) {
            inst->src_ops[i].reg = srcs[i];
            inst->src_ops[i].tag = -1;
            inst->src_ops[i].ready = true;
            if (srcs[i] != -1) {
                int tag = rmt_[srcs[i]];
                if (tag != -1) {
                    inst->src_ops[i].tag = tag;
                    inst->src_ops[i].ready = rob_[tag].ready;
                }
            }
        }

        // Destination register mapping
        if (inst->dest != -1) {
            rmt_[inst->dest] = rob_idx;
        }

        start_stage(inst, RR, cycle_ + 1);
        rr_next_.push_back(inst);
    }
    rn_.clear();
}

void Simulator::decode_stage() {
    if (de_.empty()) {
        return;
    }

    for (Instruction *inst : de_) {
        inst->stages[DE].duration++;
    }

    if (!rn_.empty() || !rn_next_.empty()) {
        de_next_ = de_;
        de_.clear();
        return;
    }

    for (Instruction *inst : de_) {
        start_stage(inst, RN, cycle_ + 1);
        rn_next_.push_back(inst);
    }
    de_.clear();
}

void Simulator::fetch_stage() {
    if (!de_.empty() || !de_next_.empty()) {
        return;
    }

    if (trace_depleted_) {
        return;
    }

    for (std::size_t i = 0; i < params_.width; ++i) {
        uint64_t pc = 0;
        int op = 0, dest = -1, s1 = -1, s2 = -1;
        int ret = std::fscanf(trace_fp_, "%lx %d %d %d %d", &pc, &op, &dest, &s1, &s2);
        if (ret == EOF || ret == 0) {
            trace_depleted_ = true;
            break;
        }

        Instruction *inst = new Instruction();
        inst->pc = pc;
        inst->op_type = op;
        inst->dest = dest;
        inst->src1 = s1;
        inst->src2 = s2;
        inst->seq_no = next_seq_no_++;
        inst->rob_index = -1;
        inst->rt_started = false;
        inst->retired = false;
        inst->src_ops[0].ready = true;
        inst->src_ops[1].ready = true;

        start_stage(inst, FE, cycle_);
        inst->stages[FE].duration = 1; // fetch always 1 cycle
        start_stage(inst, DE, cycle_ + 1);

        all_insts_.push_back(inst);
        de_next_.push_back(inst);
    }
}

void Simulator::apply_broadcasts(const std::vector<int> &tags) {
    auto apply_list = [&tags](std::vector<Instruction *> &list) {
        for (Instruction *inst : list) {
            for (int i = 0; i < 2; ++i) {
                if (!inst->src_ops[i].ready && inst->src_ops[i].tag != -1) {
                    for (int tag : tags) {
                        if (inst->src_ops[i].tag == tag) {
                            inst->src_ops[i].ready = true;
                            break;
                        }
                    }
                }
            }
        }
    };

    apply_list(rr_next_);
    apply_list(di_next_);
    apply_list(iq_next_);
}

bool Simulator::pipeline_empty() const {
    return de_.empty() && rn_.empty() && rr_.empty() && di_.empty() && iq_.empty() && exec_.empty() &&
           wb_.empty() && rob_count_ == 0;
}

void Simulator::advance_cycle() {
    de_ = std::move(de_next_);
    rn_ = std::move(rn_next_);
    rr_ = std::move(rr_next_);
    di_ = std::move(di_next_);
    iq_ = std::move(iq_next_);
    exec_ = std::move(exec_next_);
    wb_ = std::move(wb_next_);

    de_next_.clear();
    rn_next_.clear();
    rr_next_.clear();
    di_next_.clear();
    iq_next_.clear();
    exec_next_.clear();
    wb_next_.clear();

    ++cycle_;
}

void Simulator::run() {
    // Prime initial empty state
    while (true) {
#ifdef DEBUG_SIM
        if (cycle_ % 1000 == 0) {
            std::fprintf(stderr, "[cycle %lld] rob=%d iq=%zu exec=%zu wb=%zu de=%zu rn=%zu rr=%zu di=%zu trace_done=%d\n",
                         cycle_, rob_count_, iq_.size(), exec_.size(), wb_.size(), de_.size(), rn_.size(), rr_.size(),
                         di_.size(), trace_depleted_ ? 1 : 0);
        }
#endif
        retire_stage();
        writeback_stage();
        execute_stage();
        issue_stage();
        dispatch_stage();
        regread_stage();
        rename_stage();
        decode_stage();
        fetch_stage();

        apply_broadcasts(broadcast_tags_);
        advance_cycle();

        if (trace_depleted_ && pipeline_empty()) {
            break;
        }
    }

    // Output per-instruction timing
    for (Instruction *inst : all_insts_) {
        std::printf("%d fu{%d} src{%d,%d} dst{%d} ", inst->seq_no, inst->op_type, inst->src1, inst->src2,
                    inst->dest);
        std::printf("FE{%lld,%lld} ", inst->stages[FE].begin, inst->stages[FE].duration);
        std::printf("DE{%lld,%lld} ", inst->stages[DE].begin, inst->stages[DE].duration);
        std::printf("RN{%lld,%lld} ", inst->stages[RN].begin, inst->stages[RN].duration);
        std::printf("RR{%lld,%lld} ", inst->stages[RR].begin, inst->stages[RR].duration);
        std::printf("DI{%lld,%lld} ", inst->stages[DI].begin, inst->stages[DI].duration);
        std::printf("IS{%lld,%lld} ", inst->stages[IS].begin, inst->stages[IS].duration);
        std::printf("EX{%lld,%lld} ", inst->stages[EX].begin, inst->stages[EX].duration);
        std::printf("WB{%lld,%lld} ", inst->stages[WB].begin, inst->stages[WB].duration);
        std::printf("RT{%lld,%lld}\n", inst->stages[RT].begin, inst->stages[RT].duration);
    }

    double ipc = inst_retired_ == 0 ? 0.0 : static_cast<double>(inst_retired_) / static_cast<double>(cycle_);
    std::printf("# === Simulator Command =========\n");
    std::printf("# ./sim %zu %zu %zu %s\n", params_.rob_size, params_.iq_size, params_.width,
                params_.trace_file.c_str());
    std::printf("# === Processor Configuration ===\n");
    std::printf("# ROB_SIZE = %zu\n", params_.rob_size);
    std::printf("# IQ_SIZE  = %zu\n", params_.iq_size);
    std::printf("# WIDTH    = %zu\n", params_.width);
    std::printf("# === Simulation Results ========\n");
    std::printf("# Dynamic Instruction Count    = %lld\n", inst_retired_);
    std::printf("# Cycles                       = %lld\n", cycle_);
    std::printf("# Instructions Per Cycle (IPC) = %.2f\n", ipc);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::fprintf(stderr, "Error: Wrong number of inputs:%d\n", argc - 1);
        return EXIT_FAILURE;
    }

    ProcParams params{};
    params.rob_size = std::strtoul(argv[1], nullptr, 10);
    params.iq_size = std::strtoul(argv[2], nullptr, 10);
    params.width = std::strtoul(argv[3], nullptr, 10);
    params.trace_file = argv[4];

    Simulator sim(params);
    sim.run();
    return 0;
}
