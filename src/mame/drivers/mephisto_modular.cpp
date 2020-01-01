// license:LGPL-2.1+
// copyright-holders:Dirk Verwiebe, Cowering, Sandro Ronco, hap
/******************************************************************************

Hegener + Glaser Mephisto chesscomputers with plugin modules
3rd generation (2nd gen is Glasgow/Amsterdam, 1st gen is MM series)

After Roma, H+G started naming the different versions 16 Bit/32 Bit instead of 68000/68020.
With Genius and the TM versions, they still applied "68030".

Almeria 16 Bit 12MHz
Almeria 32 Bit 12MHz
Portorose 16 Bit 12MHz
Portorose 32 Bit 12MHz
Lyon 16 Bit 12MHz
Lyon 32 Bit 12MHz
Vancouver 16 Bit 12MHz
Vancouver 32 Bit 12MHz
Genius 68030 33.3330MHz

The London program (1994 competition) is not a dedicated module, but an EPROM upgrade
released by Richard Lang for Almeria, Lyon, Portorose and Vancouver modules, and also
available as upgrades for Berlin/Berlin Pro and Genius.
No Mephisto modules were released anymore after Saitek took over H+G, engine is assumed
to be same as Saitek's 1996 Mephisto London 68030 (limited release TM version).

TODO:
- add Bavaria sensor support
- add the missing very rare 'TM' Tournament Machines
- match I/S= diag speed test with real hardware (good test for proper waitstates)
- remove gen32/gen32l ROM patch

Undocumented buttons:
- holding ENTER and LEFT cursor on boot runs diagnostics
- holding UP and RIGHT cursor on boot will clear the battery backed RAM

Bavaria piece recognition board:
-------------------------------------------------
|                                               |
| 74HC21                      74HC74    74HC238 |
| 74HC4040   74HC574          74HC173   74HC374 |
| ROM                  XTAL   74HC368   74HC374 |
| 74HC4024   74HC32           74HC139   74HC374 |
|                                               |
-------------------------------------------------
XTAL = 7.37280MHz
ROM = TC57256AD-12, sinus table

Only usable with Weltmeister modules, Portorose until London (aka this MAME driver)
Also, it was patented with DE4207534.

Each piece has a Tank circuit, and in each square of the board there is a coil.
By scanning all the squares at different frequencies, the resonance frequency
of every piece is obtained in order to identify it.

Coil resonance frequency:
wJ,  bJ,  wK,  bK,  wQ,  bQ,  wP,  bP,  wB,  bB,  wN,  bN,  wR,  bR  (J = Joker)
460, 421, 381, 346, 316, 289, 259, 238, 217, 203, 180, 167, 154, 138 kHz
14,  13,  12,  11,  10,  9,   8,   7,   6,   5,   4,   3,   2,   1   piece ID

******************************************************************************/

#include "emu.h"

#include "cpu/m68000/m68000.h"
#include "machine/bankdev.h"
#include "machine/nvram.h"
#include "machine/timer.h"
#include "machine/sensorboard.h"
#include "machine/mmboard.h"
#include "video/pwm.h"

#include "screen.h"
#include "speaker.h"

// internal artwork
#include "mephisto_alm16.lh" // clickable
#include "mephisto_alm32.lh" // clickable
#include "mephisto_gen32.lh" // clickable


namespace {

class mmodular_state : public driver_device
{
public:
	mmodular_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_board(*this, "board"),
		m_led_pwm(*this, "led_pwm"),
		m_bav_busy(*this, "bav_busy"),
		m_led_out(*this, "led%u", 0U)
	{ }

	// machine configs
	void alm16(machine_config &config);
	void alm32(machine_config &config);
	void port16(machine_config &config);
	void port32(machine_config &config);
	void van16(machine_config &config);
	void van32(machine_config &config);
	void gen32(machine_config &config);

	void init_gen32();

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	// devices/pointers
	required_device<cpu_device> m_maincpu;
	required_device<sensorboard_device> m_board;
	required_device<pwm_display_device> m_led_pwm;
	required_device<timer_device> m_bav_busy;
	output_finder<64> m_led_out;

