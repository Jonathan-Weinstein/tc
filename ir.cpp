#include <vector>
#include <unordered_map>

#include "utility/ByteStream.h"

#define MaxOperands 3
#define MaxSrcs 3

#if 0
#define RA_DEBUG_PRINTF printf
#else
#define RA_DEBUG_PRINTF(...) ((void)0)
#endif

enum RegLoc : uint16_t { RegLocInvalid = 4096 };
enum SpillLoc : uint32_t { SpillLocInvalid = 4096 };

// "a" types are typeless and can hold any type for a bit layout, e.g: a32 could be float or int.
enum IrTypekind : uint8_t {
    Ir_void,
    Ir_bool,
    Ir_a32,
};

enum Opcode : uint16_t {
    Opcode_Literal, // first non-Instruction-Value
    Opcode_GlobalVariable,
    Opcode_ExplicitBlockParameter, // "explicit": this ignores "implicit" live-in values that are the same in every pred
    Opcode_read_test_input,
    Opcode_write_test_output,
    Opcode_spill,
    Opcode_load_spilled,
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
struct Instruction;

struct Use {
    RuntimeValue* value;
    uint operandIndex;

    // For multi block?
    // uint raEstimatedDistanceFrom...; from def or last use?
};

struct RuntimeValue : Value {
    // DenseBlockId?

    std::vector<Use> uses;
    uint instrIndexInBlock = uint(-1); // XXX compute this only before RA instead of always holding,
                                       // only need to care about estimates or distance deltas?
    uint useIterAccelerator = 0; // for regalloc
    RegLoc currentReg = RegLocInvalid; // for regalloc
    SpillLoc spillLoc = SpillLocInvalid;
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
        RegLoc dstReg = RegLocInvalid;
        RegLoc srcRegs[MaxOperands] = {};
        RegAllocState()
        {
            for (RegLoc& r : srcRegs)
                r = RegLocInvalid;
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
        instr->instrIndexInBlock = uint(instructions.size());

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
    case Opcode_ExplicitBlockParameter:
        unreachable; // or not implemented
    CASE(read_test_input);
    CASE(write_test_output);
    CASE(spill);
    CASE(load_spilled);
    CASE(return);
    CASE(iadd);
    }
#undef CASE
    ASSUME(s);
    return s;
}

static void PrintValue(PrintContext&, ByteStream& bs, const Value& value)
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
    } break;
    } // switch
}

static void PrintSlashAndReg(ByteStream& bs, RegLoc reg)
{
    if (reg == RegLocInvalid) Print(bs, R"(\r?)");
    else                     Print(bs, R"(\r)", unsigned(reg));
}

static void PrintBlock(PrintContext& ctx, ByteStream& bs, const Block& block, uint indentation)
{
    for (const Instruction* const instr : block.instructions) {
        bs.PutByteRepeated(' ', indentation);
        if (instr->typekind != Ir_void) {
            ByteStream_printf(bs, "%s %s", TypekindStr(instr->typekind), instr->debugName);
            if (ctx.bPrintRegs) {
                PrintSlashAndReg(bs, instr->ra.dstReg);
            }
            Print(bs, " = ");
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
                const Value* operand = instr->Operand(i);
                PrintValue(ctx, bs, *operand);
                if (ctx.bPrintRegs && operand->opcode != Opcode_Literal) {
                    ASSERT(operand->typekind != Ir_void);
                    PrintSlashAndReg(bs, instr->ra.srcRegs[i]);
                }
                if (++i == instr->OperandCount())
                    break;
                Print(bs, ", ");
            }
            bs.PutByte(')');
        }
        Print(bs, ";\n");
    }
}

struct RegAllocCtx {
    Module& module;

    uint reglimit; // can use at most this many registers

    uint32_t occupiedSpillsBitset = 0;

    uint32_t freeRegsBitset;
    RuntimeValue* valuesInReg[32] = { };
    const char* spillNames[32] = { };

    std::vector<Instruction*> newInstrs;

    RegAllocCtx(const RegAllocCtx&) = delete;
    RegAllocCtx& operator=(const RegAllocCtx&) = delete;

