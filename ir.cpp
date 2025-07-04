#include <vector>
#include <unordered_map>

#include "utility/common.h"

#define MaxOperands 3
#define MaxSrcs 3

typedef uint8_t regid;
constexpr regid RegIdInvalid = 0xFF;

enum IrTypekind : uint8_t {
    Ir_void,
    Ir_bool,
    Ir_a32,
};

enum Opcode : uint16_t {
    Opcode_Literal, // first non-Instruction-Value
    Opcode_GlobalVariable,
    Opcode_BlockParameter,
    Opcode_alloca, // first Instruction-Value
    Opcode_read_test_input,
    Opcode_write_test_output,
    Opcode_return,
    Opcode_iadd,
};

struct Value {
    Opcode opcode;
    IrTypekind typekind;
};

struct LiteralValue : Value {
    uint64_t zext;
};

struct RuntimeValue;

struct Use {
    RuntimeValue* value;
    uint operandIndex;

    // For multi block?
    // uint raEstimatedDistanceFrom...; from def or last use?
};

struct RuntimeValue : Value {
    // DenseBlockId?

    std::vector<Use> uses;
    uint useIterAccelerator = 0; // for regalloc
    uint _nOperands = 0;
    Value* _operands[MaxOperands] = { }; // XXX: not enough for pass-to-block-param terminator instrs

    // static string or long-lifetime arena-allocated
    const char* debugName = nullptr;

    void SetOperand(uint i, Value* value)
    {
        ASSERT(i < _nOperands);
        ASSERT(_operands[i] == nullptr);
        ASSERT(value != nullptr);
        _operands[i] = value;

        // See notes about not doing this here, or not always doing it.
        // Or make it clear when this stuff is valid.
        if (value->opcode != Opcode_Literal) {
            auto rtv = static_cast<RuntimeValue*>(value);
            rtv->uses.push_back({ this, i });
        }
    }
    Value* Operand(uint i) const
    {
        ASSERT(i < _nOperands);
        Value* value = _operands[i];
        ASSERT(value != nullptr);
        return value;
    }
    view<Value* const> Operands() const { return { _operands, _nOperands }; }
};

struct Instruction : RuntimeValue {
    struct RegAllocState {
        regid dstReg = RegIdInvalid;
        regid srcRegs[MaxOperands];
        RegAllocState()
        {
            for (regid& r : srcRegs)
                r = RegIdInvalid;
        }
    };

    RegAllocState ra;
};

struct Block {
    std::vector<Instruction *> instructions;

    Block() = default;
    Block(const Block& rhs) = delete;
    Block& operator=(const Block& rhs) = delete;

    ~Block()
    {
        Instruction** pp = instructions.data();
        size_t e = instructions.size();
        while (e) {
            pp[--e]->~Instruction();
        }
    }

    Instruction* CreateThenAppendInstr(Opcode opcode, IrTypekind typekind, uint numOperands)
    {
        Instruction* instr = new Instruction();
        instr->opcode = opcode;
        instr->typekind = typekind;
        instr->_nOperands = numOperands;

        instructions.push_back(instr);
        return instr;
    }
    Instruction* CreateThenAppendInstr1(Opcode opcode, IrTypekind typekind, Value* src, const char* debugName = nullptr)
    {
        // TODO: assert makes sense for opcode/typekind
        Instruction* instr = CreateThenAppendInstr(opcode, typekind, 1);
        instr->debugName = debugName;
        instr->SetOperand(0, src);
        return instr;
    }
    Instruction* CreateThenAppendInstr2(Opcode opcode, IrTypekind typekind, Value* a, Value* b, const char* debugName = nullptr)
    {
        // TODO: assert makes sense for opcode/typekind
        Instruction* instr = CreateThenAppendInstr(opcode, typekind, 2);
        instr->debugName = debugName;
        instr->SetOperand(0, a);
        instr->SetOperand(1, b);
        return instr;
    }
};

struct Module {
    // high 32 bits are typekind, low 32 bits are the zext value
    std::unordered_map<uint64_t, LiteralValue> literalsNon64BitType;
    // std::unordered_map<uint64_t, LiteralValue> literals64BitType;

