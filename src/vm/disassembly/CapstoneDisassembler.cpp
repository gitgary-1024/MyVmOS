#include "disassembly/CapstoneDisassembler.h"
#include "disassembly/x86/X86Instruction.h"
#include <capstone/capstone.h>
#include <iostream>
#include <stdexcept>

namespace disassembly {

CapstoneDisassembler::CapstoneDisassembler(Architecture arch, Mode mode)
    : arch_(arch), mode_(mode), handle_(0) {
    
    // 映射架构和模式到 Capstone 的类型
    cs_arch cs_arch_type;
    int cs_mode_val = CS_MODE_LITTLE_ENDIAN;
    
    switch (arch) {
        case Architecture::X86:
            cs_arch_type = CS_ARCH_X86;
            if (mode == Mode::MODE_64) {
                cs_mode_val |= CS_MODE_64;
            } else if (mode == Mode::MODE_32) {
                cs_mode_val |= CS_MODE_32;
            } else {
                cs_mode_val |= CS_MODE_16;
            }
            break;
        default:
            throw std::runtime_error("Unsupported architecture");
    }
    
    // 初始化 Capstone
    cs_err err = cs_open(cs_arch_type, static_cast<cs_mode>(cs_mode_val), &handle_);
    if (err != CS_ERR_OK) {
        throw std::runtime_error(std::string("Failed to initialize Capstone: ") + 
                                 cs_strerror(err));
    }
    
    // 启用详细模式以获取更多信息
    cs_option(handle_, CS_OPT_DETAIL, CS_OPT_ON);
}

CapstoneDisassembler::~CapstoneDisassembler() {
    if (handle_ != 0) {
        cs_close(&handle_);
    }
}

std::shared_ptr<InstructionStream> CapstoneDisassembler::disassemble(
    const uint8_t* code, 
    size_t code_size, 
    uint64_t start_address,
    size_t max_instructions
) {
    auto stream = std::make_shared<InstructionStream>(start_address);
    
    cs_insn* insn = nullptr;
    size_t count = cs_disasm(handle_, code, code_size, start_address, 
                             max_instructions, &insn);
    
    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            // 根据架构创建对应的指令
            std::shared_ptr<Instruction> instruction;
            
            switch (arch_) {
                case Architecture::X86:
                    instruction = x86::create_instruction_from_capstone(
                        &insn[i], 
                        start_address + insn[i].address - start_address,
                        reinterpret_cast<void*>(handle_));  // 传递 Capstone 句柄
                    break;
                default:
                    throw std::runtime_error("Unsupported architecture for disassembly");
            }
            
            if (instruction) {
                stream->add_instruction(instruction);
            }
        }
        
        cs_free(insn, count);
    }
    
    return stream;
}

void CapstoneDisassembler::set_detail_mode(bool enable) {
    cs_option(handle_, CS_OPT_DETAIL, enable ? CS_OPT_ON : CS_OPT_OFF);
}

std::string CapstoneDisassembler::get_version() const {
    int major, minor;
    unsigned int version = cs_version(&major, &minor);
    return "Capstone v" + std::to_string(major) + "." + std::to_string(minor);
}

} // namespace disassembly