    RegAllocCtx(Module& module, uint registerLimit)
        : module(module)
        , reglimit(registerLimit)
    {
        freeRegsBitset = uint32_t(-1) >> (32 - reglimit);
    }
};

// Location(s) because it is in a register now, but may have spilled somewhere before.
static void UpdateJustUsedSrcValueInReg(
    RegAllocCtx& ctx, uint /*origInstrIndex*/, Instruction* instr, uint srcIndex, RuntimeValue* src)
{
    ASSERT(src->opcode != Opcode_Literal);
    RegLoc const reg = instr->ra.srcRegs[srcIndex];
    ASSERT(src->currentReg == reg);
    ASSERT(reg != RegLocInvalid);
    ASSERT(!(ctx.freeRegsBitset & 1u << reg));

    ASSERT(src->useIterAccelerator < src->uses.size());
    if (++src->useIterAccelerator == src->uses.size()) {
        src->currentReg = RegLocInvalid;
        ctx.valuesInReg[reg] = nullptr;
        ctx.freeRegsBitset |= 1u << reg;

        if (src->spillLoc != SpillLocInvalid) {
            src->spillLoc = SpillLocInvalid;
        }
    }
}

// value could be a src or dst
static RegLoc AllocRegForValueAfterPossiblySpilling(
    RegAllocCtx& ctx, uint origInstrIndex, Instruction* instr, RuntimeValue* value)
{
    ASSERT(value->opcode != Opcode_Literal);

    RegLoc reg;
    if (ctx.freeRegsBitset == 0) {
        // See notes for accelerating this, especially for many registers.
        // Some kind of dataflow analysis for estimated distance when the next use is not with the block,
        // even if use by callblock shouldn't be considered for deciding what to spill (should callblock be considered?),
        // since could have `var a = ...; if (cond) { ThenBlock; } MergeBlock;` where ThenBlock doesn't modify var a
        // (so no ExplicitBlockParameter for it in MergeBlock) and it is live-into MergeBlock; but not used for a long time.
        uint farthestDist = 0;
        RegLoc farthestVictimReg = RegLocInvalid;
        uint32_t const occupiedRegsBitset = ctx.freeRegsBitset ^ (uint32_t(-1) >> (32 - ctx.reglimit));
        for (uint bits = occupiedRegsBitset; bits; bits &= bits - 1) {
            // When a value is a src of the current instruction, value.uses[value.useIterAccelerator] should refer to:
            //  1: Before all sources are allocated: the current instruction.
            //      Note value.useIterAccelerator might not be the last entry in value.uses to do so,
            //      unless this is the rightmost src of the value, see @useIterAccelerator_rightmost
            //  2: After all sources are allocated (case for allocating the dst): an instruction after the current
            //     instruction, or useIterAccelerator == size, which means "no next use".
            //
            // The farthest distance (Belady's) heuristic has another purpose: with bullet 1 above, it is one way of
            // preventing trying to evict src0 when allocating src1 for e.g: `dst = op(src0, src1)`.
            RegLoc const victimReg = RegLoc(bsf(bits));
            RuntimeValue* const victimValue = ctx.valuesInReg[victimReg];

            ASSERT(victimValue->uses.size() >= victimValue->useIterAccelerator);
            uint const nextUseOrigInstrIndex = victimValue->useIterAccelerator == victimValue->uses.size() ?
                uint32_t(-1) :
                victimValue->uses[victimValue->useIterAccelerator].value->instrIndexInBlock;
            ASSERT(nextUseOrigInstrIndex >= origInstrIndex);
            uint const dist = nextUseOrigInstrIndex - origInstrIndex;
            if (dist > farthestDist) {
                farthestDist = dist;
                farthestVictimReg = victimReg;
            }
        }
        ASSERT(farthestVictimReg != RegLocInvalid);
        // allocating for a src?
        if (instr != value) {
#if _DEBUG
            for (uint j = 0; j < countof(instr->ra.srcRegs); ++j) {
                ASSERT(instr->ra.srcRegs[j] != farthestVictimReg);
            }
        }
#endif
        reg = farthestVictimReg;
        RuntimeValue* const farthestVictimValue = ctx.valuesInReg[farthestVictimReg];
        farthestVictimValue->currentReg = RegLocInvalid;
        // Within a basic block, only need to spill a value once.
        if (farthestVictimValue->spillLoc == SpillLocInvalid) {
            // XXX:  Allocate spill loc in immediate dominator of other spills of this value.

            uint32_t freeSpillLocs = ~ctx.occupiedSpillsBitset;
            Implemented(freeSpillLocs);
            SpillLoc spillLoc = SpillLoc(bsf(freeSpillLocs));
            ctx.occupiedSpillsBitset |= 1u << spillLoc;
            farthestVictimValue->spillLoc = spillLoc;
            ctx.spillNames[spillLoc] = farthestVictimValue->debugName;

            Instruction* spillInstr = new Instruction();
            spillInstr->opcode = Opcode_spill;
            spillInstr->typekind = Ir_void;
            spillInstr->_nOperands = 2;
            spillInstr->_operands[0] = ctx.module.LiteralU32(spillLoc);
            spillInstr->_operands[1] = farthestVictimValue;
            spillInstr->ra.srcRegs[1] = reg;
            // XXX: uses/isntrindex messed up
            ctx.newInstrs.push_back(spillInstr);
        }
    }
    else {
        reg = RegLoc(bsf(ctx.freeRegsBitset));
        ASSERT(value->currentReg == RegLocInvalid);
        ASSERT(ctx.valuesInReg[reg] == nullptr);
        ctx.freeRegsBitset &= ~(1u << reg);
    }

    if (value->spillLoc != SpillLocInvalid) {
        ASSERT(instr != value); // should be allocating a reg for a src, not a dst

        Instruction* loadInstr = new Instruction();
        loadInstr->opcode = Opcode_load_spilled;
        loadInstr->typekind = Ir_a32;
        loadInstr->_nOperands = 1;
        loadInstr->_operands[0] = ctx.module.LiteralU32(value->spillLoc);
        // XXX: uses/isntrindex messed up
        loadInstr->debugName = ctx.spillNames[value->spillLoc]; // TODO: diff name or seqno
        loadInstr->spillLoc = value->spillLoc; // in case ahve to spill a value multiplek times, relaod from original spill
        loadInstr->ra.dstReg = reg;
        ctx.newInstrs.push_back(loadInstr);
    }

    value->currentReg = reg;
    ctx.valuesInReg[reg] = value;
    return reg;
}

