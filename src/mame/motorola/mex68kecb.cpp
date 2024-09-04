// license:BSD-3-Clause
// copyright-holders:Chris Hanson
/*
 * mex68kecb.cpp - Motorola MEX68KECB
 *
 * Created on: August 31, 2024
 *     Author: Chris Hanson
 *
 * Documentation:
 *   http://www.bitsavers.org/components/motorola/68000/MEX68KECB/MEX68KECB_D2_EduCompBd_Jul82.pdf
 *
 * The Motorola MC68000 Educational Computer Board is a single-board computer with
 * a 4MHz 68000 CPU, 32KB RAM, 16KB ROM, host and terminal serial ports, a
 * parallel interface/timer, a cassette interface, and a prototyping area with
 * full access to the 68000 bus. The ROM contains TUTOR, a debug and bootstrap
 * system that was the predecessor of MACSBUG.
 *
 * Specifications:
 * - 4MHz MC68000L4 CPU
 * - MC6850 ACIA x 2
 * - MC68230 PIT
 *
 * To Do:
 * - Cassette I/O
 * - Save/Restore
 *
 */

#include "emu.h"

#include "bus/rs232/rs232.h"
#include "cpu/m68000/m68000.h"
#include "machine/68230pit.h"
#include "machine/6850acia.h"
#include "machine/mc14411.h"


class mex68kecb_state : public driver_device
{
public:
	mex68kecb_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_pit(*this, "pit")
		, m_brg(*this, "brg")
		, m_acia1(*this, "acia1")
		, m_acia2(*this, "acia2")
		, m_acia1_baud(*this, "ACIA1_BAUD")
		, m_acia2_baud(*this, "ACIA2_BAUD")
		, m_terminal(*this, "terminal")
		, m_host(*this, "host")
	{ }

	void mex68kecb(machine_config &config);

private:
	required_device<cpu_device> m_maincpu;
	required_device<pit68230_device> m_pit;
	required_device<mc14411_device> m_brg;
	required_device<acia6850_device> m_acia1;
	required_device<acia6850_device> m_acia2;
	required_ioport m_acia1_baud;
	required_ioport m_acia2_baud;

	required_device<rs232_port_device> m_terminal;
	required_device<rs232_port_device> m_host;

	virtual void machine_start() override;
	virtual void machine_reset() override;
	void mem_map(address_map &map);

	// Clocks from Baud Rate Generator
	template <u8 bit> void write_acia_clock(int state);

	// Pointer to System ROMs needed by bootvect_r and masking RAM buffer for post reset accesses
	uint16_t *m_sysrom = nullptr;
	uint16_t m_sysram[8]{};
	uint16_t bootvect_r(offs_t offset);
	void bootvect_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
};

/* Input ports */
static INPUT_PORTS_START( mex68kecb )
	PORT_START("ACIA1_BAUD")
	PORT_DIPNAME(0xff, 0x80, "Terminal Baud Rate")
	PORT_DIPSETTING(0x80, "9600") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x40, "4800") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x20, "2400") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x10, "1200") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x08,  "600") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x04,  "300") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x02,  "150") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x01,  "110") PORT_DIPLOCATION("J10:8,7,6,5,4,3,2,1")

	PORT_START("ACIA2_BAUD")
	PORT_DIPNAME(0xff, 0x80, "Host Baud Rate")
	PORT_DIPSETTING(0x80, "9600") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x40, "4800") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x20, "2400") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x10, "1200") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x08,  "600") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x04,  "300") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x02,  "150") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
	PORT_DIPSETTING(0x01,  "110") PORT_DIPLOCATION("J9:8,7,6,5,4,3,2,1")
INPUT_PORTS_END


void mex68kecb_state::mex68kecb(machine_config &config)
{
	M68000(config, m_maincpu, 8_MHz_XTAL / 2);
	m_maincpu->set_addrmap(AS_PROGRAM, &mex68kecb_state::mem_map);

	// Set up BRG.

	MC14411(config, m_brg, 1.8432_MHz_XTAL);
	m_brg->out_f<1>().set(FUNC(mex68kecb_state::write_acia_clock<7>));  // 9600bps
	m_brg->out_f<3>().set(FUNC(mex68kecb_state::write_acia_clock<6>));  // 4800bps
	m_brg->out_f<5>().set(FUNC(mex68kecb_state::write_acia_clock<5>));  // 2400bps
	m_brg->out_f<7>().set(FUNC(mex68kecb_state::write_acia_clock<4>));  // 1200bps
	m_brg->out_f<8>().set(FUNC(mex68kecb_state::write_acia_clock<3>));  //  600bps
	m_brg->out_f<9>().set(FUNC(mex68kecb_state::write_acia_clock<2>));  //  300bps
	m_brg->out_f<11>().set(FUNC(mex68kecb_state::write_acia_clock<1>)); //  150bps
	m_brg->out_f<13>().set(FUNC(mex68kecb_state::write_acia_clock<0>)); //  110bps

	// Set up PIT and ACIAs.

	PIT68230(config, m_pit, 8_MHz_XTAL / 2);
	ACIA6850(config, m_acia1);
	ACIA6850(config, m_acia2);

	// Set up interrupts.

	// Nothing at IRQ1
	m_pit->timer_irq_callback().set_inputline("maincpu", M68K_IRQ_2);
	m_pit->port_irq_callback().set_inputline("maincpu", M68K_IRQ_3);
	// Optional 6800 peripherals at IRQ4
	m_acia1->irq_handler().set_inputline("maincpu", M68K_IRQ_5);
	m_acia2->irq_handler().set_inputline("maincpu", M68K_IRQ_6);
	// ABORT Button at IRQ7

	// Set up terminal RS-232.

	RS232_PORT(config, m_terminal, default_rs232_devices, "terminal");
	m_terminal->rxd_handler().set(m_acia1, FUNC(acia6850_device::write_rxd));
	m_terminal->cts_handler().set(m_acia1, FUNC(acia6850_device::write_cts));
	m_terminal->dcd_handler().set(m_acia1, FUNC(acia6850_device::write_dcd));
	m_acia1->txd_handler().set(m_terminal, FUNC(rs232_port_device::write_txd));
	m_acia1->rts_handler().set(m_terminal, FUNC(rs232_port_device::write_rts));

	// Set up host RS-232.

	RS232_PORT(config, m_host, default_rs232_devices, nullptr);
	m_host->rxd_handler().set(m_acia2, FUNC(acia6850_device::write_rxd));
	m_host->cts_handler().set(m_acia2, FUNC(acia6850_device::write_cts));
	m_host->dcd_handler().set(m_acia2, FUNC(acia6850_device::write_dcd));
	m_acia2->txd_handler().set(m_host, FUNC(rs232_port_device::write_txd));
	m_acia2->rts_handler().set(m_host, FUNC(rs232_port_device::write_rts));
}

