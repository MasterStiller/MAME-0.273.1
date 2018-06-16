// license:BSD-3-Clause
// copyright-holders:Bryan McPhail

#include "machine/gen_latch.h"
#include "video/bufsprite.h"

class bbusters_state : public driver_device
{
public:
	bbusters_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_spriteram(*this, "spriteram%u", 1U),
		m_soundlatch(*this, "soundlatch%u", 1U),
		m_eprom_data(*this, "eeprom"),
		m_videoram(*this, "videoram"),
		m_pf_data(*this, "pf%u_data", 1U),
		m_pf_scroll_data(*this, "pf%u_scroll_data", 1U),
		m_scale_table(*this, "scale_table"),
		m_gun_io(*this, { "GUNX1", "GUNY1", "GUNX2", "GUNY2", "GUNX3", "GUNY3" }),
		m_gun_recoil(*this, "Player%u_Gun_Recoil", 1U)
		{ }

	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device<gfxdecode_device> m_gfxdecode;
	optional_device_array<buffered_spriteram16_device, 2> m_spriteram;
	required_device_array<generic_latch_8_device, 2> m_soundlatch;

	optional_shared_ptr<uint16_t> m_eprom_data;
	required_shared_ptr<uint16_t> m_videoram;
	required_shared_ptr_array<uint16_t, 2> m_pf_data;
	required_shared_ptr_array<uint16_t, 2> m_pf_scroll_data;

	required_region_ptr<uint8_t> m_scale_table;

	optional_ioport_array<6> m_gun_io;
	output_finder<3> m_gun_recoil;

	int m_gun_select;
	tilemap_t *m_fix_tilemap;
	tilemap_t *m_pf_tilemap[2];
	const uint8_t *m_scale_table_ptr;
	uint8_t m_scale_line_count;

	DECLARE_WRITE8_MEMBER(sound_cpu_w);
	DECLARE_READ16_MEMBER(eprom_r);
	DECLARE_READ16_MEMBER(control_3_r);
	DECLARE_WRITE16_MEMBER(gun_select_w);
	DECLARE_WRITE16_MEMBER(three_gun_output_w);
	DECLARE_READ16_MEMBER(kludge_r);
	DECLARE_WRITE16_MEMBER(video_w);
	template<int Layer> DECLARE_WRITE16_MEMBER(pf_w);

	TILE_GET_INFO_MEMBER(get_tile_info);
	template<int Layer, int Gfx> TILE_GET_INFO_MEMBER(get_pf_tile_info);

	virtual void machine_start() override;
	virtual void video_start() override;

	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	inline const uint8_t *get_source_ptr(gfx_element *gfx, uint32_t sprite, int dx, int dy, int block);
	void draw_block(bitmap_ind16 &dest,int x,int y,int size,int flipx,int flipy,uint32_t sprite,int color,int bank,int block);
	void draw_sprites(bitmap_ind16 &bitmap, const uint16_t *source, int bank, int colval, int colmask);
	void bbusters(machine_config &config);
	void mechatt(machine_config &config);
	void bbusters_map(address_map &map);
	void mechatt_map(address_map &map);
	void sound_map(address_map &map);
	void sound_portmap(address_map &map);
	void sounda_portmap(address_map &map);
};

class mechatt_state : public bbusters_state
{
public:
	mechatt_state(const machine_config &mconfig, device_type type, const char *tag) :
		bbusters_state(mconfig, type, tag)
		{ }

	DECLARE_WRITE16_MEMBER(two_gun_output_w);
	DECLARE_READ16_MEMBER(mechatt_gun_r);

	virtual void machine_start() override;
	virtual void video_start() override;

	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void mechatt(machine_config &config);
	void mechatt_map(address_map &map);
	void sounda_portmap(address_map &map);
};