static void LocalRegisterAllocation(RegAllocCtx& ctx, Block& block)
{
    ASSERT(ctx.newInstrs.empty());
    ctx.newInstrs.reserve(size_t(1) << CeilLog2(uint(block.instructions.size()) | 2));

    for (uint origInstrIndex = 0; origInstrIndex < block.instructions.size(); ++origInstrIndex) {
        Instruction* const instr = block.instructions[origInstrIndex];

        // TODO: if is branch, conditional or not, do not handle here,
        // since actual HW branch needs to be after register/etc passing code.
        // After passing code, update use info of the condition bool if needed.
        // if (instr->opcode == )
        // ^^^ actually, do this by initing size/end to iterate to above loop

        uint32_t uniqueSrcIndexes = 0;
        for (uint srcIndex = 0; srcIndex < instr->OperandCount(); ++srcIndex) {
            Value* const _src = instr->Operand(srcIndex);
            if (_src->opcode == Opcode_Literal) {
                continue;
            }
            RuntimeValue* const src = static_cast<RuntimeValue *>(_src);
            // Since not iterating over callblock instrs, this loop should have few iterations:
            ASSERT(instr->OperandCount() < 6u);
            uint j = 0;
            for (; j < srcIndex; ++j) {
                if (instr->Operand(j) == src) {
                    ASSERT(instr->ra.srcRegs[j] == src->currentReg);
                    instr->ra.srcRegs[srcIndex] = src->currentReg;
                    src->useIterAccelerator++; // @useIterAccelerator_rightmost
                    goto outer_continue_target; // not unique
                }
            }
            ASSERT(srcIndex < sizeof(uniqueSrcIndexes) * BitsPerByte);
            uniqueSrcIndexes |= 1u << srcIndex;
            if (src->currentReg == RegLocInvalid) {
                RA_DEBUG_PRINTF("allocating instr %s src %d\n", instr->debugName, srcIndex);
                instr->ra.srcRegs[srcIndex] = AllocRegForValueAfterPossiblySpilling(ctx, origInstrIndex, instr, src);
            }
            else {
                instr->ra.srcRegs[srcIndex] = src->currentReg;
            }
            outer_continue_target:;
        }
        for (uint32_t bits = uniqueSrcIndexes; bits; bits &= bits - 1) {
            uint const srcIndex = bsf(bits);
            UpdateJustUsedSrcValueInReg(ctx, origInstrIndex, instr, srcIndex,
                                        static_cast<RuntimeValue*>(instr->Operand(srcIndex)));
        }
        if (instr->typekind != Ir_void) {
            RA_DEBUG_PRINTF("allocating instr %s dst\n", instr->debugName);
            instr->ra.dstReg = AllocRegForValueAfterPossiblySpilling(ctx, origInstrIndex, instr, instr);
        }
        ctx.newInstrs.push_back(instr);
    }

#if _DEBUG
    ASSERT(!block.instructions.empty());
    // Could be stricter than this, like for a value defined in a block but only used in that block.
    if (block.instructions.back()->opcode == Opcode_return) {
        for (const Instruction* const instr : block.instructions) {
            ASSERT(instr->useIterAccelerator == instr->uses.size());
        }
    }
#endif

    // NOTE: use vectors are invalid if spilled, but don't have those always anyway?
    block.instructions = std::move(ctx.newInstrs);
}