	// address maps
	void alm16_mem(address_map &map);
	void alm32_mem(address_map &map);
	void port16_mem(address_map &map);
	void port32_mem(address_map &map);
	void van16_mem(address_map &map);
	void van32_mem(address_map &map);
	void gen32_mem(address_map &map);
	void nvram_map(address_map &map);

	// I/O handlers
	DECLARE_WRITE8_MEMBER(mux_w);
	DECLARE_WRITE8_MEMBER(led_w);
	DECLARE_READ8_MEMBER(input_r);
	DECLARE_WRITE8_MEMBER(bavaria_w);
	DECLARE_READ8_MEMBER(bavaria1_r);
	DECLARE_READ8_MEMBER(bavaria2_r);

	u8 m_mux = 0;
	u8 m_led_data = 0;
	u8 m_bav_data = 0;
};

void mmodular_state::machine_start()
{
	m_led_out.resolve();

	// register for savestates
	save_item(NAME(m_mux));
	save_item(NAME(m_led_data));
	save_item(NAME(m_bav_data));
}

void mmodular_state::machine_reset()
{
	m_bav_data = 0;
}

void mmodular_state::init_gen32()
{
	// patch LCD delay loop
	uint8_t *rom = memregion("maincpu")->base();
	if (rom[0x870] == 0x0c && rom[0x871] == 0x78)
		rom[0x870] = 0x38;
}



/******************************************************************************
    I/O
******************************************************************************/

WRITE8_MEMBER(mmodular_state::mux_w)
{
	// d0-d7: input/led mux
	m_mux = ~data;
	m_led_pwm->matrix(m_mux, m_led_data);
}

WRITE8_MEMBER(mmodular_state::led_w)
{
	// d0-d7: led data
	m_led_data = data;
	m_led_pwm->matrix(m_mux, m_led_data);
}

READ8_MEMBER(mmodular_state::input_r)
{
	u8 data = 0;

	// read magnet sensors
	for (int i = 0; i < 8; i++)
		if (BIT(m_mux, i))
			data |= m_board->read_rank(i);

	return ~data;
}

WRITE8_MEMBER(mmodular_state::bavaria_w)
{
	// d0-d5: select square?
	// d6: no function?
	// d7: start search
	if (m_bav_data & ~data & 0x80)
		m_bav_busy->adjust(attotime::from_usec(3000));

	m_bav_data = data;
}

READ8_MEMBER(mmodular_state::bavaria1_r)
{
	// d0-d3: piece id
	// other: unused?
	return 0;
}

READ8_MEMBER(mmodular_state::bavaria2_r)
{
	return 0;

	// d7: busy signal
	// other: unused?
	return m_bav_busy->enabled() ? 0x80 : 0;
}



/******************************************************************************
    Address Maps
******************************************************************************/

void mmodular_state::nvram_map(address_map &map)
{
	// nvram is 8-bit (8KB)
	map(0x0000, 0x1fff).ram().share("nvram");
}

void mmodular_state::alm16_mem(address_map &map)
{
	map(0x000000, 0x01ffff).rom();
	map(0x400000, 0x47ffff).ram();
	map(0x800000, 0x803fff).m("nvram_map", FUNC(address_map_bank_device::amap8)).umask16(0xff00);
	map(0xc00000, 0xc00000).r(FUNC(mmodular_state::input_r));
	map(0xc80000, 0xc80000).w(FUNC(mmodular_state::mux_w));
	map(0xd00000, 0xd00000).w(FUNC(mmodular_state::led_w));
	map(0xd00000, 0xd00001).nopr(); // clr.b
	map(0xf00000, 0xf00003).portr("KEY1");
	map(0xf00004, 0xf00007).portr("KEY2");
	map(0xf00008, 0xf0000b).portr("KEY3");
	map(0xd80000, 0xd80000).w("display", FUNC(mephisto_display_modul_device::latch_w));
	map(0xd80008, 0xd80008).w("display", FUNC(mephisto_display_modul_device::io_w));
}

void mmodular_state::port16_mem(address_map &map)
{
	alm16_mem(map);

	map(0xe80002, 0xe80002).r(FUNC(mmodular_state::bavaria1_r));
	map(0xe80004, 0xe80004).w(FUNC(mmodular_state::bavaria_w));
	map(0xe80006, 0xe80006).r(FUNC(mmodular_state::bavaria2_r));
}

void mmodular_state::van16_mem(address_map &map)
{
	port16_mem(map);

	map(0x000000, 0x03ffff).rom();
}


void mmodular_state::alm32_mem(address_map &map)
{
	map(0x00000000, 0x0001ffff).rom();
	map(0x40000000, 0x400fffff).ram();
	map(0x800000ec, 0x800000ef).portr("KEY1");
	map(0x800000f4, 0x800000f7).portr("KEY2");
	map(0x800000f8, 0x800000fb).portr("KEY3");
	map(0x800000fc, 0x800000fc).r(FUNC(mmodular_state::input_r));
	map(0x88000000, 0x88000007).w(FUNC(mmodular_state::mux_w)).umask32(0xff000000);
	map(0x90000000, 0x90000007).w(FUNC(mmodular_state::led_w)).umask32(0xff000000);
	map(0xa0000000, 0xa0000000).w("display", FUNC(mephisto_display_modul_device::latch_w));
	map(0xa0000010, 0xa0000010).w("display", FUNC(mephisto_display_modul_device::io_w));
	map(0xa8000000, 0xa8007fff).m("nvram_map", FUNC(address_map_bank_device::amap8)).umask32(0xff000000);
}

void mmodular_state::port32_mem(address_map &map)
{
	alm32_mem(map);

	map(0x98000004, 0x98000004).r(FUNC(mmodular_state::bavaria1_r));
	map(0x98000008, 0x98000008).w(FUNC(mmodular_state::bavaria_w));
	map(0x9800000c, 0x9800000c).r(FUNC(mmodular_state::bavaria2_r));
}

void mmodular_state::van32_mem(address_map &map)
{
	port32_mem(map);

	map(0x00000000, 0x0003ffff).rom();
}


void mmodular_state::gen32_mem(address_map &map)
{
	map(0x00000000, 0x0003ffff).rom();
	map(0x40000000, 0x4007ffff).ram();
	map(0x80000000, 0x8003ffff).ram();
	map(0xc0000000, 0xc0000000).r(FUNC(mmodular_state::input_r));
	map(0xc8000004, 0xc8000004).w(FUNC(mmodular_state::mux_w));
	map(0xd0000004, 0xd0000004).w(FUNC(mmodular_state::led_w));
	map(0xd8000004, 0xd8000004).r(FUNC(mmodular_state::bavaria1_r));
	map(0xd8000008, 0xd8000008).w(FUNC(mmodular_state::bavaria_w));
	map(0xd800000c, 0xd800000c).r(FUNC(mmodular_state::bavaria2_r));
	map(0xe0000000, 0xe0000000).w("display", FUNC(mephisto_display_modul_device::latch_w));
	map(0xe0000010, 0xe0000010).w("display", FUNC(mephisto_display_modul_device::io_w));
	map(0xe8000000, 0xe8007fff).m("nvram_map", FUNC(address_map_bank_device::amap8)).umask32(0xff000000);
	map(0xf0000004, 0xf0000007).portr("KEY1");
	map(0xf0000008, 0xf000000b).portr("KEY2");
	map(0xf0000010, 0xf0000013).portr("KEY3");
}



/******************************************************************************
    Input Ports
******************************************************************************/

static INPUT_PORTS_START( alm16 )
	PORT_START("KEY1")
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYPAD)      PORT_NAME("LEFT")   PORT_CODE(KEYCODE_LEFT)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYPAD)      PORT_NAME("ENT")    PORT_CODE(KEYCODE_ENTER)

	PORT_START("KEY2")
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYPAD)      PORT_NAME("RIGHT")  PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYPAD)      PORT_NAME("UP")     PORT_CODE(KEYCODE_UP)

	PORT_START("KEY3")
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYPAD)      PORT_NAME("DOWN")   PORT_CODE(KEYCODE_DOWN)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYPAD)      PORT_NAME("CL")     PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL)
INPUT_PORTS_END