void mex68kecb_state::mem_map(address_map &map)
{
	map.unmap_value_high();
	map(0x000000, 0x000007).ram().w(FUNC(mex68kecb_state::bootvect_w));       /* After first write we act as RAM */
	map(0x000000, 0x000007).rom().r(FUNC(mex68kecb_state::bootvect_r));       /* ROM mirror just during reset */
	map(0x000008, 0x007fff).ram(); /* 32KB RAM */
	map(0x008000, 0x00bfff).rom().region("roms", 0); /* 16KB ROM */
	map(0x010000, 0x01003f).rw("pit", FUNC(pit68230_device::read), FUNC(pit68230_device::write)).umask16(0x00ff);
	map(0x010040, 0x010043).rw("acia1", FUNC(acia6850_device::read), FUNC(acia6850_device::write)).umask16(0xff00);
	map(0x010040, 0x010043).rw("acia2", FUNC(acia6850_device::read), FUNC(acia6850_device::write)).umask16(0x00ff);
}

void mex68kecb_state::machine_start()
{
	/* Setup pointer to bootvector in ROM for bootvector handler bootvect_r */
	m_sysrom = (uint16_t *)(memregion("roms")->base());
}

void mex68kecb_state::machine_reset()
{
	// Reset BRG.
	m_brg->rsa_w(CLEAR_LINE);
	m_brg->rsb_w(ASSERT_LINE);

	/* Reset pointer to bootvector in ROM for bootvector handler bootvect_r */
	if (m_sysrom == &m_sysram[0]) /* Condition needed because memory map is not setup first time */
		m_sysrom = (uint16_t*)(memregion("roms")->base());
}

template <u8 bit>
void mex68kecb_state::write_acia_clock(int state)
{
	if (BIT(m_acia1_baud->read(), bit)) {
		m_acia1->write_txc(state);
		m_acia1->write_rxc(state);
	}

	if (BIT(m_acia2_baud->read(), bit)) {
		m_acia2->write_txc(state);
		m_acia2->write_rxc(state);
	}
}

/* Boot vector handler, the PCB hardwires the first 16 bytes from 0xfc0000 to 0x0 at reset. */
uint16_t mex68kecb_state::bootvect_r(offs_t offset) {
	return m_sysrom[offset];
}

void mex68kecb_state::bootvect_w(offs_t offset, uint16_t data, uint16_t mem_mask) {
	m_sysram[offset % std::size(m_sysram)] &= ~mem_mask;
	m_sysram[offset % std::size(m_sysram)] |= (data & mem_mask);
	m_sysrom = &m_sysram[0]; // redirect all upcoming accesses to masking RAM until reset.
}


/* ROM definition */
ROM_START( mex68kecb )
	ROM_REGION16_BE(0x4000, "roms", ROMREGION_ERASE00)
	ROM_DEFAULT_BIOS("tutor13")

	ROM_SYSTEM_BIOS(0, "tutor13", "Motorola TUTOR 1.3")
	ROMX_LOAD("tutor13u.bin", 0x000000, 0x002000, CRC(7d11a0e9) SHA1(18ec8899651e78301b406f4fe6d4141c853e9e30), ROM_SKIP(1) | ROM_BIOS(0) )
	ROMX_LOAD("tutor13l.bin", 0x000001, 0x002000, CRC(2bb3a4e2) SHA1(3dac64ec5af4f46a367959ec80677103e3822f20), ROM_SKIP(1) | ROM_BIOS(0) )
ROM_END


/* Driver */
/*    YEAR  NAME       PARENT  COMPAT  MACHINE    INPUT      CLASS            INIT        COMPANY     FULLNAME            FLAGS */
COMP( 1981, mex68kecb, 0,      0,      mex68kecb, mex68kecb, mex68kecb_state, empty_init, "Motorola", "Motorola 68K ECB", MACHINE_NO_SOUND_HW )