void DoSomething()
{
    PrintContext ctx = { };
    ubyte streambuf[2048];

    Module m;
    Block block;
    
    {
#define IADD(n, a, b) Value* const n = block.CreateThenAppendInstr2(Opcode_iadd, Ir_a32, a, b, #n);
#define READ_TEST_INPUT(n, c) Value* const n = block.CreateThenAppendInstr1(Opcode_read_test_input, Ir_a32, m.LiteralU32(c), #n);
#define WRITE_TEST_OUTPUT(c, v) (void) block.CreateThenAppendInstr2(Opcode_write_test_output, Ir_void, m.LiteralU32(c), v)

        READ_TEST_INPUT(x, 0);
        READ_TEST_INPUT(y, 4);
        IADD(xy, x, y);
        READ_TEST_INPUT(z, 8);
        IADD(zy, z, y);
        WRITE_TEST_OUTPUT(0, xy);
        WRITE_TEST_OUTPUT(4, zy);
        READ_TEST_INPUT(w, 12);
        IADD(ww, w, w);
        WRITE_TEST_OUTPUT(8, ww);

        (void)block.CreateThenAppendInstr(Opcode_return, Ir_void, 0);
#undef WRITE_TEST_OUTPUT
#undef READ_TEST_INPUT
#undef IADD
    }

    {
        FixedBufferByteStream bs(streambuf, sizeof streambuf);
        ctx.bPrintRegs = false;
        Print(bs, "// Before RA/spilling:\n");
        Print(bs, "void main()\n{\n");
        PrintBlock(ctx, bs, block, 4);
        Print(bs, "}\n");
        fwrite(streambuf, 1, bs.WrappedSize(), stdout);
    }

    {
        RegAllocCtx ractx(m, 2); // try changing this...
        LocalRegisterAllocation(ractx, block);
    }

    {
        FixedBufferByteStream bs(streambuf, sizeof streambuf);
        ctx.bPrintRegs = true;
        Print(bs, "// After RA/spilling:\n");
        Print(bs, "void main()\n{\n");
        PrintBlock(ctx, bs, block, 4);
        Print(bs, "}\n");
        fwrite(streambuf, 1, bs.WrappedSize(), stdout);
    }
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
        if (valueToEvict->spillId == InvalidSpillLoc) {
            valueToEvict->spillId = GetNewSpillLoc;
        }
        else {
            // TODO: Ensure the SpillLoc is allocated in the immediate dominator of all the blocks using it.
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
                FreeRegAndSpillLocOfValue(src);
            }
        }
        if (hasDst) {
            instr->dstReg = AllocRegForValueAfterPossiblySpilling(..., the instr def itself)
        }
        output.Emit(Instr);
    }
}
#endif
