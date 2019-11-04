// license:GPL-2.0+
// copyright-holders:Peter Trauner
/* TimeTop - GameKing */
/*
  PeT mess@utanet.at 2015

  Thanks to Deathadder, Judge, Porchy, Klaus Sommer, James Brolly & Brian Provinciano

  hopefully my work (reverse engineerung, cartridge+bios backup, emulation) will be honored in future
  and my name will not be removed entirely, especially by simple code rewrites of working emulation

  flashcard, handheld, programmer, assembler ready to do some test on real hardware

  todo:
  !back up gameking3 bios so emulation of gameking3 gets possible; my gameking bios backup solution should work
  (improove emulation)
  (add audio)

  use gameking3 cartridge to get illegal cartridge scroller
*/

#include "emu.h"
#include "cpu/m6502/st2204.h"
#include "bus/generic/slot.h"
#include "bus/generic/carts.h"
#include "emupal.h"
#include "screen.h"
#include "softlist.h"


class gameking_state : public driver_device
{
public:
	gameking_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_cart(*this, "cartslot"),
		m_io_joy(*this, "JOY"),
		m_palette(*this, "palette")
	{ }

	void gameking(machine_config &config);
	void gameking3(machine_config &config);
	void gameking1(machine_config &config);

	void init_gameking();

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	void gameking_palette(palette_device &palette) const;
	void timer_w(uint8_t data);
	uint8_t input_r();
	uint8_t input2_r();
	TIMER_CALLBACK_MEMBER(gameking_timer);
	TIMER_CALLBACK_MEMBER(gameking_timer2);

	uint32_t screen_update_gameking(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	DECLARE_DEVICE_IMAGE_LOAD_MEMBER(cart_load);

	void gameking_mem(address_map &map);

	required_device<cpu_device> m_maincpu;
	required_device<generic_slot_device> m_cart;
	required_ioport m_io_joy;
	required_device<palette_device> m_palette;

	memory_region *m_cart_rom;
	emu_timer *timer1;
	emu_timer *timer2;

	uint8_t m_timer;
};


void gameking_state::timer_w(uint8_t data)
{
	m_timer = data;
	//m_maincpu->set_input_line(M6502_IRQ_LINE, CLEAR_LINE);
	timer1->enable(true);
	timer1->reset(m_maincpu->cycles_to_attotime(data * 300/*?*/));
}

uint8_t gameking_state::input_r()
{
	return m_io_joy->read() | ~3;
}

uint8_t gameking_state::input2_r()
{
	return m_io_joy->read() | 3;
}

void gameking_state::gameking_mem(address_map &map)
{
	map(0x000000, 0x07ffff).rom().region("maincpu", 0x10000);
}


static INPUT_PORTS_START( gameking )
	PORT_START("JOY")
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START) PORT_NAME("Start")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SELECT) PORT_NAME("Select") //?
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2) PORT_NAME("A")
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1) PORT_NAME("B")
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) //?
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP)
INPUT_PORTS_END

static constexpr rgb_t gameking_pens[] =
{
	{ 255, 255, 255 },
	{ 127, 127, 127 },
	{  63,  63,  63 },
	{   0,   0,   0 }
};

void gameking_state::gameking_palette(palette_device &palette) const
{
	palette.set_pen_colors(0, gameking_pens);
}


uint32_t gameking_state::screen_update_gameking(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	address_space *maincpu_ram = &m_maincpu->space(AS_PROGRAM);
	offs_t lssa = m_maincpu->state_int(st2xxx_device::ST_LSSA);
	if (lssa < 0x0080)
		return 0;

	for (int y=31, i=0;i<32;i++,y--)
	{
		for (int x=0, j=0;j<48/4;x+=4, j++)
		{
			uint8_t data=maincpu_ram->read_byte(lssa+j+i*12);
			bitmap.pix16(y, x+3)=data&3;
			bitmap.pix16(y, x+2)=(data>>2)&3;
			bitmap.pix16(y, x+1)=(data>>4)&3;
			bitmap.pix16(y, x)=(data>>6)&3;
		}
	}
	return 0;
}


void gameking_state::init_gameking()
{
	timer1 = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(gameking_state::gameking_timer), this));
	timer2 = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(gameking_state::gameking_timer2), this));
}

TIMER_CALLBACK_MEMBER(gameking_state::gameking_timer)
{
	m_maincpu->set_state_int(st2xxx_device::ST_IREQ,
		m_maincpu->state_int(st2xxx_device::ST_IREQ) | (0x016 & m_maincpu->state_int(st2xxx_device::ST_IENA)));
	timer1->enable(false);
	timer2->enable(true);
	timer2->reset(m_maincpu->cycles_to_attotime(10/*?*/));
}

