// license:BSD-3-Clause
// copyright-holders:David Haywood

#include "emu.h"
#include "cpu/m68000/m68000.h"
#include "sound/okim6295.h"

class flagrall_state : public driver_device
{
public:
	flagrall_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_spr_info(*this, "spr_info"),
		m_spr_videoram(*this, "spr_videoram"),
		m_bak_videoram(*this, "bak_videoram"),
		m_maincpu(*this, "maincpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_oki(*this, "oki"),
		xscroll(0),
		yscroll(0),
		ctrl(0)
		{ }

	/* memory pointers */
	required_shared_ptr<UINT16> m_spr_info;
	required_shared_ptr<UINT16> m_spr_videoram;
	required_shared_ptr<UINT16> m_bak_videoram;

	/* video-related */
	tilemap_t    *m_bak_tilemap;
	DECLARE_WRITE16_MEMBER(flagrall_bak_videoram_w);
	TILE_GET_INFO_MEMBER(get_flagrall_bak_tile_info);

	virtual void video_start() override;
	UINT32 screen_update_flagrall(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	required_device<cpu_device> m_maincpu;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<okim6295_device> m_oki;

	DECLARE_WRITE16_MEMBER(flagrall_xscroll_w);
	DECLARE_WRITE16_MEMBER(flagrall_yscroll_w);
	DECLARE_WRITE16_MEMBER(flagrall_ctrl_w);

	UINT16 xscroll;
	UINT16 yscroll;
	UINT16 ctrl;
};




WRITE16_MEMBER(flagrall_state::flagrall_xscroll_w)
{
	COMBINE_DATA(&xscroll);
	m_bak_tilemap->set_scrollx(0, xscroll);
}

WRITE16_MEMBER(flagrall_state::flagrall_yscroll_w)
{
	COMBINE_DATA(&yscroll);
	m_bak_tilemap->set_scrolly(0, yscroll);
}

WRITE16_MEMBER(flagrall_state::flagrall_ctrl_w)
{
	COMBINE_DATA(&ctrl);
	
	// 0x0200 on startup
	// 0x0100 on startup

	// 0x80 - ?
	// 0x40 - ?
	// 0x20 - toggles, might trigger vram -> buffer transfer?
	// 0x10 - unknown, always on?
	// 0x08 - ?
	// 0x06 - oki bank
	// 0x01 - ?

	if (ctrl & 0xfcc9)
		popmessage("unk control %04x", ctrl & 0xfcc9);

	m_oki->set_bank_base(0x40000 * ((data & 0x6)>>1) );

}




WRITE16_MEMBER(flagrall_state::flagrall_bak_videoram_w)
{
	COMBINE_DATA(&m_bak_videoram[offset]);
	m_bak_tilemap->mark_tile_dirty(offset);
}

TILE_GET_INFO_MEMBER(flagrall_state::get_flagrall_bak_tile_info)
{
	int tileno = m_bak_videoram[tile_index];
	SET_TILE_INFO_MEMBER(1, tileno, 0, 0);
}


void flagrall_state::video_start()
{
	m_bak_tilemap = &machine().tilemap().create(m_gfxdecode, tilemap_get_info_delegate(FUNC(flagrall_state::get_flagrall_bak_tile_info),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
}

UINT32 flagrall_state::screen_update_flagrall(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	m_bak_tilemap->draw(screen, bitmap, cliprect, 0, 0);

	// sprites are simple, 2 ram areas
	
	// area 1 (1 word per sprite)
	// xxxx xxxx yyyy yyyy (x / y = low 8 x / y position bits)
	// area 2 (1 word per sprites)
	// tttt tttt tttt tttX  (t = tile number, X = high x-bit)

	for (int i = 0;i < 0x1000 / 2;i++)
	{
		gfx_element *gfx = m_gfxdecode->gfx(0);

		int sprx = m_spr_info[i] >> 8;
		int spry = m_spr_info[i] & 0x00ff;
		sprx |= (m_spr_videoram[i] & 0x01) << 8;
		UINT16 sprtile = m_spr_videoram[i] >> 1;

		gfx->transpen(bitmap,cliprect,sprtile,1,0,0,sprx,spry,0);
		gfx->transpen(bitmap,cliprect,sprtile,1,0,0,sprx,spry-0x100,0);
		gfx->transpen(bitmap,cliprect,sprtile,1,0,0,sprx-0x200,spry,0);
		gfx->transpen(bitmap,cliprect,sprtile,1,0,0,sprx-0x200,spry-0x100,0);

	}

	return 0;
}


static ADDRESS_MAP_START( flagrall_map, AS_PROGRAM, 16, flagrall_state )
	AM_RANGE(0x000000, 0x07ffff) AM_ROM
	AM_RANGE(0x100000, 0x10ffff) AM_RAM // main ram

	AM_RANGE(0x200000, 0x2003ff) AM_RAM_DEVWRITE("palette", palette_device, write) AM_SHARE("palette")
	AM_RANGE(0x240000, 0x240fff) AM_RAM AM_SHARE("spr_info") 
	AM_RANGE(0x280000, 0x280fff) AM_RAM AM_SHARE("spr_videoram") 
	AM_RANGE(0x2c0000, 0x2c07ff) AM_RAM_WRITE(flagrall_bak_videoram_w) AM_SHARE("bak_videoram") 

	AM_RANGE(0x340000, 0x340001) AM_WRITE(flagrall_xscroll_w)
	AM_RANGE(0x380000, 0x380001) AM_WRITE(flagrall_yscroll_w)
	AM_RANGE(0x3c0000, 0x3c0001) AM_WRITE(flagrall_ctrl_w)

	AM_RANGE(0x400000, 0x400001) AM_READ_PORT("IN0")
	AM_RANGE(0x440000, 0x440001) AM_READ_PORT("IN1")
	AM_RANGE(0x480000, 0x480001) AM_READ_PORT("IN2")

	AM_RANGE(0x4c0000, 0x4c0001) AM_DEVREADWRITE8("oki", okim6295_device, read, write, 0x00ff)
ADDRESS_MAP_END


static INPUT_PORTS_START( flagrall )
	PORT_START("IN0")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN1")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN2")
	PORT_DIPNAME(  0x0003, 0x0003, DEF_STR( Coinage ) ) PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(       0x0000, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(       0x0001, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(       0x0003, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(       0x0002, DEF_STR( 1C_2C ) )
	PORT_DIPUNUSED_DIPLOC( 0x0004, IP_ACTIVE_LOW, "SW1:3" )
	PORT_DIPUNUSED_DIPLOC( 0x0008, IP_ACTIVE_LOW, "SW1:4" )		
	PORT_DIPNAME(  0x0010, 0x0000, DEF_STR( Demo_Sounds ) )  PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(       0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(       0x0000, DEF_STR( On ) )
	PORT_DIPNAME(  0x0020, 0x0020, "Dip Control" )  PORT_DIPLOCATION("SW1:6")
	PORT_DIPSETTING(       0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(       0x0000, DEF_STR( On ) )		
	PORT_DIPUNUSED_DIPLOC( 0x0040, IP_ACTIVE_LOW, "SW1:7" )
	PORT_DIPNAME(  0x0080, 0x0080, "Picture Test" )  PORT_DIPLOCATION("SW1:8")
	PORT_DIPSETTING(       0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(       0x0000, DEF_STR( On ) )

	PORT_DIPNAME(  0x0300, 0x0300, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:1,2")
	PORT_DIPSETTING(       0x0200, "1" )
	PORT_DIPSETTING(       0x0100, "2" )
	PORT_DIPSETTING(       0x0300, "3" )
	PORT_DIPSETTING(       0x0000, "5" )
	PORT_DIPNAME(  0x0400, 0x0400, "Bonus Type" )  PORT_DIPLOCATION("SW2:3")
	PORT_DIPSETTING (      0x0400, "0" )
	PORT_DIPSETTING(       0x0000, "1" )
	PORT_DIPUNUSED_DIPLOC( 0x0800, IP_ACTIVE_LOW, "SW2:4" )	
	PORT_DIPNAME(  0x3000, 0x3000, DEF_STR( Difficulty ) ) PORT_DIPLOCATION("SW2:5,6")
	PORT_DIPSETTING(       0x0000, DEF_STR( Very_Hard ) )
	PORT_DIPSETTING(       0x1000, DEF_STR( Hard ) )
	PORT_DIPSETTING(       0x2000, DEF_STR( Easy ) )
	PORT_DIPSETTING(       0x3000, DEF_STR( Normal ) )
	PORT_DIPUNUSED_DIPLOC( 0x4000, IP_ACTIVE_LOW, "SW2:7" )	
	PORT_DIPNAME(  0x8000, 0x8000, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(       0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(       0x0000, DEF_STR( On ) )
INPUT_PORTS_END

static const gfx_layout flagrall_layout =
{
	16,16,
	RGN_FRAC(1,1),
	8,
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,8*8,9*8,10*8,11*8,12*8,13*8,14*8,15*8 },
	{ 0*128, 1*128, 2*128, 3*128, 4*128, 5*128, 6*128, 7*128, 8*128, 9*128, 10*128, 11*128, 12*128, 13*128, 14*128, 15*128 },
	16*128,
};



static GFXDECODE_START( flagrall )
	GFXDECODE_ENTRY( "sprites", 0, flagrall_layout,   0x0, 2  ) /* sprite tiles */
	GFXDECODE_ENTRY( "tiles", 0, flagrall_layout,  0x0, 2  ) /* bg tiles */
GFXDECODE_END


static MACHINE_CONFIG_START( flagrall, flagrall_state )

	MCFG_CPU_ADD("maincpu", M68000, 16000000 ) // ?
	MCFG_CPU_PROGRAM_MAP(flagrall_map)
	MCFG_CPU_VBLANK_INT_DRIVER("screen", flagrall_state,  irq4_line_hold)

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", flagrall)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60) // not verified
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(64*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0*8, 40*8-1, 0*8, 30*8-1)
	MCFG_SCREEN_UPDATE_DRIVER(flagrall_state, screen_update_flagrall)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_PALETTE_ADD("palette", 0x200)
	MCFG_PALETTE_FORMAT(xBBBBBGGGGGRRRRR)

	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MCFG_OKIM6295_ADD("oki", 16000000/16, OKIM6295_PIN7_HIGH) // not verified
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.47)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.47)
MACHINE_CONFIG_END


ROM_START( flagrall )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "11_u34.bin", 0x00001, 0x40000, CRC(24dd439d) SHA1(88857ad5ed69f29de86702dcc746d35b69b3b93d) )
	ROM_LOAD16_BYTE( "12_u35.bin", 0x00000, 0x40000, CRC(373b71a5) SHA1(be9ab93129e2ffd9bfe296c341dbdf47f1949ac7) )

	ROM_REGION( 0x100000, "oki", 0 ) /* Samples */
	// 3x banks
	ROM_LOAD( "13_su4.bin", 0x00000, 0x80000, CRC(7b0630b3) SHA1(c615e6630ffd12c122762751c25c249393bf7abd) )
	ROM_LOAD( "14_su6.bin", 0x80000, 0x40000, CRC(593b038f) SHA1(b00dcf321fe541ee52c34b79e69c44f3d7a9cd7c) )

	ROM_REGION( 0x300000, "sprites", 0 )
	ROM_LOAD32_BYTE( "1_u5.bin",  0x000000, 0x080000, CRC(9377704b) SHA1(ac516a8ba6d1a70086469504c2a46d47a1f4560b) )
	ROM_LOAD32_BYTE( "5_u6.bin",  0x000001, 0x080000, CRC(1ac0bd0c) SHA1(ab71bb84e61f5c7168601695f332a8d4a30d9948) )
	ROM_LOAD32_BYTE( "2_u7.bin",  0x000002, 0x080000, CRC(5f6db2b3) SHA1(84caa019d3b75be30a14d19ccc2f28e5e94028bd) )
	ROM_LOAD32_BYTE( "6_u8.bin",  0x000003, 0x080000, CRC(79e4643c) SHA1(274f2741f39c63e32f49c6a1a72ded1263bdcdaa) )
		
	ROM_LOAD32_BYTE( "3_u58.bin", 0x200000, 0x040000, CRC(c913df7d) SHA1(96e89ecb9e5f4d596d71d7ba35af7b2af4670342) )
	ROM_LOAD32_BYTE( "4_u59.bin", 0x200001, 0x040000, CRC(cb192384) SHA1(329b4c1a4dc388d9f4ce063f9a54cbf3b967682a) )
	ROM_LOAD32_BYTE( "7_u60.bin", 0x200002, 0x040000, CRC(f187a7bf) SHA1(f4ce9ac9fe376250fe426de6ee404fc7841ef08a) )
	ROM_LOAD32_BYTE( "8_u61.bin", 0x200003, 0x040000, CRC(b73fa441) SHA1(a5a3533563070c870276ead5e2f9cb9aaba303cc))

	ROM_REGION( 0x100000, "tiles", 0 )
	ROM_LOAD( "10_u102.bin", 0x00000, 0x80000, CRC(b1fd3279) SHA1(4a75581e13d43bef441ce81eae518c2f6bc1d5f8) )
	ROM_LOAD( "9_u103.bin",  0x80000, 0x80000, CRC(01e6d654) SHA1(821d61a5b16f5cb76e2a805c8504db1ef38c3a48) )
ROM_END


GAME( 199?, flagrall, 0,        flagrall, flagrall, driver_device, 0, ROT0,  "<unknown>", "'96 Flag Rally", 0 )


