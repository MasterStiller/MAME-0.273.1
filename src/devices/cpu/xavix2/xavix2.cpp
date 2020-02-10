// license:BSD-3-Clause
// copyright-holders:Olivier Galibert, Nathan Gilbert

#include "emu.h"
#include "xavix2.h"
#include "xavix2d.h"
#include "debugger.h"

DEFINE_DEVICE_TYPE(XAVIX2, xavix2_device, "xavix2", "Xavix 2 CPU")

const u8 xavix2_device::bpo[8] = { 4, 3, 3, 2, 2, 2, 2, 1 };

xavix2_device::xavix2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: cpu_device(mconfig, XAVIX2, tag, owner, clock)
	, m_program_config("program", ENDIANNESS_LITTLE, 32, 32)
{
}

void xavix2_device::device_start()
{
	m_program = &space(AS_PROGRAM);
	m_program_cache = m_program->cache<2, 0, ENDIANNESS_LITTLE>();

	state_add(STATE_GENPC,     "GENPC",     m_pc).callexport().noshow();
	state_add(STATE_GENPCBASE, "CURPC",     m_pc).callexport().noshow();
	state_add(STATE_GENSP,     "GENSP",     m_r[7]).noshow();
	state_add(STATE_GENFLAGS,  "GENFLAGS",  m_f).callimport().formatstr("%4s").noshow();
	state_add(XAVIX2_PC,       "PC",        m_pc).callimport();
	state_add(XAVIX2_FLAGS,    "FLAGS",     m_f).callimport();
	state_add(XAVIX2_R0,       "R0",        m_r[0]);
	state_add(XAVIX2_R1,       "R1",        m_r[1]);
	state_add(XAVIX2_R2,       "R2",        m_r[2]);
	state_add(XAVIX2_R3,       "R3",        m_r[3]);
	state_add(XAVIX2_R4,       "R4",        m_r[4]);
	state_add(XAVIX2_R5,       "R5",        m_r[5]);
	state_add(XAVIX2_SP,       "SP",        m_r[6]);
	state_add(XAVIX2_LR,       "LR",        m_r[7]);

	save_item(NAME(m_pc));
	save_item(NAME(m_f));
	save_item(NAME(m_r));

	set_icountptr(m_icount);

	m_pc = 0;
	m_f = 0;
	memset(m_r, 0, sizeof(m_r));
}

void xavix2_device::device_reset()
{
	m_pc = 0x40000000;
}

uint32_t xavix2_device::execute_min_cycles() const noexcept
{
	return 1;
}

uint32_t xavix2_device::execute_max_cycles() const noexcept
{
	return 5;
}

uint32_t xavix2_device::execute_input_lines() const noexcept
{
	return 0;
}

void xavix2_device::execute_set_input(int inputnum, int state)
{
}

device_memory_interface::space_config_vector xavix2_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_PROGRAM, &m_program_config)
	};
}

std::unique_ptr<util::disasm_interface> xavix2_device::create_disassembler()
{
	return std::make_unique<xavix2_disassembler>();
}

void xavix2_device::state_string_export(const device_state_entry &entry, std::string &str) const
{
}