static INPUT_PORTS_START( alm32 )
	PORT_START("KEY1")
	PORT_BIT(0x4000, IP_ACTIVE_LOW, IPT_KEYPAD)       PORT_NAME("RIGHT")  PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT(0x8000, IP_ACTIVE_LOW, IPT_KEYPAD)       PORT_NAME("CL")     PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL)

	PORT_START("KEY2")
	PORT_BIT(0x4000, IP_ACTIVE_LOW, IPT_KEYPAD)       PORT_NAME("DOWN")   PORT_CODE(KEYCODE_DOWN)
	PORT_BIT(0x8000, IP_ACTIVE_LOW, IPT_KEYPAD)       PORT_NAME("UP")     PORT_CODE(KEYCODE_UP)

	PORT_START("KEY3")
	PORT_BIT(0x4000, IP_ACTIVE_LOW, IPT_KEYPAD)       PORT_NAME("LEFT")   PORT_CODE(KEYCODE_LEFT)
	PORT_BIT(0x8000, IP_ACTIVE_LOW, IPT_KEYPAD)       PORT_NAME("ENT")    PORT_CODE(KEYCODE_ENTER)
INPUT_PORTS_END

static INPUT_PORTS_START( gen32 )
	PORT_START("KEY1")
	PORT_BIT(0x01000000, IP_ACTIVE_HIGH, IPT_KEYPAD)  PORT_NAME("ENT")    PORT_CODE(KEYCODE_ENTER)
	PORT_BIT(0x02000000, IP_ACTIVE_HIGH, IPT_KEYPAD)  PORT_NAME("LEFT")   PORT_CODE(KEYCODE_LEFT)

	PORT_START("KEY2")
	PORT_BIT(0x01000000, IP_ACTIVE_HIGH, IPT_KEYPAD)  PORT_NAME("UP")     PORT_CODE(KEYCODE_UP)
	PORT_BIT(0x02000000, IP_ACTIVE_HIGH, IPT_KEYPAD)  PORT_NAME("DOWN")   PORT_CODE(KEYCODE_DOWN)

	PORT_START("KEY3")
	PORT_BIT(0x01000000, IP_ACTIVE_HIGH, IPT_KEYPAD)  PORT_NAME("CL")     PORT_CODE(KEYCODE_BACKSPACE) PORT_CODE(KEYCODE_DEL)
	PORT_BIT(0x02000000, IP_ACTIVE_HIGH, IPT_KEYPAD)  PORT_NAME("RIGHT")  PORT_CODE(KEYCODE_RIGHT)
