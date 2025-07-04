#include <vector>
#include <unordered_map>

#include "utility/ByteStream.h"

#define MaxOperands 3
#define MaxSrcs 3

typedef uint8_t regid;
constexpr regid RegIdInvalid = 0xFF;

// "a" types are typeless and can hold any type for a bit layout, e.g: a32 could be float or int.
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
    uint OperandCount() const { return _nOperands; }
    view<Value* const> Operands() const { return { _operands, _nOperands }; }
};

struct Instruction : RuntimeValue {
    struct RegAllocState {
        regid dstReg = RegIdInvalid;
        regid srcRegs[MaxOperands] = {};
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

    LiteralValue* LiteralU32(uint32_t z)
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
        lit_zero_a32 = LiteralU32(0);
    }
};

#if 0
static void LocalRegisterAllocation(Block& block)
{
    Block outblock;

    block.instructions = std::move(outblock.instructions);
}
#endif

struct PrintContext {
    bool bPrintRegs = false;
};

static const char* TypekindStr(IrTypekind typekind)
{
    const char* s = nullptr;
    switch (typekind) {
    case Ir_void: s = "void"; break;
    case Ir_bool: s = "bool"; break;
    case Ir_a32:  s = "dword"; break; // idea is to not use numbers since many other things will have numbers
    } // switch
    ASSUME(s);
    return s;
}

static const char* InstructionOpcodeStr(Opcode opcode)
{
    const char* s = nullptr;
#define CASE(n) case Opcode_##n: s = #n; break
    switch (opcode)
    {
    case Opcode_Literal:
    case Opcode_GlobalVariable:
    case Opcode_BlockParameter:
    case Opcode_alloca:
        unreachable; // or not implemented
    CASE(read_test_input);
    CASE(write_test_output);
    CASE(return);
    CASE(iadd);
    }
#undef CASE
    ASSUME(s);
    return s;
}

static void PrintValue(PrintContext& ctx, ByteStream& bs, const Value& value)
{
    switch (value.opcode) {
    case Opcode_Literal: {
        const LiteralValue& lit = static_cast<const LiteralValue&>(value);
        switch (value.typekind) {
        case Ir_void:
            unreachable;
        case Ir_bool:
            ASSERT(lit.zext < 2u);
            Print(bs, lit.zext ? "true" : "false");
            break;
        case Ir_a32:
            // i32 is most common, so have no suffix for it (so just "0" instead of "0_i32" or "i32 0").
            // Will want suffix for other types.
            // If opcode [+ operand index] is float data: print float, in addition to hex. 
            Print(bs, int32_t(lit.zext));
            break;
        } // switch
    } break;
    default: {
        const RuntimeValue& rtv = static_cast<const RuntimeValue&>(value);
        Print(bs, rtv.debugName);
        Implemented(!ctx.bPrintRegs);
    } break;
    } // switch
}

static void PrintBlock(PrintContext& ctx, ByteStream& bs, const Block& block, uint indentation)
{
    for (const Instruction* const instr : block.instructions) {
        bs.PutByteRepeated(' ', indentation);
        if (instr->typekind != Ir_void) {
            ByteStream_printf(bs, "%s %s = ", TypekindStr(instr->typekind), instr->debugName);
        }
        Print(bs, InstructionOpcodeStr(instr->opcode));
        switch (instr->opcode) {
        case Opcode_return:
            if (instr->OperandCount() == 0)
                break;
            // fallthrough
        default:
            bs.PutByte('(');
            for (uint i = 0;;) {
                PrintValue(ctx, bs, *instr->Operand(i));
                if (++i == instr->OperandCount())
                    break;
                Print(bs, ", ");
            }
            bs.PutByte(')');
        }
        Print(bs, ";\n");
    }
}

void DoSomething()
{
    PrintContext ctx = {};
    ubyte streambuf[2048];
    FixedBufferByteStream bs(streambuf, sizeof streambuf);

    Module m;
    Block block;
    Value* x =     block.CreateThenAppendInstr1(Opcode_read_test_input, Ir_a32, m.lit_zero_a32, "x");
    (void)         block.CreateThenAppendInstr2(Opcode_write_test_output, Ir_void, m.lit_zero_a32, x);
    (void)         block.CreateThenAppendInstr(Opcode_return, Ir_void, 0);

    Print(bs, "void main()\n{\n");
    PrintBlock(ctx, bs, block, 4);
    Print(bs, "}\n");

    fwrite(streambuf, 1, bs.WrappedSize(), stdout);
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
#endif
