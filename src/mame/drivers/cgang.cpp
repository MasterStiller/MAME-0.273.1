// license:BSD-3-Clause
// copyright-holders:hap
/******************************************************************************

『コズモギャングス』 (COSMOGANGS) by Namco, 1990. USA distribution was handled by
Data East, they titled it "Cosmo Gang".

It is an electromechanical arcade lightgun game with ticket redemption.
There is no screen, feedback is with motorized elements, lamps and 7segs,
and of course sounds and music.

TODO:
- everything

Hardware notes:

Main CPU side:
- HD6809P @ 4MHz
- 32KB ROM(27C256), 8KB RAM(HM6264AP-10)
- 4*M5L8255AP-5 PPI, 2*M5L8253P-5 PIT
- 5*MB8713 motor drivers

Audio CPU side:
- HD68B09EP @ 2MHz (8MHz XTAL)
- 32KB ROM(27C256), 16KB RAM(2*HM6264AP-10, some pins N/C)
- M5L8255AP-5 PPI
- Namco CUS121 sound interface, same chip used in Namco System 1
- Yamaha YM2151 @ 3.57MHz, 2*NEC D7759C @ 640kHz
- 2*128KB ADPCM ROM (27C010, one for each D7759C)

Cabinet:
- 5 lanes with movable aliens, lightsensor under mouth
- 5 'energy containers', aliens will try to steal them
- 2 lightguns
- UFO with leds above cabinet
- 7segs for scorekeeping
- 2 ticket dispensers

******************************************************************************/

#include "emu.h"
#include "cpu/m6809/m6809.h"
#include "machine/gen_latch.h"
#include "machine/i8255.h"
#include "machine/pit8253.h"
#include "machine/ripple_counter.h"
#include "sound/upd7759.h"
#include "sound/ym2151.h"
#include "speaker.h"


namespace {

class cgang_state : public driver_device
{
public:
	cgang_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_latch(*this, "latch%u", 0),
		m_pit(*this, "pit%u", 0),
		m_ppi(*this, "ppi%u", 0),
		m_upd(*this, "adpcm%u", 0),
		m_ymsnd(*this, "ymsnd")
	{ }

	// machine drivers
	void cgang(machine_config &config);

protected:
	virtual void machine_start() override;

private:
	// devices/pointers
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device_array<generic_latch_8_device, 2> m_latch;
	required_device_array<pit8253_device, 2> m_pit;
	required_device_array<i8255_device, 5> m_ppi;
	required_device_array<upd7759_device, 2> m_upd;
	required_device<ym2151_device> m_ymsnd;

	// address maps
	void main_map(address_map &map);
	void sound_map(address_map &map);

	// I/O handlers
	DECLARE_WRITE_LINE_MEMBER(main_irq_w);
	DECLARE_WRITE_LINE_MEMBER(main_firq_w);
	DECLARE_WRITE8_MEMBER(main_irq_clear_w) { m_maincpu->set_input_line(M6809_IRQ_LINE, CLEAR_LINE); }
	DECLARE_WRITE8_MEMBER(main_firq_clear_w) { m_maincpu->set_input_line(M6809_FIRQ_LINE, CLEAR_LINE); }
	template<int N> DECLARE_WRITE8_MEMBER(motor_clock_w);

	DECLARE_READ8_MEMBER(ppi0_c_r);

	template<int N> DECLARE_WRITE8_MEMBER(adpcm_w);
	DECLARE_WRITE8_MEMBER(spot_w);

	DECLARE_WRITE8_MEMBER(ppi4_a_w);
	DECLARE_READ8_MEMBER(ppi4_c_r);