INPUT_PORTS_END



/******************************************************************************
    Machine Configs
******************************************************************************/

void mmodular_state::alm16(machine_config &config)
{
	/* basic machine hardware */
	M68000(config, m_maincpu, 12_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &mmodular_state::alm16_mem);
	m_maincpu->set_periodic_int(FUNC(mmodular_state::irq2_line_hold), attotime::from_hz(600));

	NVRAM(config, "nvram", nvram_device::DEFAULT_ALL_0);
	ADDRESS_MAP_BANK(config, "nvram_map").set_map(&mmodular_state::nvram_map).set_options(ENDIANNESS_BIG, 8, 13);

	TIMER(config, "bav_busy").configure_generic(nullptr);

	SENSORBOARD(config, m_board).set_type(sensorboard_device::MAGNETS);
	m_board->init_cb().set(m_board, FUNC(sensorboard_device::preset_chess));
	m_board->set_delay(attotime::from_msec(150));

	/* video hardware */
	MEPHISTO_DISPLAY_MODUL(config, "display");
	PWM_DISPLAY(config, m_led_pwm).set_size(8, 8);
	m_led_pwm->output_x().set([this](offs_t offset, u8 data) { m_led_out[offset >> 6 | (offset & 7) << 3] = data; });
	config.set_default_layout(layout_mephisto_alm16);
}

void mmodular_state::port16(machine_config &config)
{
	alm16(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &mmodular_state::port16_mem);
}

void mmodular_state::van16(machine_config &config)
{
	port16(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &mmodular_state::van16_mem);
}

void mmodular_state::alm32(machine_config &config)
{
	alm16(config);

	/* basic machine hardware */
	M68020(config.replace(), m_maincpu, 12_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &mmodular_state::alm32_mem);
	m_maincpu->set_periodic_int(FUNC(mmodular_state::irq6_line_hold), attotime::from_hz(750));

	config.set_default_layout(layout_mephisto_alm32);
}

