// license:BSD-3-Clause
// copyright-holders:Sterophonick

// Gigatron disassembler

#ifndef MAME_CPU_GIGATRON_GIGATRONDASM_H
#define MAME_CPU_GIGATRON_GIGATRONDASM_H

#pragma once

class gigatron_disassembler : public util::disasm_interface
{
public:
	gigatron_disassembler() = default;
	virtual ~gigatron_disassembler() = default;

	virtual u32 opcode_alignment() const override;
	virtual offs_t disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params) override;

private:
};

#endif