TIMER_CALLBACK_MEMBER(gameking_state::gameking_timer2)
{
	//m_maincpu->set_input_line(M6502_IRQ_LINE, CLEAR_LINE); // in reality int for vector at fff4
	timer2->enable(false);
	timer1->enable(true);
	timer1->reset(m_maincpu->cycles_to_attotime(m_timer * 300/*?*/));
}

DEVICE_IMAGE_LOAD_MEMBER(gameking_state::cart_load)
{
	uint32_t size = m_cart->common_get_size("rom");

	if (size > 0x80000)
	{
		image.seterror(IMAGE_ERROR_UNSPECIFIED, "Unsupported cartridge size");
		return image_init_result::FAIL;
	}

	m_cart->rom_alloc(size, GENERIC_ROM8_WIDTH, ENDIANNESS_LITTLE);
	m_cart->common_load_rom(m_cart->get_rom_base(), size, "rom");

	return image_init_result::PASS;
}

void gameking_state::machine_start()
{
	std::string region_tag;
	m_cart_rom = memregion(region_tag.assign(m_cart->tag()).append(GENERIC_ROM_REGION_TAG).c_str());
	if (m_cart_rom)
		m_maincpu->space(AS_DATA).install_rom(0x400000, 0x400000 + m_cart_rom->bytes() - 1, m_cart_rom->base());
}

void gameking_state::machine_reset()
{
}


void gameking_state::gameking(machine_config &config)
{
	/* basic machine hardware */
	st2xxx_device &maincpu(ST2204(config, m_maincpu, 6000000));
	maincpu.set_addrmap(AS_DATA, &gameking_state::gameking_mem);
	maincpu.in_pa_callback().set(FUNC(gameking_state::input_r));
	maincpu.in_pb_callback().set(FUNC(gameking_state::input2_r));
	maincpu.out_pc_callback().set(FUNC(gameking_state::timer_w)); // wrong
	maincpu.in_pl_callback().set_constant(6); // bios protection endless loop

	/* video hardware */
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_LCD));
	screen.set_refresh_hz(60);
	screen.set_size(48, 32);
	screen.set_visarea_full();
	screen.set_screen_update(FUNC(gameking_state::screen_update_gameking));
	screen.set_palette(m_palette);

	PALETTE(config, m_palette, FUNC(gameking_state::gameking_palette), ARRAY_LENGTH(gameking_pens));

	/* cartridge */
	GENERIC_CARTSLOT(config, "cartslot", generic_plain_slot, "gameking_cart", "bin").set_device_load(FUNC(gameking_state::cart_load));
}

void gameking_state::gameking1(machine_config &config)
{
	gameking(config);
	SOFTWARE_LIST(config, "cart_list").set_original("gameking");
}

void gameking_state::gameking3(machine_config &config)
{
	gameking(config);
	SOFTWARE_LIST(config, "cart_list").set_original("gameking");
	SOFTWARE_LIST(config, "cart_list_3").set_original("gameking3");
}


ROM_START(gameking)
	ROM_REGION(0x10000+0x80000, "maincpu", ROMREGION_ERASE00)
//  ROM_LOAD("gm218.bin", 0x10000, 0x80000, CRC(8f52a928) SHA1(2e791fc7b642440d36820d2c53e1bb732375eb6e) ) // a14 inversed
	ROM_LOAD("gm218.bin", 0x10000, 0x80000, CRC(5a1ade3d) SHA1(e0d056f8ebfdf52ef6796d0375eba7fcc4a6a9d3) )
ROM_END

ROM_START(gamekin3)
	ROM_REGION(0x10000+0x80000, "maincpu", ROMREGION_ERASE00)
	ROM_LOAD("gm220.bin", 0x10000, 0x80000, CRC(1dc43bd5) SHA1(f9dcd3cb76bb7cb10565a1acb070ab375c082b4c) )
ROM_END

CONS( 2003, gameking, 0, 0, gameking1, gameking, gameking_state, init_gameking, "TimeTop", "GameKing GM-218", MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
// the GameKing 2 (GM-219) is probably identical HW

CONS( 2003, gamekin3, 0, 0, gameking3, gameking, gameking_state, init_gameking, "TimeTop", "GameKing 3",      MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
// gameking 3: similiar cartridges, accepts gameking cartridges, gameking3 cartridges not working on gameking (illegal cartridge scroller)
// my gameking bios backup solution might work on it