void mmodular_state::port32(machine_config &config)
{
	alm32(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &mmodular_state::port32_mem);
}

void mmodular_state::van32(machine_config &config)
{
	port32(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &mmodular_state::van32_mem);
}

void mmodular_state::gen32(machine_config &config)
{
	van32(config);

	/* basic machine hardware */
	M68EC030(config.replace(), m_maincpu, 33.333_MHz_XTAL); // M68EC030RP40B
	m_maincpu->set_addrmap(AS_PROGRAM, &mmodular_state::gen32_mem);

	const attotime irq_period = attotime::from_hz(6.144_MHz_XTAL / 0x4000); // through 4060, 375Hz
	m_maincpu->set_periodic_int(FUNC(mmodular_state::irq2_line_hold), irq_period);

	config.set_default_layout(layout_mephisto_gen32);
}



/******************************************************************************
    ROM Definitions
******************************************************************************/

ROM_START( alm16 )
	ROM_REGION16_BE( 0x20000, "maincpu", 0 )
	ROM_LOAD16_BYTE("alm16eve.bin", 0x00000, 0x10000, CRC(ee5b6ec4) SHA1(30920c1b9e16ffae576da5afa0b56da59ada3dbb) )
	ROM_LOAD16_BYTE("alm16odd.bin", 0x00001, 0x10000, CRC(d0be4ee4) SHA1(d36c074802d2c9099cd44e75f9de3fc7d1fd9908) )
ROM_END

ROM_START( alm32 )
	ROM_REGION32_BE( 0x20000, "maincpu", 0 )
	ROM_LOAD("alm32.bin", 0x00000, 0x20000, CRC(38f4b305) SHA1(43459a057ff29248c74d656a036ac325202b9c15) )
ROM_END

