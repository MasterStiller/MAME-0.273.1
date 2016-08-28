// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria,Vas Crabb
#include "video/bufsprite.h"

#define MCFG_DOOYONG_ROM_TILEMAP_ADD(tag, gfx, num, rom, offset) \
		MCFG_DEVICE_ADD(tag, DOOYONG_ROM_TILEMAP, 0) \
		dooyong_rom_tilemap_device::static_set_gfxdecode_tag(*device, "^" gfx); \
		dooyong_rom_tilemap_device::static_set_tilerom_tag(*device, "^" rom); \
		dooyong_rom_tilemap_device::static_set_gfxnum(*device, (num)); \
		dooyong_rom_tilemap_device::static_set_tilerom_offset(*device, (offset));
#define MCFG_DOOYONG_ROM_TILEMAP_TRANSPARENT_PEN(pen) \
		dooyong_rom_tilemap_device::static_set_transparent_pen(*device, (pen));
#define MCFG_DOOYONG_ROM_TILEMAP_PRIMELLA_CODE_BITS(bits) \
		dooyong_rom_tilemap_device::static_set_primella_code_bits(*device, (bits));

#define MCFG_RSHARK_ROM_TILEMAP_ADD(tag, gfx, num, rom, offset, rom2, offset2) \
		MCFG_DEVICE_ADD(tag, RSHARK_ROM_TILEMAP, 0) \
		dooyong_rom_tilemap_device::static_set_gfxdecode_tag(*device, "^" gfx); \
		dooyong_rom_tilemap_device::static_set_tilerom_tag(*device, "^" rom); \
		dooyong_rom_tilemap_device::static_set_gfxnum(*device, (num)); \
		dooyong_rom_tilemap_device::static_set_tilerom_offset(*device, (offset)); \
		dooyong_rom_tilemap_device::static_set_primella_code_bits(*device, 13); \
		rshark_rom_tilemap_device::static_set_colorrom_tag(*device, "^" rom2); \
		rshark_rom_tilemap_device::static_set_colorrom_offset(*device, (offset2));


extern device_type const DOOYONG_ROM_TILEMAP;
extern device_type const RSHARK_ROM_TILEMAP;


class dooyong_rom_tilemap_device : public device_t
{
public:
	dooyong_rom_tilemap_device(machine_config const &mconfig, char const *tag, device_t *owner, UINT32 clock);

	static void static_set_gfxdecode_tag(device_t &device, char const *tag);
	static void static_set_tilerom_tag(device_t &device, char const *tag);
	static void static_set_gfxnum(device_t &device, int gfxnum);
	static void static_set_tilerom_offset(device_t &device, int offset);
	static void static_set_transparent_pen(device_t &device, unsigned pen);
	static void static_set_primella_code_bits(device_t &device, unsigned bits);

	void draw(screen_device &screen, bitmap_ind16 &dest, rectangle const &cliprect, UINT32 flags, UINT8 priority);

	DECLARE_WRITE8_MEMBER(ctrl_w);
	void set_palette_bank(UINT16 bank);

protected:
	dooyong_rom_tilemap_device(
			machine_config const &mconfig,
			device_type type,
			char const *name,
			char const *tag,
			device_t *owner,
			UINT32 clock,
			char const *shortname,
			char const *source);

	virtual void device_start() override;

	virtual TILE_GET_INFO_MEMBER(tile_info);

	gfx_element const &gfx() const
	{ return *m_gfxdecode->gfx(m_gfxnum); }

	tilemap_memory_index adjust_tile_index(tilemap_memory_index tile_index) const
	{ return tile_index + ((unsigned(m_registers[1]) * 256U / gfx().width()) * m_rows); }

	int m_rows;

private:
	required_device<gfxdecode_device> m_gfxdecode;
	required_region_ptr<UINT8> m_tilerom;
	int m_gfxnum;
	int m_tilerom_offset;
	unsigned m_transparent_pen;
	unsigned m_primella_code_mask;
	unsigned m_primella_color_mask;
	unsigned m_primella_color_shift;

	tilemap_t *m_tilemap;

	UINT8 m_registers[0x10];
	UINT16 m_palette_bank;
};


class rshark_rom_tilemap_device : public dooyong_rom_tilemap_device
{
public:
	rshark_rom_tilemap_device(machine_config const &mconfig, char const *tag, device_t *owner, UINT32 clock);

	static void static_set_colorrom_tag(device_t &device, char const *tag);
	static void static_set_colorrom_offset(device_t &device, int offset);

protected:
	virtual void device_start() override;

	virtual TILE_GET_INFO_MEMBER(tile_info) override;

private:
	required_region_ptr<UINT8> m_colorrom;
	int m_colorrom_offset;
};


