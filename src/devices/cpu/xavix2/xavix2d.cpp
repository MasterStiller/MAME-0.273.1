// license:BSD-3-Clause
// copyright-holders:Olivier Galibert, Nathan Gilbert

// Xavix2 disassembler

#include "emu.h"
#include "xavix2d.h"

u32 xavix2_disassembler::opcode_alignment() const
{
	return 1;
}

const u8 xavix2_disassembler::bpo[8] = { 4, 3, 3, 2, 2, 2, 2, 1 };

const char *const xavix2_disassembler::reg_names[8] = { "r0", "r1", "r2", "r3", "r4", "r5", "sp", "lnk" };

const char *xavix2_disassembler::r1()
{
	return reg_names[(m_opcode >> 22) & 7];
}

const char *xavix2_disassembler::r2()
{
	return reg_names[(m_opcode >> 19) & 7];
}

const char *xavix2_disassembler::r3()
{
	return reg_names[(m_opcode >> 16) & 7];
}

std::string xavix2_disassembler::val22s()
{
	u32 r = m_opcode & 0x3fffff;
	if(m_opcode & 0x200000)
		return util::string_format("-%06x", 0x400000 - r);
	else
		return util::string_format("%06x", r);
}

std::string xavix2_disassembler::val19u()
{
	return util::string_format("%05x", m_opcode & 0x7ffff);
}

std::string xavix2_disassembler::val14h()
{
	return util::string_format("%08x", ((m_opcode >> 8) & 0x3fff) << 18);
}

std::string xavix2_disassembler::val14u()
{
	return util::string_format("%04x", (m_opcode >> 8) & 0x3fff);
}

std::string xavix2_disassembler::val14s()
{
	u16 r = (m_opcode >> 8) & 0x3fff;
	if(r & 0x2000)
		return util::string_format("-%04x", 0x4000 - r);
	else
		return util::string_format("%04x", r);
}

std::string xavix2_disassembler::val11u()
{
	return util::string_format("%03x", (m_opcode >> 8) & 0x7ff);
}

std::string xavix2_disassembler::val6s()
{
	u16 r = (m_opcode >> 16) & 0x3f;
	if(r & 0x20)
		return util::string_format("-%02x", 0x40 - r);
	else
		return util::string_format("%02x", r);
}

std::string xavix2_disassembler::val3u()
{
	return util::string_format("%x", (m_opcode >> 16) & 0x7);
}

std::string xavix2_disassembler::off11s()
{
	u16 r = (m_opcode >> 8) & 0x7ff;
	if(r & 0x400)
		return util::string_format(" - %03x", 0x800 - r);
	else if(r)
		return util::string_format(" + %03x", r);
	else
		return "";
}

std::string xavix2_disassembler::off6s()
{
	u16 r = (m_opcode >> 16) & 0x3f;
	if(r & 0x20)
		return util::string_format(" - %02x", 0x40 - r);
	else if(r)
		return util::string_format(" + %02x", r);
	else
		return "";
}

std::string xavix2_disassembler::off3s()
{
	u16 r = (m_opcode >> 16) & 0x7;
	if(r & 0x4)
		return util::string_format(" - %x", 8 - r);
	else if(r)
		return util::string_format(" + %x", r);
	else
		return "";
}

std::string xavix2_disassembler::adr24()
{
	return util::string_format("%06x", m_opcode & 0xffffff);
}

std::string xavix2_disassembler::adr16()
{
	return util::string_format("%06x", (m_pc & 0xffff0000) | ((m_opcode >> 8) & 0xffff));
}

std::string xavix2_disassembler::rel16()
{
	return util::string_format("%06x", m_pc + static_cast<s16>(m_opcode >> 8));
}

std::string xavix2_disassembler::rel8()
{
	return util::string_format("%06x", m_pc + static_cast<s8>(m_opcode >> 16));
}