ROM_START( port16 )
	ROM_REGION16_BE( 0x20000, "maincpu", 0 )
	ROM_LOAD16_BYTE("port16ev.bin", 0x00000, 0x0d000, CRC(88f627d9) SHA1(8de93628d0c5bf9a2901750a7a05c5942cbf2601) )
	ROM_LOAD16_BYTE("port16od.bin", 0x00001, 0x0d000, CRC(7b0d4228) SHA1(9186fd512eab9a663b2b506a3b7a1eeeb09fc7d8) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( port32 )
	ROM_REGION32_BE( 0x20000, "maincpu", 0 )
	ROM_SYSTEM_BIOS( 0, "v103", "V1.03" )
	ROMX_LOAD("portorose_32bit_v103", 0x00000, 0x20000, CRC(02c091b3) SHA1(f1d48e73b24093288dbb8a06617bb62420c07508), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS( 1, "v101", "V1.01" )
	ROMX_LOAD("portorose_32bit_v101", 0x00000, 0x20000, CRC(405bd668) SHA1(8c6eacff7f6784fa1d38344d594c7e52ac828a23), ROM_BIOS(1) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( lyon16 )
	ROM_REGION16_BE( 0x20000, "maincpu", 0 )
	ROM_LOAD16_BYTE("lyon16ev.bin", 0x00000, 0x10000, CRC(497bd41a) SHA1(3ffefeeac694f49997c10d248ec6a7aa932898a4) )
	ROM_LOAD16_BYTE("lyon16od.bin", 0x00001, 0x10000, CRC(f9de3f54) SHA1(4060e29566d2f40122ccde3c1f84c94a9c1ed54f) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( lyon32 )
	ROM_REGION32_BE( 0x20000, "maincpu", 0 )
	ROM_LOAD("lyon32.bin", 0x00000, 0x20000, CRC(5c128b06) SHA1(954c8f0d3fae29900cb1e9c14a41a9a07a8e185f) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( van16 )
	ROM_REGION16_BE( 0x40000, "maincpu", 0 )
	ROM_LOAD16_BYTE("va16even.bin", 0x00000, 0x20000, CRC(e87602d5) SHA1(90cb2767b4ae9e1b265951eb2569b9956b9f7f44) )
	ROM_LOAD16_BYTE("va16odd.bin",  0x00001, 0x20000, CRC(585f3bdd) SHA1(90bb94a12d3153a91e3760020e1ea2a9eaa7ec0a) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( van32 )
	ROM_REGION32_BE( 0x40000, "maincpu", 0 )
	ROM_LOAD("vanc32.bin", 0x00000, 0x40000, CRC(f872beb5) SHA1(9919f207264f74e2b634b723b048ae9ca2cefbc7) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( gen32 )
	ROM_REGION32_BE( 0x40000, "maincpu", 0 )
	ROM_SYSTEM_BIOS( 0, "v401", "V4.01" )
	ROMX_LOAD("gen32_41.bin", 0x00000, 0x40000, CRC(ea9938c0) SHA1(645cf0b5b831b48104ad6cec8d78c63dbb6a588c), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS( 1, "v400", "V4.00" )
	ROMX_LOAD("gen32_4.bin",  0x00000, 0x40000, CRC(6cc4da88) SHA1(ea72acf9c67ed17c6ac8de56a165784aa629c4a1), ROM_BIOS(1) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( gen32l )
	ROM_REGION32_BE( 0x40000, "maincpu", 0 )
	ROM_LOAD("gen32l.bin", 0x00000, 0x40000, CRC(853baa4e) SHA1(946951081d4e91e5bdd9e93d0769568a7fe79bad) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( lond16 )
	ROM_REGION16_BE( 0x40000, "maincpu", 0 )
	ROM_LOAD16_BYTE("london_program_68000_module_even", 0x00000, 0x20000, CRC(68cfc2de) SHA1(93b551180f01f8ed6991c082795cd9ead922179a) )
	ROM_LOAD16_BYTE("london_program_68000_module_odd",  0x00001, 0x20000, CRC(2d75e2cf) SHA1(2ec9222c95f4be9667fb3b4be1b6f90fd4ad11c4) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

ROM_START( lond32 )
	ROM_REGION32_BE( 0x40000, "maincpu", 0 )
	ROM_LOAD("london_program_68020_module", 0x00000, 0x40000, CRC(3225b8da) SHA1(fd8f6f4e9c03b6cdc86d8405e856c26041bfad12) )

	ROM_REGION( 0x8000, "bavaria", 0 )
	ROM_LOAD( "sinus_15_bavaria", 0x0000, 0x8000, CRC(84421306) SHA1(5aab13bf38d80a4233c11f6eb5657f2749c14547) )
ROM_END

} // anonymous namespace



/******************************************************************************
    Drivers
******************************************************************************/

/*    YEAR  NAME     PARENT   COMPAT  MACHINE  INPUT   CLASS           INIT        COMPANY             FULLNAME                     FLAGS */
CONS( 1988, alm32,   0,       0,      alm32,   alm32,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Almeria 32 Bit",   MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1988, alm16,   alm32,   0,      alm16,   alm16,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Almeria 16 Bit",   MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1989, port32,  0,       0,      port32,  alm32,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Portorose 32 Bit", MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1989, port16,  port32,  0,      port16,  alm16,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Portorose 16 Bit", MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1990, lyon32,  0,       0,      port32,  alm32,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Lyon 32 Bit",      MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1990, lyon16,  lyon32,  0,      port16,  alm16,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Lyon 16 Bit",      MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1991, van32,   0,       0,      van32,   alm32,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Vancouver 32 Bit", MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1991, van16,   van32,   0,      van16,   alm16,  mmodular_state, empty_init, "Hegener + Glaser", "Mephisto Vancouver 16 Bit", MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1993, gen32,   0,       0,      gen32,   gen32,  mmodular_state, init_gen32, "Hegener + Glaser", "Mephisto Genius 68030",     MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1996, gen32l,  gen32,   0,      gen32,   gen32,  mmodular_state, init_gen32, "Richard Lang",     "Mephisto Genius 68030 (London upgrade)", MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK )
CONS( 1996, lond32,  0,       0,      van32,   alm32,  mmodular_state, empty_init, "Richard Lang",     "Mephisto London 32 Bit",    MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK ) // for alm32/port32/lyon32/van32
CONS( 1996, lond16,  lond32,  0,      van16,   alm16,  mmodular_state, empty_init, "Richard Lang",     "Mephisto London 16 Bit",    MACHINE_SUPPORTS_SAVE | MACHINE_IMPERFECT_TIMING | MACHINE_CLICKABLE_ARTWORK ) // for alm16/port16/lyon16/van16