void xavix2_device::execute_run()
{
	while(m_icount > 0) {
		if(machine().debug_flags & DEBUG_FLAG_ENABLED)
			debugger_instruction_hook(m_pc);

		u32 opcode = m_program_cache->read_byte(m_pc) << 24;
		m_icount --;

		u8 nb = bpo[opcode >> 29];
		u32 npc = m_pc + nb;
		for(u8 i=1; i != nb; i++) {
			opcode |= m_program_cache->read_byte(m_pc + i) << (24 - 8*i);
			m_icount --;
		}

		switch(opcode >> 24) {
		case 0x00: case 0x01: m_r[r1(opcode)] = do_add(m_r[r2(opcode)], val19s(opcode)); break;
			// 02-03
		case 0x04: case 0x05: m_r[r1(opcode)] = do_sub(m_r[r2(opcode)], val19s(opcode)); break;
		case 0x06: case 0x07: m_r[r1(opcode)] = val22s(opcode); break;
		case 0x08:            npc = val24u(opcode) | (m_pc & 0xff000000); break;
		case 0x09:            m_r[7] = npc; npc = val24u(opcode) | (m_pc & 0xff000000); break;
		case 0x0a: case 0x0b: m_r[r1(opcode)] = do_and(m_r[r2(opcode)], val19u(opcode)); break;
		case 0x0c: case 0x0d: m_r[r1(opcode)] = do_or (m_r[r2(opcode)], val19u(opcode)); break;
		case 0x0e: case 0x0f: m_r[r1(opcode)] = do_xor(m_r[r2(opcode)], val19u(opcode)); break;

		case 0x10: case 0x11: m_r[r1(opcode)] = (s8)m_program->read_byte(m_r[r2(opcode)] + val19s(opcode)); break;
		case 0x12: case 0x13: m_r[r1(opcode)] = m_program->read_byte(m_r[r2(opcode)] + val19s(opcode)); break;
		case 0x14: case 0x15: m_r[r1(opcode)] = (s16)m_program->read_word(m_r[r2(opcode)] + val19s(opcode)); break;
		case 0x16: case 0x17: m_r[r1(opcode)] = m_program->read_word(m_r[r2(opcode)] + val19s(opcode)); break;
		case 0x18: case 0x19: m_r[r1(opcode)] = m_program->read_dword(m_r[r2(opcode)] + val19s(opcode)); break;
		case 0x1a: case 0x1b: m_program->write_byte(m_r[r2(opcode)] + val19s(opcode), m_r[r1(opcode)]); break;
		case 0x1c: case 0x1d: m_program->write_byte(m_r[r2(opcode)] + val19s(opcode), m_r[r1(opcode)]); break;
		case 0x1e: case 0x1f: m_program->write_byte(m_r[r2(opcode)] + val19s(opcode), m_r[r1(opcode)]); break;

		case 0x20: case 0x21: m_r[r1(opcode)] = do_sub(m_r[r1(opcode)], val14s(opcode)); break;
		case 0x22: case 0x23: m_r[r1(opcode)] = val14h(opcode); break;
		case 0x24: case 0x25: m_r[r1(opcode)] = do_sub(m_r[r1(opcode)], val14s(opcode)); break;
		case 0x26: case 0x27: do_sub(m_r[r1(opcode)], val14s(opcode)); break;
		case 0x28:            npc = m_pc + val16s(opcode); break;
		case 0x29:            m_r[7] = npc; npc = m_pc + val16s(opcode); break;
		case 0x2a: case 0x2b: m_r[r1(opcode)] = do_and(m_r[r2(opcode)], val11u(opcode)); break;
		case 0x2c: case 0x2d: m_r[r1(opcode)] = do_or (m_r[r2(opcode)], val11u(opcode)); break;
		case 0x2e: case 0x2f: m_r[r1(opcode)] = do_xor(m_r[r2(opcode)], val11u(opcode)); break;

			// 30-3f

		case 0x40: case 0x41: m_r[r1(opcode)] = (s8)m_program->read_byte(m_r[r2(opcode)] + val11s(opcode)); break;
		case 0x42: case 0x43: m_r[r1(opcode)] = m_program->read_byte(m_r[r2(opcode)] + val11s(opcode)); break;
		case 0x44: case 0x45: m_r[r1(opcode)] = (s16)m_program->read_word(m_r[r2(opcode)] + val11s(opcode)); break;
		case 0x46: case 0x47: m_r[r1(opcode)] = m_program->read_word(m_r[r2(opcode)] + val11s(opcode)); break;
		case 0x48: case 0x49: m_r[r1(opcode)] = m_program->read_dword(m_r[r2(opcode)] + val11s(opcode)); break;
		case 0x4a: case 0x4b: m_program->write_byte(m_r[r2(opcode)] + val11s(opcode), m_r[r1(opcode)]); break;
		case 0x4c: case 0x4d: m_program->write_byte(m_r[r2(opcode)] + val11s(opcode), m_r[r1(opcode)]); break;
		case 0x4e: case 0x4f: m_program->write_byte(m_r[r2(opcode)] + val11s(opcode), m_r[r1(opcode)]); break;

		case 0x50: case 0x51: m_r[r1(opcode)] = (s8)m_program->read_byte(val14u(opcode)); break;
		case 0x52: case 0x53: m_r[r1(opcode)] = m_program->read_byte(val14u(opcode)); break;
		case 0x54: case 0x55: m_r[r1(opcode)] = (s16)m_program->read_word(val14u(opcode)); break;
		case 0x56: case 0x57: m_r[r1(opcode)] = m_program->read_word(val14u(opcode)); break;
		case 0x58: case 0x59: m_r[r1(opcode)] = m_program->read_dword(val14u(opcode)); break;
		case 0x5a: case 0x5b: m_program->write_byte(val14u(opcode), m_r[r1(opcode)]); break;
		case 0x5c: case 0x5d: m_program->write_word(val14u(opcode), m_r[r1(opcode)]); break;
		case 0x5e: case 0x5f: m_program->write_dword(val14u(opcode), m_r[r1(opcode)]); break;

		case 0x60: case 0x61: m_r[r1(opcode)] = do_add(m_r[r1(opcode)], val6s(opcode)); break;
		case 0x62: case 0x63: m_r[r1(opcode)] = val6s(opcode); break;
		case 0x64: case 0x65: m_r[r1(opcode)] = do_sub(m_r[r1(opcode)], val6s(opcode)); break;
		case 0x66: case 0x67: do_sub(m_r[r1(opcode)], val6s(opcode)); break;
			// 68-69
		case 0x6a: case 0x6b: m_r[r1(opcode)] = do_asr(m_r[r2(opcode)], val3u(opcode)); break;
		case 0x6c: case 0x6d: m_r[r1(opcode)] = do_lsr(m_r[r2(opcode)], val3u(opcode)); break;
		case 0x6e: case 0x6f: m_r[r1(opcode)] = do_lsl(m_r[r2(opcode)], val3u(opcode)); break;

		case 0x70: case 0x71: m_r[r1(opcode)] = (s8)m_program->read_byte(m_r[6] + val6s(opcode)); break;
		case 0x72: case 0x73: m_r[r1(opcode)] = m_program->read_byte(m_r[6] + val6s(opcode)); break;
		case 0x74: case 0x75: m_r[r1(opcode)] = (s16)m_program->read_word(m_r[6] + val6s(opcode)); break;
		case 0x76: case 0x77: m_r[r1(opcode)] = m_program->read_word(m_r[6] + val6s(opcode)); break;
		case 0x78: case 0x79: m_r[r1(opcode)] = m_program->read_dword(m_r[6] + val6s(opcode)); break;
		case 0x7a: case 0x7b: m_program->write_byte(m_r[6] + val6s(opcode), m_r[r1(opcode)]); break;
		case 0x7c: case 0x7d: m_program->write_word(m_r[6] + val6s(opcode), m_r[r1(opcode)]); break;
		case 0x7e: case 0x7f: m_program->write_dword(m_r[6] + val6s(opcode), m_r[r1(opcode)]); break;

		case 0x80: case 0x81: m_r[r1(opcode)] = do_add(m_r[r2(opcode)], m_r[r3(opcode)]); break;
			// 82-83
		case 0x84: case 0x85: m_r[r1(opcode)] = do_sub(m_r[r2(opcode)], m_r[r3(opcode)]); break;
			// 86-89
		case 0x8a: case 0x8b: m_r[r1(opcode)] = do_and(m_r[r2(opcode)], m_r[r3(opcode)]); break;
		case 0x8c: case 0x8d: m_r[r1(opcode)] = do_or (m_r[r2(opcode)], m_r[r3(opcode)]); break;
		case 0x8e: case 0x8f: m_r[r1(opcode)] = do_xor(m_r[r2(opcode)], m_r[r3(opcode)]); break;

		case 0x90: case 0x91: m_r[r1(opcode)] = (s8)m_program->read_byte(m_r[r2(opcode)] + val3s(opcode)); break;
		case 0x92: case 0x93: m_r[r1(opcode)] = m_program->read_byte(m_r[r2(opcode)] + val3s(opcode)); break;
		case 0x94: case 0x95: m_r[r1(opcode)] = (s16)m_program->read_word(m_r[r2(opcode)] + val3s(opcode)); break;
		case 0x96: case 0x97: m_r[r1(opcode)] = m_program->read_word(m_r[r2(opcode)] + val3s(opcode)); break;
		case 0x98: case 0x99: m_r[r1(opcode)] = m_program->read_dword(m_r[r2(opcode)] + val3s(opcode)); break;
		case 0x9a: case 0x9b: m_program->write_byte(m_r[r2(opcode)] + val3s(opcode), m_r[r1(opcode)]); break;
		case 0x9c: case 0x9d: m_program->write_word(m_r[r2(opcode)] + val3s(opcode), m_r[r1(opcode)]); break;
		case 0x9e: case 0x9f: m_program->write_dword(m_r[r2(opcode)] + val3s(opcode), m_r[r1(opcode)]); break;

			// a0-a1
		case 0xa2: case 0xa3: m_r[r1(opcode)] = m_r[r2(opcode)]; break;
			// a4-a5
		case 0xa6: case 0xa7: do_sub(m_r[r1(opcode)], m_r[r2(opcode)]); break;
			// a8-ab
		case 0xaa: case 0xab: m_r[r1(opcode)] = do_asr(m_r[r2(opcode)], m_r[r3(opcode)]); break;
		case 0xac: case 0xad: m_r[r1(opcode)] = do_lsr(m_r[r2(opcode)], m_r[r3(opcode)]); break;
		case 0xae: case 0xaf: m_r[r1(opcode)] = do_lsl(m_r[r2(opcode)], m_r[r3(opcode)]); break;

		case 0xd0:            if(m_f & F_V) npc = m_pc + val8s(opcode); break;
		case 0xd1:            if(((m_f & F_N) && !(m_f & F_V)) || ((m_f & F_V) && !(m_f & F_N))) npc = m_pc + val8s(opcode); break;
		case 0xd2:            if(m_f & F_Z) npc = m_pc + val8s(opcode); break;
		case 0xd3:            if((m_f & F_Z) || ((m_f & F_N) && !(m_f & F_V)) || ((m_f & F_V) && !(m_f & F_N))) npc = m_pc + val8s(opcode); break;
		case 0xd4:            if(m_f & F_N) npc = m_pc + val8s(opcode); break;
		case 0xd5:            npc = m_pc + val8s(opcode); break;
		case 0xd6:            if(m_f & F_C) npc = m_pc + val8s(opcode); break;
		case 0xd7:            if((m_f & F_Z) || (m_f & F_C)) npc = m_pc + val8s(opcode); break;
		case 0xd8:            if(!(m_f & F_V)) npc = m_pc + val8s(opcode); break;
		case 0xd9:            if(((m_f & F_N) && (m_f & F_V)) || (!(m_f & F_V) && !(m_f & F_N))) npc = m_pc + val8s(opcode); break;
		case 0xda:            if(!(m_f & F_Z)) npc = m_pc + val8s(opcode); break;
		case 0xdb:            if((!(m_f & F_Z) && (m_f & F_N) && (m_f & F_V)) || (!(m_f & F_Z) && !(m_f & F_V) && !(m_f & F_N))) npc = m_pc + val8s(opcode); break;
		case 0xdc:            if(!(m_f & F_N)) npc = m_pc + val8s(opcode); break;
		case 0xdd:            break;														  
		case 0xde:            if(!(m_f & F_C)) npc = m_pc + val8s(opcode); break;
		case 0xdf:            if(!(m_f & F_Z) && !(m_f & F_C)) npc = m_pc + val8s(opcode); break;

		case 0xe0:            npc = m_r[7]; break;
		case 0xe1:            /* rti1 */ break;
			// e2
		case 0xe3:            /* rti2 */ break;
			// e4-fb
		case 0xfc:            break;
			// fd-ff
		}
			
		m_pc = npc;
	}
}