    LiteralValue* Literal32(uint32_t z)
    {
        std::pair<uint64_t, LiteralValue> p;
        p.first = uint64_t(Ir_a32) << 32 | z;
        p.second.opcode   = Opcode_Literal;
        p.second.typekind = Ir_a32;
        p.second.zext     = z;
        auto r = literalsNon64BitType.insert(p);
        return &r.first->second; // r.first is iterator to pair<key, value>
    }

    LiteralValue* lit_zero_a32;

    Module()
    {
        lit_zero_a32 = Literal32(0);
    }
};

void LocalRegisterAllocation(Block& block)
{
    Block outblock;

    block.instructions = std::move(outblock.instructions);
}

void DoSomething()
{
    Module m;
    Block block;
    Value* input = block.CreateThenAppendInstr1(Opcode_read_test_input, Ir_a32, m.lit_zero_a32, "input");
    (void)         block.CreateThenAppendInstr2(Opcode_write_test_output, Ir_void, m.lit_zero_a32, input);
    (void)         block.CreateThenAppendInstr(Opcode_return, Ir_void, 0);
}

#if 0
// For src operands, caller must ensure its a unique value for the instruction
// Should do updates in here?
// pass in curr instruction heuristic for the 
RegisterId AllocRegForValueAfterPossiblySpilling(
    RegAllocCtx& ctx, Block& block, CodeArray& output, Value *valueToReceiveReg)
{
    if (not have a free reg r) {
        Value* valueToEvict = pick some live value whose next use is farthest from this instruction;
        // For a value, only need to store it to a spill location once.
        if (valueToEvict->spillId == InvalidSpillId) {
            valueToEvict->spillId = GetNewSpillId;
        }
        else {
            // TODO: Ensure the SpillId is allocated in the immediate dominator of all the blocks using it.
        }
        output.EmitSpillStore(spillId, valueToEvict)

        regs[valueToEvict->currentReg].currentValue = nullptr;
        freeRegs.clear(valueToEvict->currentReg);
    }
    // do these updates here, instead of the caller?
    regs[r].currentValue = valueToReceiveReg;
    freeRegs.clear(r);
    valueToReceiveReg.currentReg = r;
    return r;
}

void LocalRegisterAllocation(RegAllocCtx& ctx, Block& block, CodeArray& output)
{
    // some init stuff here
    for (int instrIndex = 0; instrIndex < block->numInstrs; ++instrIndex) {
        Instr* instr = &block->instrs[instrIndex];

        uint32_t uniqueOperandIndexes = 0; // and non-constants

        uint endOperandToCheck = instr->numOperands;
        // TODO: Don't handle the block-params here:
        // if (isUnconditionalBranch) endOperandToCheck = 0;
        // if (isConditionalBranch)   endOperandToCheck = 1;

        for (uint operandIndex = 0; operandIndex < endOperandToCheck; ++operandIndex) {
            Value* operand = intr->operands[operandIndex];
            if (operand->IsConstant) {
                continue;
            }
            // just do this inline or don't care about goto
            // or: FindValueBeforeIndex, return it
            if (ContainsValueBeforeOperandIndex(instr, value, operandIndex)) {
                instr->srcRegs[operandIndex] = instr->srcRegs[j]
                continue;
            }
            uniqueOperandIndexes |= 1u << operandIndex;
            instr->srcRegs[operandIndex] = AllocRegForValueAfterPossiblySpilling(..., operand);
        }
        for (each unique src) {
            update distance heurstics acceleration state
            if (is last use of src) {
                FreeRegAndSpillIdOfValue(src);
            }
        }
        if (hasDst) {
            instr->dstReg = AllocRegForValueAfterPossiblySpilling(..., the instr def itself)
        }
        output.Emit(Instr);
    }
}
// Not an issue for everything is 32-bit scalars and <= 1 dst, but for beyond that:
// If num regs for dsts is > num regs for srcs, would need to spill, but can't spill something just allocated!
// Compute stuff up front, or pass in "cannot be spilled"? Or the farthest use heuristic makes that so?
// ... wait... what am I smoking?}
#endif