offs_t xavix2_disassembler::disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params)
{
	m_pc = pc;
	m_opcode = opcodes.r8(m_pc) << 24;
	u8 nb = bpo[m_opcode >> 29];
	for(u8 i=1; i != nb; i++)
		m_opcode |= opcodes.r8(m_pc + i) << (24 - 8*i);

	u32 flags = 0;
	switch(m_opcode >> 24) {
		// 00-05
	case 0x06: case 0x07: util::stream_format(stream, "%s = %s", r1(), val22s()); break;
	case 0x08:            util::stream_format(stream, "jmp %s", adr24()); break;
	case 0x09:            util::stream_format(stream, "jsr %s", adr24()); flags = STEP_OVER; break;
	case 0x0a: case 0x0b: util::stream_format(stream, "%s = %s & %s", r1(), r2(), val19u()); break;
	case 0x0c: case 0x0d: util::stream_format(stream, "%s = %s | %s", r1(), r2(), val19u()); break;
		// 0e-1f

	case 0x20:            util::stream_format(stream, "jmp %s", adr16()); break;
	case 0x21:            util::stream_format(stream, "jsr %s", adr16()); flags = STEP_OVER; break;
	case 0x22: case 0x23: util::stream_format(stream, "%s = %s", r1(), val14h()); break;
	case 0x24: case 0x25: util::stream_format(stream, "%s -= %s", r1(), val14s()); break;
	case 0x26: case 0x27: util::stream_format(stream, "cmp %s, %s", r1(), val14s()); break;
	case 0x28:            util::stream_format(stream, "bra %s", rel16()); break;
	case 0x29:            util::stream_format(stream, "bsr %s", rel16()); flags = STEP_OVER; break;
	case 0x2a: case 0x2b: util::stream_format(stream, "%s = %s & %s", r1(), r2(), val11u()); break;
	case 0x2c: case 0x2d: util::stream_format(stream, "%s = %s | %s", r1(), r2(), val11u()); break;
		// 2e-41

	case 0x42: case 0x43: util::stream_format(stream, "%s = (%s%s).b", r1(), r2(), off11s()); break;
	case 0x44: case 0x45: util::stream_format(stream, "%s = (%s%s).w", r1(), r2(), off11s()); break;
	case 0x46: case 0x47: util::stream_format(stream, "%s = (%s%s).w???", r1(), r2(), off11s()); break;
	case 0x48: case 0x49: util::stream_format(stream, "%s = (%s%s).l", r1(), r2(), off11s()); break;
	case 0x4a: case 0x4b: util::stream_format(stream, "(%s%s).b = %s", r2(), off11s(), r1()); break;
	case 0x4c: case 0x4d: util::stream_format(stream, "(%s%s).w = %s", r2(), off11s(), r1()); break;
	case 0x4e: case 0x4f: util::stream_format(stream, "(%s%s).l = %s", r2(), off11s(), r1()); break;

	case 0x50: case 0x51: util::stream_format(stream, "%s = %s.b???", r1(), val14u()); break;
	case 0x52: case 0x53: util::stream_format(stream, "%s = %s.b", r1(), val14u()); break;
	case 0x54: case 0x55: util::stream_format(stream, "%s = %s.w", r1(), val14u()); break;
	case 0x56: case 0x57: util::stream_format(stream, "%s = %s.l", r1(), val14u()); break;
	case 0x58: case 0x59: util::stream_format(stream, "%s.b = %s???", val14u(), r1()); break;
	case 0x5a: case 0x5b: util::stream_format(stream, "%s.b = %s", val14u(), r1()); break;
	case 0x5c: case 0x5d: util::stream_format(stream, "%s.w = %s", val14u(), r1()); break;
	case 0x5e: case 0x5f: util::stream_format(stream, "%s.l = %s", val14u(), r1()); break;

	case 0x60: case 0x61: util::stream_format(stream, "%s += %s", r1(), val6s()); break;
	case 0x62: case 0x63: util::stream_format(stream, "%s = %s", r1(), val6s()); break;
	case 0x64: case 0x65: util::stream_format(stream, "%s -= %s", r1(), val6s()); break;
	case 0x66: case 0x67: util::stream_format(stream, "cmp %s, %s", r1(), val6s()); break;
		// 68-6b
	case 0x6c: case 0x6d: util::stream_format(stream, "%s = %s >> %s", r1(), r2(), val3u()); break;
	case 0x6e: case 0x6f: util::stream_format(stream, "%s = %s << %s", r1(), r2(), val3u()); break;

		// 70-77
	case 0x78: case 0x79: util::stream_format(stream, "%s = (sp%s).l", r1(), off6s()); break;
		// 7a-7d
	case 0x7e: case 0x7f: util::stream_format(stream, "(sp%s).l = %s", off6s(), r1()); break;

	case 0x80: case 0x81: util::stream_format(stream, "%s = %s + %s", r1(), r2(), r3()); break;
		// 82-83
	case 0x84: case 0x85: util::stream_format(stream, "?84 %s %s %s", r1(), r2(), r3()); break;
		// 86-89
	case 0x8a: case 0x8b: util::stream_format(stream, "%s = %s & %s", r1(), r2(), r3()); break;
	case 0x8c: case 0x8d: util::stream_format(stream, "%s = %s | %s", r1(), r2(), r3()); break;
		// 8e-8f

	case 0x90: case 0x91: util::stream_format(stream, "%s = (%s%s).b???", r1(), r2(), off3s()); break;
	case 0x92: case 0x93: util::stream_format(stream, "%s = (%s%s).b", r1(), r2(), off3s()); break;
	case 0x94: case 0x95: util::stream_format(stream, "(%s%s).b = %s???", r2(), off3s(), r1()); break;
	case 0x96: case 0x97: util::stream_format(stream, "%s = (%s%s).w", r1(), r2(), off3s()); break;
	case 0x98: case 0x99: util::stream_format(stream, "%s = (%s%s).l", r1(), r2(), off3s()); break;
	case 0x9a: case 0x9b: util::stream_format(stream, "(%s%s).b = %s", r2(), off3s(), r1()); break;
	case 0x9c: case 0x9d: util::stream_format(stream, "(%s%s).w = %s", r2(), off3s(), r1()); break;
	case 0x9e: case 0x9f: util::stream_format(stream, "(%s%s).l = %s", r2(), off3s(), r1()); break;

		// a0-a1
	case 0xa2: case 0xa3: util::stream_format(stream, "%s = %s", r1(), r2()); break;
		// a4-a5
	case 0xa6: case 0xa7: util::stream_format(stream, "cmp %s, %s", r1(), r2()); break;
		// a8-ab
	case 0xac: case 0xad: util::stream_format(stream, "%s = %s >> %s", r1(), r2(), r3()); break;
	case 0xae: case 0xaf: util::stream_format(stream, "%s = %s << %s", r1(), r2(), r3()); break;

		// b0-b1
	case 0xb2: case 0xb3: util::stream_format(stream, "?b2 %s %s %s", r1(), r2(), r3()); break;
		// b4-cf

	case 0xd0:            util::stream_format(stream, "b??0 %s", rel8()); break;
	case 0xd1:            util::stream_format(stream, "b??1 %s", rel8()); break;
	case 0xd2:            util::stream_format(stream, "bles %s", rel8()); break;
	case 0xd3:            util::stream_format(stream, "b??3 %s", rel8()); break;
	case 0xd4:            util::stream_format(stream, "b??4 %s", rel8()); break;
	case 0xd5:            util::stream_format(stream, "b??5 %s", rel8()); break;
	case 0xd6:            util::stream_format(stream, "b??6 %s", rel8()); break;
	case 0xd7:            util::stream_format(stream, "bleu %s", rel8()); break;
	case 0xd8:            util::stream_format(stream, "b??8 %s", rel8()); break;
	case 0xd9:            util::stream_format(stream, "b??9 %s", rel8()); break;
	case 0xda:            util::stream_format(stream, "bne  %s", rel8()); break;
	case 0xdb:            util::stream_format(stream, "bgts %s", rel8()); break;
	case 0xdc:            util::stream_format(stream, "b??c %s", rel8()); break;
	case 0xdd:            util::stream_format(stream, "b??d %s", rel8()); break;
	case 0xde:            util::stream_format(stream, "b??e %s", rel8()); break;
	case 0xdf:            util::stream_format(stream, "b??f %s", rel8()); break;

	case 0xe0:            util::stream_format(stream, "jmp lr"); flags = STEP_OUT; break;
	case 0xe1:            util::stream_format(stream, "rti1"); flags = STEP_OUT; break;
		// e2
	case 0xe3:            util::stream_format(stream, "rti2"); flags = STEP_OUT; break;
		// e4-ff

	default:	          util::stream_format(stream, "?%02x", m_opcode >> 24);
	}

	return nb | flags | SUPPORTED;
}