	int m_main_irq = 0;
	int m_main_firq = 0;
};

void cgang_state::machine_start()
{
	// register for savestates
	save_item(NAME(m_main_irq));
	save_item(NAME(m_main_firq));
}



/******************************************************************************
    I/O
******************************************************************************/

// maincpu

WRITE_LINE_MEMBER(cgang_state::main_irq_w)
{
	// irq on rising edge
	if (state && !m_main_irq)
		m_maincpu->set_input_line(M6809_IRQ_LINE, ASSERT_LINE);

	m_main_irq = state;
}

WRITE_LINE_MEMBER(cgang_state::main_firq_w)
{
	// firq on rising edge
	if (state && !m_main_firq)
		m_maincpu->set_input_line(M6809_FIRQ_LINE, ASSERT_LINE);

	m_main_firq = state;
}

template<int N>
WRITE8_MEMBER(cgang_state::motor_clock_w)
{
}

READ8_MEMBER(cgang_state::ppi0_c_r)
{
	u8 data = 0;

	// PC7: CALL-CPU1
	data |= m_latch[1]->pending_r() ? 0 : 0x80;

	return data;
}


// audiocpu

template<int N>
WRITE8_MEMBER(cgang_state::adpcm_w)
{
	m_upd[N]->port_w(data);

	// also strobes start
	m_upd[N]->start_w(0);
	m_upd[N]->start_w(1);
}

WRITE8_MEMBER(cgang_state::spot_w)
{
}

WRITE8_MEMBER(cgang_state::ppi4_a_w)
{
	// PA0,PA1: ADPCM reset
	m_upd[0]->reset_w(BIT(data, 0));
	m_upd[1]->reset_w(BIT(data, 1));
}

READ8_MEMBER(cgang_state::ppi4_c_r)
{
	u8 data = 0;

	// PC0,PC1: ADPCM busy
	data |= m_upd[0]->busy_r() ? 1 : 0;
	data |= m_upd[1]->busy_r() ? 2 : 0;

	// PC2: CALL-CPU2
	data |= m_latch[0]->pending_r() ? 0 : 4;

	return data;
}



/******************************************************************************
    Address Maps
******************************************************************************/

void cgang_state::main_map(address_map &map)
{
	map(0x0000, 0x1fff).ram();
	map(0x2000, 0x2003).rw(m_ppi[0], FUNC(i8255_device::read), FUNC(i8255_device::write));
	map(0x2004, 0x2007).rw(m_ppi[1], FUNC(i8255_device::read), FUNC(i8255_device::write));
	map(0x2008, 0x200b).rw(m_ppi[2], FUNC(i8255_device::read), FUNC(i8255_device::write));
	map(0x200c, 0x200f).rw(m_ppi[3], FUNC(i8255_device::read), FUNC(i8255_device::write));
	map(0x4000, 0x4000).mirror(0x0003).w(m_latch[0], FUNC(generic_latch_8_device::write));
	map(0x4004, 0x4004).mirror(0x0003).r(m_latch[1], FUNC(generic_latch_8_device::read));
	map(0x4008, 0x4008).mirror(0x0003).w(FUNC(cgang_state::main_irq_clear_w));
	map(0x400c, 0x400c).mirror(0x0003).w(FUNC(cgang_state::main_firq_clear_w));
	map(0x4010, 0x4013).rw(m_pit[0], FUNC(pit8253_device::read), FUNC(pit8253_device::write));
	map(0x4014, 0x4017).rw(m_pit[1], FUNC(pit8253_device::read), FUNC(pit8253_device::write));
	map(0x8000, 0xffff).rom();
}

void cgang_state::sound_map(address_map &map)
{
	map(0x0000, 0x0fff).ram();
	map(0x1000, 0x1001).rw(m_ymsnd, FUNC(ym2151_device::status_r), FUNC(ym2151_device::write));
	map(0x2000, 0x2003).mirror(0x0ffc).rw(m_ppi[4], FUNC(i8255_device::read), FUNC(i8255_device::write));
	map(0x3000, 0x3000).mirror(0x0fff).r(m_latch[0], FUNC(generic_latch_8_device::read));
	map(0x4000, 0x4000).mirror(0x0fff).w(m_latch[1], FUNC(generic_latch_8_device::write));
	map(0x5000, 0x5000).mirror(0x0fff).w(FUNC(cgang_state::adpcm_w<0>));
	map(0x6000, 0x6000).mirror(0x0fff).w(FUNC(cgang_state::adpcm_w<1>));
	map(0x7000, 0x7000).mirror(0x0fff).w(FUNC(cgang_state::spot_w)).nopr();
	map(0x8000, 0xffff).rom();
}



/******************************************************************************
    Input Ports
******************************************************************************/

static INPUT_PORTS_START( cgang )
INPUT_PORTS_END



/******************************************************************************
    Machine Drivers
******************************************************************************/

void cgang_state::cgang(machine_config &config)
{
	/* basic machine hardware */
	MC6809(config, m_maincpu, 4_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &cgang_state::main_map);

	MC6809E(config, m_audiocpu, 8_MHz_XTAL/4);
	m_audiocpu->set_addrmap(AS_PROGRAM, &cgang_state::sound_map);
	m_audiocpu->set_periodic_int(FUNC(cgang_state::nmi_line_pulse), attotime::from_hz(8_MHz_XTAL/4 / 0x1000));

	PIT8253(config, m_pit[0], 0);
	m_pit[0]->set_clk<0>(4_MHz_XTAL/4);
	m_pit[0]->set_clk<1>(4_MHz_XTAL/4);
	m_pit[0]->set_clk<2>(4_MHz_XTAL/4);
	m_pit[0]->out_handler<0>().set(FUNC(cgang_state::motor_clock_w<0>));
	m_pit[0]->out_handler<1>().set(FUNC(cgang_state::motor_clock_w<1>));
	m_pit[0]->out_handler<2>().set(FUNC(cgang_state::motor_clock_w<2>));

	PIT8253(config, m_pit[1], 0);
	m_pit[1]->set_clk<0>(4_MHz_XTAL/4);
	m_pit[1]->set_clk<1>(4_MHz_XTAL/4);
	m_pit[1]->set_clk<2>(4_MHz_XTAL/4);
	m_pit[1]->out_handler<0>().set(FUNC(cgang_state::motor_clock_w<3>));
	m_pit[1]->out_handler<1>().set(FUNC(cgang_state::motor_clock_w<4>));
	m_pit[1]->out_handler<2>().set("int_clk", FUNC(ripple_counter_device::clock_w));

	ripple_counter_device &int_clk(RIPPLE_COUNTER(config, "int_clk")); // 4040
	int_clk.set_stages(12);
	int_clk.count_out_cb().set_inputline(m_maincpu, INPUT_LINE_NMI).bit(0);
	int_clk.count_out_cb().append(FUNC(cgang_state::main_irq_w)).bit(3);
	int_clk.count_out_cb().append(FUNC(cgang_state::main_firq_w)).bit(4);

	GENERIC_LATCH_8(config, m_latch[0]);
	GENERIC_LATCH_8(config, m_latch[1]);

	I8255(config, m_ppi[0]); // 0x9b: all = input
	m_ppi[0]->in_pa_callback().set_constant(0);
	m_ppi[0]->in_pb_callback().set_constant(0);
	m_ppi[0]->in_pc_callback().set(FUNC(cgang_state::ppi0_c_r));

	I8255(config, m_ppi[1]); // 0x9a: A & B = input, Clow = output, Chigh = input
	m_ppi[1]->in_pa_callback().set_constant(0);
	m_ppi[1]->in_pb_callback().set_constant(0);
	m_ppi[1]->in_pc_callback().set_constant(0);

	I8255(config, m_ppi[2]); // 0x80: all = output

	I8255(config, m_ppi[3]); // 0x80: all = output

	I8255(config, m_ppi[4]); // 0x89: A & B = output, C = input
	m_ppi[4]->out_pa_callback().set(FUNC(cgang_state::ppi4_a_w));
	m_ppi[4]->in_pc_callback().set(FUNC(cgang_state::ppi4_c_r));

	/* sound hardware */
	SPEAKER(config, "mono").front_center();

	YM2151(config, m_ymsnd, 3.579545_MHz_XTAL);
	m_ymsnd->add_route(ALL_OUTPUTS, "mono", 0.5);

	UPD7759(config, m_upd[0], 640_kHz_XTAL);
	m_upd[0]->add_route(ALL_OUTPUTS, "mono", 0.5);
	UPD7759(config, m_upd[1], 640_kHz_XTAL);
	m_upd[1]->add_route(ALL_OUTPUTS, "mono", 0.5);
}



/******************************************************************************
    ROM Definitions
******************************************************************************/

ROM_START( cgang )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD("cg1_mp0d.4j", 0x8000, 0x8000, CRC(2114cb55) SHA1(4e330cb3d8d96ec06faa25cbaeed97b1c2eff8db) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD("cg1_sp0b.4b", 0x8000, 0x8000, CRC(62974140) SHA1(5eee3f6345521e3fb76acb3acaa5c9df75db91db) )

	ROM_REGION( 0x20000, "adpcm0", 0 )
	ROM_LOAD( "9c", 0x00000, 0x20000, CRC(f9a3f8a0) SHA1(5ad8b408d36397227019afd15c3516f85488c6df) )

	ROM_REGION( 0x20000, "adpcm1", 0 )
	ROM_LOAD( "9e", 0x00000, 0x20000, CRC(40e7f60b) SHA1(af641b0562db1ae033cee67df583d178fd8c93f3) )
ROM_END

} // anonymous namespace



/******************************************************************************
    Drivers
******************************************************************************/

/*    YEAR  NAME   PARENT  MACHINE  INPUT  CLASS        INIT        MONITOR  COMPANY, FULLNAME, FLAGS */
GAME( 1990, cgang, 0,      cgang,   cgang, cgang_state, empty_init, ROT0,    "Namco (Data East license)", "Cosmo Gang (US)", MACHINE_MECHANICAL | MACHINE_NOT_WORKING )
