// license:BSD-3-Clause
// copyright-holders:hap
/*

  TMS1000 family - TMS2100, TMS2170, TMS2300, TMS2370

TMS2100 is an enhanced version of TMS1100, adding interrupt, timer, A/D converter, and a 4-level callstack
- the mpla has a similar layout as TMS1400, terms reduced to 26
  (looks like it's optimized and not meant to be custom)
- the opla is the same as TMS1400

Extra functions are controlled with the R register (not mapped to pins):
- R15: enable external interrupt
- R16: K/J input select
- R17: load initial value register with TMA
- R18: internal/external counter clock control
- R19: A1/A2 input select
- R20: enable A/D
- R21: R0-R3 I/O control
- R22: R0-R3/ACC2 output select
- R23: enable decrementer load
- R24: enable interrupts

TODO:
- timer interrupt
- external interrupt (INT pin)
- event counter (EC1 pin)
- R0-R3 I/O, TRA opcode
- A/D converter, TADM opcode

*/

#include "emu.h"
#include "tms2100.h"
#include "tms1k_dasm.h"


// device definitions
DEFINE_DEVICE_TYPE(TMS2100, tms2100_cpu_device, "tms2100", "Texas Instruments TMS2100") // 28-pin DIP, 7 R pins
DEFINE_DEVICE_TYPE(TMS2170, tms2170_cpu_device, "tms2170", "Texas Instruments TMS2170") // high voltage version, 1 R pin removed for Vpp
DEFINE_DEVICE_TYPE(TMS2300, tms2300_cpu_device, "tms2300", "Texas Instruments TMS2300") // 40-pin DIP, 15 R pins, J pins
DEFINE_DEVICE_TYPE(TMS2370, tms2370_cpu_device, "tms2370", "Texas Instruments TMS2370") // high voltage version, 1 R pin removed for Vpp


tms2100_cpu_device::tms2100_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	tms2100_cpu_device(mconfig, TMS2100, tag, owner, clock, 8 /* o pins */, 7 /* r pins */, 6 /* pc bits */, 8 /* byte width */, 3 /* x width */, 4 /* stack levels */, 11 /* rom width */, address_map_constructor(FUNC(tms2100_cpu_device::rom_11bit), this), 7 /* ram width */, address_map_constructor(FUNC(tms2100_cpu_device::ram_7bit), this))
{ }

tms2100_cpu_device::tms2100_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock, u8 o_pins, u8 r_pins, u8 pc_bits, u8 byte_bits, u8 x_bits, u8 stack_levels, int rom_width, address_map_constructor rom_map, int ram_width, address_map_constructor ram_map) :
	tms1100_cpu_device(mconfig, type, tag, owner, clock, o_pins, r_pins, pc_bits, byte_bits, x_bits, stack_levels, rom_width, rom_map, ram_width, ram_map)
{ }

tms2170_cpu_device::tms2170_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	tms2100_cpu_device(mconfig, TMS2170, tag, owner, clock, 8, 6, 6, 8, 3, 4, 11, address_map_constructor(FUNC(tms2170_cpu_device::rom_11bit), this), 7, address_map_constructor(FUNC(tms2170_cpu_device::ram_7bit), this))
{ }

tms2300_cpu_device::tms2300_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	tms2300_cpu_device(mconfig, TMS2300, tag, owner, clock, 8, 15, 6, 8, 3, 4, 11, address_map_constructor(FUNC(tms2300_cpu_device::rom_11bit), this), 7, address_map_constructor(FUNC(tms2300_cpu_device::ram_7bit), this))
{ }

tms2300_cpu_device::tms2300_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock, u8 o_pins, u8 r_pins, u8 pc_bits, u8 byte_bits, u8 x_bits, u8 stack_levels, int rom_width, address_map_constructor rom_map, int ram_width, address_map_constructor ram_map) :
	tms2100_cpu_device(mconfig, type, tag, owner, clock, o_pins, r_pins, pc_bits, byte_bits, x_bits, stack_levels, rom_width, rom_map, ram_width, ram_map)
{ }

tms2370_cpu_device::tms2370_cpu_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	tms2300_cpu_device(mconfig, TMS2370, tag, owner, clock, 8, 14, 6, 8, 3, 4, 11, address_map_constructor(FUNC(tms2370_cpu_device::rom_11bit), this), 7, address_map_constructor(FUNC(tms2370_cpu_device::ram_7bit), this))
{ }


// machine configs
void tms2100_cpu_device::device_add_mconfig(machine_config &config)
{
	// microinstructions PLA, output PLA
	PLA(config, "mpla", 8, 16, 26).set_format(pla_device::FMT::BERKELEY);
	PLA(config, "opla", 5, 8, 32).set_format(pla_device::FMT::BERKELEY);
}


// disasm
std::unique_ptr<util::disasm_interface> tms2100_cpu_device::create_disassembler()
{
	return std::make_unique<tms2100_disassembler>();
}


// device_start/reset
void tms2100_cpu_device::device_start()
{
	tms1100_cpu_device::device_start();

	// zerofill
	m_ac2 = 0;
	m_ivr = 0;

	// register for savestates
	save_item(NAME(m_ac2));
	save_item(NAME(m_ivr));

	state_add(++m_state_count, "AC2", m_ac2).formatstr("%01X"); // 9
}

void tms2100_cpu_device::device_reset()
{
	tms1100_cpu_device::device_reset();

	// changed/added fixed instructions
	m_fixed_decode[0x09] = F_TAX;
	m_fixed_decode[0x0e] = F_TADM;
	m_fixed_decode[0x21] = F_TMA;
	m_fixed_decode[0x26] = F_TAC;
	m_fixed_decode[0x73] = F_TCA;
	m_fixed_decode[0x7b] = F_TRA;
}


// i/o handling
u8 tms2100_cpu_device::read_k_input()
{
	// select K/J port with R16
	return (BIT(m_r, 16) ? m_read_j() : m_read_k()) & 0xf;
}


// opcode deviations
void tms2100_cpu_device::op_tax()
{
	// TAX: transfer accumulator to X register
	m_x = m_a & m_x_mask;
}

void tms2100_cpu_device::op_tra()
{
	// TRA: transfer R inputs to accumulator
}

void tms2100_cpu_device::op_tac()
{
	// TAC: transfer accumulator to AC2
	m_ac2 = m_a;
}

void tms2100_cpu_device::op_tca()
{
	// TCA: transfer AC2 to accumulator
	m_a = m_ac2;
}

void tms2100_cpu_device::op_tadm()
{
	// TADM: transfer A/D register to memory
}

void tms2100_cpu_device::op_tma()
{
	// TMA: if R17 is high, destination is IVR instead of A
	if (BIT(m_r, 17))
	{
		u8 shift = (m_y & 1) * 4;
		m_ivr = (m_ivr & ~(0xf << shift)) | (m_ram_in << shift);
		m_micro &= ~M_AUTA; // don't store in A
	}
}