class dooyong_state : public driver_device
{
public:
	dooyong_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_audiocpu(*this, "audiocpu")
		, m_gfxdecode(*this, "gfxdecode")
		, m_palette(*this, "palette")
		, m_bg(*this, "bg")
		, m_bg2(*this, "bg2")
		, m_fg(*this, "fg")
		, m_fg2(*this, "fg2")
	{ }

	tilemap_t *m_tx_tilemap = nullptr;

	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<palette_device> m_palette;
	optional_device<dooyong_rom_tilemap_device> m_bg;
	optional_device<dooyong_rom_tilemap_device> m_bg2;
	optional_device<dooyong_rom_tilemap_device> m_fg;
	optional_device<dooyong_rom_tilemap_device> m_fg2;
};

class dooyong_z80_state : public dooyong_state
{
public:
	dooyong_z80_state(const machine_config &mconfig, device_type type, const char *tag)
		: dooyong_state(mconfig, type, tag)
		, m_txvideoram(*this, "txvideoram")
		, m_spriteram(*this, "spriteram")
	{ }

	enum
	{
		SPRITE_12BIT = 0x01,
		SPRITE_HEIGHT = 0x02,
		SPRITE_YSHIFT_BLUEHAWK = 0x04,
		SPRITE_YSHIFT_FLYTIGER = 0x08
	};

	DECLARE_WRITE8_MEMBER(flip_screen_w);
	DECLARE_WRITE8_MEMBER(bankswitch_w);
	DECLARE_WRITE8_MEMBER(txvideoram_w);
	DECLARE_WRITE8_MEMBER(primella_ctrl_w);
	DECLARE_READ8_MEMBER(paletteram_flytiger_r);
	DECLARE_WRITE8_MEMBER(paletteram_flytiger_w);
	DECLARE_WRITE8_MEMBER(flytiger_ctrl_w);
	TILE_GET_INFO_MEMBER(get_tx_tile_info);
	void draw_sprites(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, unsigned extensions = 0);
	UINT32 screen_update_bluehawk(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_flytiger(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_primella(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	DECLARE_MACHINE_START(cpu_z80);
	DECLARE_VIDEO_START(bluehawk);
	DECLARE_VIDEO_START(flytiger);
	DECLARE_VIDEO_START(primella);

	required_shared_ptr<UINT8> m_txvideoram;
	std::unique_ptr<UINT8[]> m_paletteram_flytiger;
	UINT8 m_sprites_disabled = 0;
	UINT8 m_flytiger_pri = 0;
	UINT8 m_tx_pri = 0;
	UINT8 m_palette_bank = 0;
	int m_tx_tilemap_mode = 0;

	optional_device<buffered_spriteram8_device> m_spriteram;
};

class dooyong_z80_ym2203_state : public dooyong_z80_state
{
public:
	dooyong_z80_ym2203_state(const machine_config &mconfig, device_type type, const char *tag)
		: dooyong_z80_state(mconfig, type, tag)
	{ }

	DECLARE_WRITE8_MEMBER(lastday_ctrl_w);
	DECLARE_WRITE8_MEMBER(pollux_ctrl_w);
	DECLARE_WRITE_LINE_MEMBER(irqhandler_2203_1);
	DECLARE_WRITE_LINE_MEMBER(irqhandler_2203_2);
	DECLARE_READ8_MEMBER(unk_r);
	DECLARE_MACHINE_RESET(sound_ym2203);
	UINT32 screen_update_lastday(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_gulfstrm(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_pollux(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	DECLARE_VIDEO_START(lastday);
	DECLARE_VIDEO_START(gulfstrm);
	DECLARE_VIDEO_START(pollux);

	int m_interrupt_line_1 = 0;
	int m_interrupt_line_2 = 0;
};

class dooyong_68k_state : public dooyong_state
{
public:
	dooyong_68k_state(const machine_config &mconfig, device_type type, const char *tag)
		: dooyong_state(mconfig, type, tag)
		, m_bg_bitmap()
		, m_bg2_bitmap()
		, m_screen(*this, "screen")
		, m_spriteram(*this, "spriteram")
	{ }

	DECLARE_WRITE16_MEMBER(ctrl_w);
	TIMER_DEVICE_CALLBACK_MEMBER(scanline);
	void draw_sprites(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_rshark(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	UINT32 screen_update_popbingo(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	DECLARE_VIDEO_START(rshark);
	DECLARE_VIDEO_START(popbingo);

	UINT16 m_bg2_priority = 0;

	bitmap_ind16 m_bg_bitmap;
	bitmap_ind16 m_bg2_bitmap;

	required_device<screen_device> m_screen;
	required_device<buffered_spriteram16_device> m_spriteram;
};
