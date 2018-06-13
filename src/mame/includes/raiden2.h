// license:LGPL-2.1+
// copyright-holders:Olivier Galibert, Angelo Salese, David Haywood, Tomasz Slanina
#include "audio/seibu.h"
#include "machine/seibucop/seibucop.h"
#include "video/seibu_crtc.h"

#include <algorithm>

class raiden2_state : public driver_device
{
public:
	raiden2_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_spriteram(*this, "spriteram")
		, m_maincpu(*this, "maincpu")
		, m_seibu_sound(*this, "seibu_sound")
		, m_gfxdecode(*this, "gfxdecode")
		, m_palette(*this, "palette")

		, m_mainbank(*this, "mainbank%u", 1)

		, m_bg_bank(0)
		, m_fg_bank(0)
		, m_mid_bank(0)
		, m_tx_bank(0)
		, m_tilemap_enable(0)
		, m_prg_bank(0)
		, m_cop_bank(0)

		, m_sprite_prot_x(0)
		, m_sprite_prot_y(0)
		, m_dst1(0)
		, m_cop_spr_maxx(0)
		, m_cop_spr_off(0)

		, m_tile_buffer(320, 256)
		, m_sprite_buffer(320, 256)
		, m_raiden2cop(*this, "raiden2cop")
	{
		std::fill(std::begin(m_scrollvals), std::end(m_scrollvals), 0);
		std::fill(std::begin(m_sprite_prot_src_addr), std::end(m_sprite_prot_src_addr), 0);
	}

	std::unique_ptr<uint16_t[]> m_back_data;
	std::unique_ptr<uint16_t[]> m_fore_data;
	std::unique_ptr<uint16_t[]> m_mid_data;
	std::unique_ptr<uint16_t[]> m_text_data; // private buffers, allocated in init
	std::unique_ptr<uint16_t[]> m_palette_data;
	required_shared_ptr<uint16_t> m_spriteram;
	required_device<cpu_device> m_maincpu;
	optional_device<seibu_sound_device> m_seibu_sound;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<palette_device> m_palette;

	optional_memory_bank_array<2> m_mainbank;

	DECLARE_WRITE16_MEMBER ( raiden2_bank_w );
	DECLARE_READ16_MEMBER ( cop_tile_bank_2_r );
	DECLARE_WRITE16_MEMBER ( cop_tile_bank_2_w );
	DECLARE_WRITE16_MEMBER ( raidendx_cop_bank_2_w );
	DECLARE_WRITE16_MEMBER ( tilemap_enable_w );
	DECLARE_WRITE16_MEMBER ( tile_scroll_w );
	DECLARE_WRITE16_MEMBER ( tile_bank_01_w );
	DECLARE_WRITE16_MEMBER ( background_w );
	DECLARE_WRITE16_MEMBER ( foreground_w );
	DECLARE_WRITE16_MEMBER ( midground_w );
	DECLARE_WRITE16_MEMBER ( text_w );
	DECLARE_WRITE16_MEMBER(m_videoram_private_w);

	DECLARE_WRITE16_MEMBER( sprcpt_val_1_w );
	DECLARE_WRITE16_MEMBER( sprcpt_val_2_w );
	DECLARE_WRITE16_MEMBER( sprcpt_data_1_w );
	DECLARE_WRITE16_MEMBER( sprcpt_data_2_w );
	DECLARE_WRITE16_MEMBER( sprcpt_data_3_w );
	DECLARE_WRITE16_MEMBER( sprcpt_data_4_w );
	DECLARE_WRITE16_MEMBER( sprcpt_adr_w );
	DECLARE_WRITE16_MEMBER( sprcpt_flags_1_w );
	DECLARE_WRITE16_MEMBER( sprcpt_flags_2_w );

	void common_reset(int bgbank, int fgbank, int midbank, int txbank);

	static uint16_t const raiden_blended_colors[];
	static uint16_t const xsedae_blended_colors[];
	static uint16_t const zeroteam_blended_colors[];

	bool m_blend_active[0x800]; // cfg
	
	tilemap_t *m_background_layer,;
	tilemap_t *m_midground_layer;
	tilemap_t *m_foreground_layer;
	tilemap_t *m_text_layer;

	int m_bg_bank, m_fg_bank, m_mid_bank, m_tx_bank;
	uint16_t m_tilemap_enable;
	uint8_t m_prg_bank;
	uint16_t m_cop_bank;

	uint16_t m_scrollvals[6];

	DECLARE_WRITE16_MEMBER( sprite_prot_x_w );
	DECLARE_WRITE16_MEMBER( sprite_prot_y_w );
	DECLARE_WRITE16_MEMBER( sprite_prot_src_seg_w );
	DECLARE_WRITE16_MEMBER( sprite_prot_src_w );
	DECLARE_READ16_MEMBER( sprite_prot_src_seg_r );
	DECLARE_READ16_MEMBER( sprite_prot_dst1_r );
	DECLARE_READ16_MEMBER( sprite_prot_maxx_r );
	DECLARE_READ16_MEMBER( sprite_prot_off_r );
	DECLARE_WRITE16_MEMBER( sprite_prot_dst1_w );
	DECLARE_WRITE16_MEMBER( sprite_prot_maxx_w );
	DECLARE_WRITE16_MEMBER( sprite_prot_off_w );

	uint16_t m_sprite_prot_x,m_sprite_prot_y,m_dst1,m_cop_spr_maxx,m_cop_spr_off;
	uint16_t m_sprite_prot_src_addr[2];

	void draw_sprites(const rectangle &cliprect);

	const int *m_cur_spri; // cfg

	void init_raidendx();
	void init_xsedae();
	void init_zeroteam();
	void init_raiden2();
	TILE_GET_INFO_MEMBER(get_back_tile_info);
	TILE_GET_INFO_MEMBER(get_mid_tile_info);
	TILE_GET_INFO_MEMBER(get_fore_tile_info);
	TILE_GET_INFO_MEMBER(get_text_tile_info);
	DECLARE_MACHINE_RESET(raiden2);
	DECLARE_VIDEO_START(raiden2);
	DECLARE_MACHINE_RESET(zeroteam);
	DECLARE_MACHINE_RESET(xsedae);
	DECLARE_MACHINE_RESET(raidendx);
	uint32_t screen_update_raiden2(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	INTERRUPT_GEN_MEMBER(raiden2_interrupt);
	void combine32(uint32_t *val, int offset, uint16_t data, uint16_t mem_mask);
	void sprcpt_init();

	void blend_layer(bitmap_rgb32 &bitmap, const rectangle &cliprect, bitmap_ind16 &source, int layer);
	void tilemap_draw_and_blend(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect, tilemap_t *tilemap);

	void init_blending(const uint16_t *table);

	bitmap_ind16 m_tile_buffer, m_sprite_buffer;
	optional_device<raiden2cop_device> m_raiden2cop;

	void raidendx(machine_config &config);
	void xsedae(machine_config &config);
	void zeroteam(machine_config &config);
	void raiden2(machine_config &config);
	void raiden2_cop_mem(address_map &map);
	void raiden2_mem(address_map &map);
	void raiden2_sound_map(address_map &map);
	void raidendx_mem(address_map &map);
	void xsedae_mem(address_map &map);
	void zeroteam_mem(address_map &map);
	void zeroteam_sound_map(address_map &map);
protected:
	virtual void machine_start() override;
};

GFXDECODE_EXTERN( gfx_raiden2 );
