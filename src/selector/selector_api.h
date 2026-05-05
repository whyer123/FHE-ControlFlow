#pragma once

#include "src/fhe/fhe_context.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

enum class ABE2NodeKind {
    Conditional,
    Work,
    Terminal
};

struct ABE2StateToken {
    size_t node_id = 0;
    uint64_t capability = 0;
};

struct ABE2TransitionResult {
    ABE2StateToken next_token;
    bool is_terminal = false;
};

class SelectorAPI {
public:
    virtual ~SelectorAPI() = default;

    virtual ABE2StateToken BootstrapToken() const = 0;
    virtual ABE2NodeKind GetNodeKind(const ABE2StateToken& token) const = 0;
    virtual std::string DescribeState(const ABE2StateToken& token) const = 0;
    virtual bool IsTerminal(const ABE2StateToken& token) const = 0;
    virtual ABE2TransitionResult EvaluateConditional(const ABE2StateToken& token,
                                                     const LWECiphertext& cond_bit) = 0;
    virtual ABE2TransitionResult EvaluateUnconditional(const ABE2StateToken& token) = 0;
};

class ProgrammedSelectorBase : public SelectorAPI {
public:
    explicit ProgrammedSelectorBase(uint64_t program_secret)
        : program_secret_(program_secret),
          program_({
              {"CHECK_CONDITION", ABE2NodeKind::Conditional, 1, 2, 0},
              {"LOOP_BODY", ABE2NodeKind::Work, 0, 0, 0},
              {"HALT", ABE2NodeKind::Terminal, 0, 0, 2},
          }) {}

    ABE2StateToken BootstrapToken() const override {
        return MintToken(0);
    }

    ABE2NodeKind GetNodeKind(const ABE2StateToken& token) const override {
        return ValidateAndGetNode(token).kind;
    }

    std::string DescribeState(const ABE2StateToken& token) const override {
        return ValidateAndGetNode(token).name;
    }

    bool IsTerminal(const ABE2StateToken& token) const override {
        return GetNodeKind(token) == ABE2NodeKind::Terminal;
    }

    ABE2TransitionResult EvaluateConditional(const ABE2StateToken& token,
                                             const LWECiphertext& cond_bit) override {
        const auto& node = ValidateAndGetNode(token);
        if (node.kind != ABE2NodeKind::Conditional) {
            throw std::logic_error("Conditional transition requested from non-conditional node.");
        }

        const size_t next_id = ExtractBranch(cond_bit) ? node.next_on_true : node.next_on_false;
        return {MintToken(next_id), program_[next_id].kind == ABE2NodeKind::Terminal};
    }

    ABE2TransitionResult EvaluateUnconditional(const ABE2StateToken& token) override {
        const auto& node = ValidateAndGetNode(token);
        if (node.kind != ABE2NodeKind::Work) {
            throw std::logic_error("Unconditional transition requested from non-work node.");
        }

        const size_t next_id = node.next_on_unconditional;
        return {MintToken(next_id), program_[next_id].kind == ABE2NodeKind::Terminal};
    }

protected:
    virtual bool ExtractBranch(const LWECiphertext& cond_bit) = 0;

private:
    struct ProgramNode {
        std::string name;
        ABE2NodeKind kind;
        size_t next_on_true;
        size_t next_on_false;
        size_t next_on_unconditional;
    };

    ABE2StateToken MintToken(size_t node_id) const {
        return {node_id, program_secret_ ^ (0x9E3779B97F4A7C15ULL * (node_id + 1))};
    }

    const ProgramNode& ValidateAndGetNode(const ABE2StateToken& token) const {
        if (token.node_id >= program_.size()) {
            throw std::out_of_range("Invalid ABE2 state token: unknown node.");
        }

        const auto expected = MintToken(token.node_id).capability;
        if (token.capability != expected) {
            throw std::runtime_error("Invalid ABE2 state token: capability mismatch.");
        }

        return program_[token.node_id];
    }

    uint64_t program_secret_;
    std::vector<ProgramNode> program_;
};

class TrustedSelector : public ProgrammedSelectorBase {
public:
    explicit TrustedSelector(FHEContextWrapper& fhe)
        : ProgrammedSelectorBase(0x5452555354454453ULL), fhe_ctx(fhe) {}

private:
    bool ExtractBranch(const LWECiphertext& cond_bit) override;

    FHEContextWrapper& fhe_ctx;
};

class ABESelector : public ProgrammedSelectorBase {
public:
    explicit ABESelector(FHEContextWrapper& ctx);

private:
    bool ExtractBranch(const LWECiphertext& cond_bit) override;

    FHEContextWrapper& fhe_ctx;
};
