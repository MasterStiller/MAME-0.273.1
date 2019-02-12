// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/*
    SGI "Newport" graphics board used in the Indy and some Indigo2s

    Newport is modular, consisting of the following custom chips:
    - REX3: Raster Engine, which is basically a blitter which can also draw antialiased lines.
            REX also acts as the interface to the rest of the system - all the other chips on
        a Newport board are accessed through it.
    - RB2: Frame buffer input controller
    - RO1: Frame buffer output controller
    - XMAP9: Final display generator
    - CMAP: Palette mapper
    - VC2: Video timing controller / CRTC

    Taken from the Linux Newport driver, slave addresses for Newport devices are:
            VC2         0
            Both CMAPs  1
            CMAP 0      2
            CMAP 1      3
            Both XMAPs  4
            XMAP 0      5
            XMAP 1      6
            RAMDAC      7
            VIDEO (CC1) 8
            VIDEO (AB1) 9
*/

#include "emu.h"
#include "video/newport.h"

#define LOG_UNKNOWN     (1 << 0)
#define LOG_VC2         (1 << 1)
#define LOG_CMAP0       (1 << 2)
#define LOG_CMAP1       (1 << 3)
#define LOG_XMAP0       (1 << 4)
#define LOG_XMAP1       (1 << 5)
#define LOG_REX3        (1 << 6)
#define LOG_COMMANDS    (1 << 7)
#define LOG_ALL         (LOG_UNKNOWN | LOG_VC2 | LOG_CMAP0 | LOG_CMAP1 | LOG_XMAP0 | LOG_XMAP1 | LOG_REX3)

#define VERBOSE 0
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NEWPORT_VIDEO, newport_video_device, "newport_video", "SGI Newport graphics board")


newport_video_device::newport_video_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, NEWPORT_VIDEO, tag, owner, clock)
	, m_maincpu(*this, finder_base::DUMMY_TAG)
	, m_hpc3(*this, finder_base::DUMMY_TAG)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void newport_video_device::device_start()
{
	m_base = make_unique_clear<uint8_t[]>((1280+64) * (1024+64));

	save_pointer(NAME(m_base), (1280+64) * (1024+64));
	save_item(NAME(m_vc2.m_vid_entry));
	save_item(NAME(m_vc2.m_cursor_entry));
	save_item(NAME(m_vc2.m_cursor_x));
	save_item(NAME(m_vc2.m_cursor_y));
	save_item(NAME(m_vc2.m_cur_cursor_x));
	save_item(NAME(m_vc2.m_did_entry));
	save_item(NAME(m_vc2.m_scanline_len));
	save_item(NAME(m_vc2.m_ram_addr));
	save_item(NAME(m_vc2.m_vt_frame_ptr));
	save_item(NAME(m_vc2.m_vt_line_ptr));
	save_item(NAME(m_vc2.m_vt_line_run));
	save_item(NAME(m_vc2.m_vt_line_count));
	save_item(NAME(m_vc2.m_cursor_table_ptr));
	save_item(NAME(m_vc2.m_work_cursor_y));
	save_item(NAME(m_vc2.m_did_frame_ptr));
	save_item(NAME(m_vc2.m_did_line_ptr));
	save_item(NAME(m_vc2.m_display_ctrl));
	save_item(NAME(m_vc2.m_config));
	save_item(NAME(m_vc2.m_ram));
	save_item(NAME(m_vc2.m_reg_idx));
	save_item(NAME(m_vc2.m_reg_data));

	save_item(NAME(m_xmap0.m_config));
	save_item(NAME(m_xmap0.m_revision));
	save_item(NAME(m_xmap0.m_entries));
	save_item(NAME(m_xmap0.m_cursor_cmap));
	save_item(NAME(m_xmap0.m_popup_cmap));
	save_item(NAME(m_xmap0.m_mode_table_idx));
	save_item(NAME(m_xmap0.m_mode_table));
	save_item(NAME(m_xmap1.m_config));
	save_item(NAME(m_xmap1.m_revision));
	save_item(NAME(m_xmap1.m_entries));
	save_item(NAME(m_xmap1.m_cursor_cmap));
	save_item(NAME(m_xmap1.m_popup_cmap));
	save_item(NAME(m_xmap1.m_mode_table_idx));
	save_item(NAME(m_xmap1.m_mode_table));

	save_item(NAME(m_rex3.m_draw_mode1));
	save_item(NAME(m_rex3.m_draw_mode0));
	save_item(NAME(m_rex3.m_ls_mode));
	save_item(NAME(m_rex3.m_ls_pattern));
	save_item(NAME(m_rex3.m_ls_pattern_saved));
	save_item(NAME(m_rex3.m_z_pattern));
	save_item(NAME(m_rex3.m_color_back));
	save_item(NAME(m_rex3.m_color_vram));
	save_item(NAME(m_rex3.m_alpha_ref));
	save_item(NAME(m_rex3.m_smask_x));
	save_item(NAME(m_rex3.m_smask_y));
	save_item(NAME(m_rex3.m_setup));
	save_item(NAME(m_rex3.m_step_z));
	save_item(NAME(m_rex3.m_x_start));
	save_item(NAME(m_rex3.m_y_start));
	save_item(NAME(m_rex3.m_x_end));
	save_item(NAME(m_rex3.m_y_end));

	save_item(NAME(m_rex3.m_x_save));
	save_item(NAME(m_rex3.m_xy_move));
	save_item(NAME(m_rex3.m_bres_d));
	save_item(NAME(m_rex3.m_bres_s1));
	save_item(NAME(m_rex3.m_bres_octant_inc1));
	save_item(NAME(m_rex3.m_bres_round_inc2));
	save_item(NAME(m_rex3.m_bres_e1));
	save_item(NAME(m_rex3.m_bres_s2));
	save_item(NAME(m_rex3.m_a_weight0));
	save_item(NAME(m_rex3.m_a_weight1));
	save_item(NAME(m_rex3.m_x_start_f));
	save_item(NAME(m_rex3.m_y_start_f));
	save_item(NAME(m_rex3.m_x_end_f));
	save_item(NAME(m_rex3.m_y_end_f));
	save_item(NAME(m_rex3.m_x_start_i));
	save_item(NAME(m_rex3.m_xy_start_i));
	save_item(NAME(m_rex3.m_xy_end_i));
	save_item(NAME(m_rex3.m_x_start_end_i));
	save_item(NAME(m_rex3.m_color_red));
	save_item(NAME(m_rex3.m_color_alpha));
	save_item(NAME(m_rex3.m_color_green));
	save_item(NAME(m_rex3.m_color_blue));
	save_item(NAME(m_rex3.m_slope_red));
	save_item(NAME(m_rex3.m_slope_alpha));
	save_item(NAME(m_rex3.m_slope_green));
	save_item(NAME(m_rex3.m_slope_blue));
	save_item(NAME(m_rex3.m_write_mask));
	save_item(NAME(m_rex3.m_zero_fract));
	save_item(NAME(m_rex3.m_zero_overflow));
	save_item(NAME(m_rex3.m_host_dataport));
	save_item(NAME(m_rex3.m_dcb_mode));
	save_item(NAME(m_rex3.m_dcb_reg_select));
	save_item(NAME(m_rex3.m_dcb_slave_select));
	save_item(NAME(m_rex3.m_dcb_data_msw));
	save_item(NAME(m_rex3.m_dcb_data_lsw));
	save_item(NAME(m_rex3.m_top_scanline));
	save_item(NAME(m_rex3.m_xy_window));
	save_item(NAME(m_rex3.m_clip_mode));
	save_item(NAME(m_rex3.m_config));
	save_item(NAME(m_rex3.m_status));
	save_item(NAME(m_rex3.m_iter_x));
	save_item(NAME(m_rex3.m_iter_y));
	save_item(NAME(m_rex3.m_xfer_width));
	save_item(NAME(m_rex3.m_read_active));

	save_item(NAME(m_cmap0.m_palette_idx));
	save_item(NAME(m_cmap0.m_palette));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void newport_video_device::device_reset()
{
	memset(&m_vc2, 0, sizeof(vc2_t));
	memset(&m_xmap0, 0, sizeof(xmap_t));
	memset(&m_xmap1, 0, sizeof(xmap_t));
	memset(&m_rex3, 0, sizeof(rex3_t));
	memset(&m_cmap0, 0, sizeof(cmap_t));

	m_rex3.m_draw_mode0 = 0x00000000;
	m_rex3.m_draw_mode1 = 0x3002f001;
	m_rex3.m_dcb_mode = 0x00000780;

	m_xmap0.m_entries = 0x2;
	m_xmap1.m_entries = 0x2;
}

uint32_t newport_video_device::get_cursor_pixel(int x, int y)
{
	if (x < 0 || y < 0)
		return 0;

	bool monochrome_cursor = BIT(m_vc2.m_display_ctrl, DCR_CURSOR_SIZE_BIT) == DCR_CURSOR_SIZE_64;

	int size = monochrome_cursor ? 64 : 32;
	if (x >= size || y >= size)
		return 0;

	const int shift = 15 - (x % 16);

	if (monochrome_cursor)
	{
		const int address = y * 4 + (x / 16);
		const uint16_t word = m_vc2.m_ram[m_vc2.m_cursor_entry + address];
		const uint16_t entry = BIT(word, shift);
		return m_cmap0.m_palette[entry];
	}
	else
	{
		const int address = y * 2 + (x / 16);
		const uint16_t word0 = m_vc2.m_ram[m_vc2.m_cursor_entry + address];
		const uint16_t word1 = m_vc2.m_ram[m_vc2.m_cursor_entry + address + 64];
		const uint16_t entry = BIT(word0, shift) | (BIT(word1, shift) << 1);
		return m_cmap0.m_palette[entry];
	}
}

uint32_t newport_video_device::screen_update(screen_device &device, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	bool enable_cursor = BIT(m_vc2.m_display_ctrl, DCR_CURSOR_FUNC_ENABLE_BIT) != 0
					  && BIT(m_vc2.m_display_ctrl, DCR_CURSOR_ENABLE_BIT) != 0
					  && BIT(m_vc2.m_display_ctrl, DCR_CURSOR_MODE_BIT) == DCR_CURSOR_MODE_GLYPH;

	/* loop over rows and copy to the destination */
	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		uint8_t *src = &m_base[1344 * y];
		uint32_t *dest = &bitmap.pix32(y, cliprect.min_x);

		/* loop over columns */
		for (int x = cliprect.min_x; x < cliprect.max_x; x++)
		{
			uint32_t cursor_pixel = 0;
			if (x >= (m_vc2.m_cursor_x - 31) && x <= m_vc2.m_cursor_x && y >= (m_vc2.m_cursor_y - 31) && y <= m_vc2.m_cursor_y && enable_cursor)
			{
				cursor_pixel = get_cursor_pixel(x - ((int)m_vc2.m_cursor_x - 31), y - ((int)m_vc2.m_cursor_y - 31));
			}
			*dest++ = cursor_pixel ? cursor_pixel : m_cmap0.m_palette[*src];
			src++;
		}
	}
	return 0;
}


void newport_video_device::cmap0_write(uint32_t data)
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0x00:
		LOGMASKED(LOG_CMAP0, "CMAP0 Palette Index Write: %04x\n", data & 0xffff);
		m_cmap0.m_palette_idx = (uint16_t)data;
		break;
	case 0x02:
		m_cmap0.m_palette[m_cmap0.m_palette_idx & 0xff] = data >> 8;
		LOGMASKED(LOG_CMAP0, "CMAP0 Palette Entry %04x Write: %08x\n", m_cmap0.m_palette_idx, data >> 8);
		break;
	default:
		LOGMASKED(LOG_CMAP0 | LOG_UNKNOWN, "Unknown CMAP0 Register %d Write: %08x\n", m_rex3.m_dcb_reg_select, data);
		break;
	}
}

uint32_t newport_video_device::cmap0_read()
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0x04:
		LOGMASKED(LOG_CMAP0, "CMAP0 Status Read: %08x\n", 0x8);
		return 0x8;
	case 0x06: /* Revision */
		LOGMASKED(LOG_CMAP0, "CMAP0 Revision Read: CMAP Rev 1, Board Rev 2, 8bpp (000000a1)\n");
		return 0xa1;
	default:
		LOGMASKED(LOG_CMAP0 | LOG_UNKNOWN, "Unknown CMAP0 Register %d Read\n", m_rex3.m_dcb_reg_select);
		return 0;
	}
}

uint32_t newport_video_device::cmap1_read()
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0x04:
		LOGMASKED(LOG_CMAP1, "CMAP1 Status Read: %08x\n", 0x8);
		return 0x8;
	case 0x06: /* Revision */
		LOGMASKED(LOG_CMAP1, "CMAP1 Revision Read: CMAP Rev 1, Board Rev 2, 8bpp (000000a1)\n");
		return 0xa1;
	default:
		LOGMASKED(LOG_CMAP1 | LOG_UNKNOWN, "Unknown CMAP0 Register %d Read\n", m_rex3.m_dcb_reg_select);
		return 0;
	}
}

uint32_t newport_video_device::xmap0_read()
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0:
		LOGMASKED(LOG_XMAP0, "XMAP0 Config Read: %08x\n", m_xmap0.m_config);
		return m_xmap0.m_config;
	case 1:
		LOGMASKED(LOG_XMAP0, "XMAP0 Revision Read: %08x\n", 0);
		return 1;
	case 2:
		LOGMASKED(LOG_XMAP0, "XMAP0 FIFO Availability Read: %08x\n", 0x2);
		return 0x2;
	case 3:
		LOGMASKED(LOG_XMAP0, "XMAP0 Cursor CMAP MSB Read: %08x\n", m_xmap0.m_cursor_cmap);
		return m_xmap0.m_cursor_cmap;
	case 4:
		LOGMASKED(LOG_XMAP0, "XMAP0 Pop Up CMAP MSB Read: %08x\n", m_xmap0.m_popup_cmap);
		return m_xmap0.m_popup_cmap;
	case 5:
	{
		uint8_t mode_idx = (m_xmap0.m_mode_table_idx & 0x7c) >> 2;
		switch (m_xmap0.m_mode_table_idx & 3)
		{
		case 0:
		{
			uint8_t ret = (uint8_t)(m_xmap0.m_mode_table[mode_idx] >> 16);
			LOGMASKED(LOG_XMAP0, "XMAP0 Mode Register Read: %02x (Byte 0): %08x\n", mode_idx, ret);
			return ret;
		}
		case 1:
		{
			uint8_t ret = (uint8_t)(m_xmap0.m_mode_table[mode_idx] >> 8);
			LOGMASKED(LOG_XMAP0, "XMAP0 Mode Register Read: %02x (Byte 1): %08x\n", mode_idx, ret);
			return ret;
		}
		case 2:
		{
			uint8_t ret = (uint8_t)m_xmap0.m_mode_table[mode_idx];
			LOGMASKED(LOG_XMAP0, "XMAP0 Mode Register Read: %02x (Byte 2): %08x\n", mode_idx, ret);
			return ret;
		}
		}
		break;
	}
	case 6:
		LOGMASKED(LOG_XMAP0, "XMAP0 Unused Read: %08x\n", 0);
		return 0;
	case 7:
		LOGMASKED(LOG_XMAP0, "XMAP0 Mode Table Address Read: %08x\n", m_xmap0.m_mode_table_idx);
		return m_xmap0.m_mode_table_idx;
	default:
		LOGMASKED(LOG_XMAP0 | LOG_UNKNOWN, "XMAP0 Unknown DCB Register Select Value: %02x, returning 0\n", m_rex3.m_dcb_reg_select);
		return 0;
	}
	return 0;
}

void newport_video_device::xmap0_write(uint32_t data)
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0:
		LOGMASKED(LOG_XMAP0, "XMAP0 Config Write: %02x\n", (uint8_t)data);
		m_xmap0.m_config = (uint8_t)data;
		break;
	case 1:
		LOGMASKED(LOG_XMAP0, "XMAP0 Revision Write (Ignored): %02x\n", (uint8_t)data);
		break;
	case 2:
		LOGMASKED(LOG_XMAP0, "XMAP0 FIFO Availability Write (Ignored): %02x\n", (uint8_t)data);
		break;
	case 3:
		LOGMASKED(LOG_XMAP0, "XMAP0 Cursor CMAP MSB Write: %02x\n", (uint8_t)data);
		m_xmap0.m_cursor_cmap = (uint8_t)data;
		break;
	case 4:
		LOGMASKED(LOG_XMAP0, "XMAP0 Pop Up CMAP MSB Write: %02x\n", (uint8_t)data);
		m_xmap0.m_popup_cmap = (uint8_t)data;
		break;
	case 5:
		LOGMASKED(LOG_XMAP0, "XMAP0 Mode Register Write: %02x = %06x\n", data >> 24, data & 0xffffff);
		m_xmap0.m_mode_table[data >> 24] = data & 0xffffff;
		break;
	case 6:
		LOGMASKED(LOG_XMAP0, "XMAP0 Unused Write (Ignored): %08x\n", data);
		break;
	case 7:
		LOGMASKED(LOG_XMAP0, "XMAP0 Mode Table Address Write: %02x\n", (uint8_t)data);
		m_xmap0.m_mode_table_idx = (uint8_t)data;
		break;
	default:
		LOGMASKED(LOG_XMAP0 | LOG_UNKNOWN, "XMAP0 Unknown DCB Register Select Value: %02x = %08x\n", m_rex3.m_dcb_reg_select, data);
		break;
	}
}

uint32_t newport_video_device::xmap1_read()
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0:
		LOGMASKED(LOG_XMAP1, "XMAP1 Config Read: %08x\n", m_xmap1.m_config);
		return m_xmap1.m_config;
	case 1:
		LOGMASKED(LOG_XMAP1, "XMAP1 Revision Read: %08x\n", 0);
		return 1;
	case 2:
		LOGMASKED(LOG_XMAP1, "XMAP1 FIFO Availability Read: %08x\n", 0x02);
		return 0x2;
	case 3:
		LOGMASKED(LOG_XMAP1, "XMAP1 Cursor CMAP MSB Read: %08x\n", m_xmap1.m_cursor_cmap);
		return m_xmap1.m_cursor_cmap;
	case 4:
		LOGMASKED(LOG_XMAP1, "XMAP1 Pop Up CMAP MSB Read: %08x\n", m_xmap1.m_popup_cmap);
		return m_xmap1.m_popup_cmap;
	case 5:
	{
		const uint8_t mode_idx = (m_xmap1.m_mode_table_idx & 0x7c) >> 2;
		switch (m_xmap1.m_mode_table_idx & 3)
		{
		case 0:
		{
			uint8_t ret = (uint8_t)(m_xmap1.m_mode_table[mode_idx] >> 16);
			LOGMASKED(LOG_XMAP1, "XMAP1 Mode Register Read: %02x (Byte 0): %08x\n", mode_idx, ret);
			return ret;
		}
		case 1:
		{
			uint8_t ret = (uint8_t)(m_xmap1.m_mode_table[mode_idx] >> 8);
			LOGMASKED(LOG_XMAP1, "XMAP1 Mode Register Read: %02x (Byte 1): %08x\n", mode_idx, ret);
			return ret;
		}
		case 2:
		{
			uint8_t ret = (uint8_t)m_xmap1.m_mode_table[mode_idx];
			LOGMASKED(LOG_XMAP1, "XMAP1 Mode Register Read: %02x (Byte 2): %08x\n", mode_idx, ret);
			return ret;
		}
		}
		break;
	}
	case 6:
		LOGMASKED(LOG_XMAP1, "XMAP1 Unused Read: %08x\n", 0);
		return 0;
	case 7:
		LOGMASKED(LOG_XMAP1, "XMAP1 Mode Table Address Read: %08x\n", m_xmap0.m_mode_table_idx);
		return m_xmap1.m_mode_table_idx;
	default:
		LOGMASKED(LOG_XMAP1 | LOG_UNKNOWN, "XMAP1 Unknown DCB Register Select Value: %02x, returning 0\n", m_rex3.m_dcb_reg_select);
		return 0;
	}
	return 0;
}

void newport_video_device::xmap1_write(uint32_t data)
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0:
		LOGMASKED(LOG_XMAP1, "XMAP1 Config Write: %02x\n", (uint8_t)data);
		m_xmap1.m_config = (uint8_t)data;
		break;
	case 1:
		LOGMASKED(LOG_XMAP1, "XMAP1 Revision Write (Ignored): %02x\n", (uint8_t)data);
		break;
	case 2:
		LOGMASKED(LOG_XMAP1, "XMAP1 FIFO Availability Write (Ignored): %02x\n", (uint8_t)data);
		break;
	case 3:
		LOGMASKED(LOG_XMAP1, "XMAP1 Cursor CMAP MSB Write: %02x\n", (uint8_t)data);
		m_xmap1.m_cursor_cmap = (uint8_t)data;
		break;
	case 4:
		LOGMASKED(LOG_XMAP1, "XMAP1 Pop Up CMAP MSB Write: %02x\n", (uint8_t)data);
		m_xmap1.m_popup_cmap = (uint8_t)data;
		break;
	case 5:
		LOGMASKED(LOG_XMAP1, "XMAP1 Mode Register Write: %02x = %06x\n", data >> 24, data & 0xffffff);
		m_xmap1.m_mode_table[data >> 24] = data & 0xffffff;
		break;
	case 6:
		LOGMASKED(LOG_XMAP1, "XMAP1 Unused Write (Ignored): %08x\n", data);
		break;
	case 7:
		LOGMASKED(LOG_XMAP1, "XMAP1 Mode Table Address Write: %02x\n", (uint8_t)data);
		m_xmap1.m_mode_table_idx = (uint8_t)data;
		break;
	default:
		LOGMASKED(LOG_XMAP0 | LOG_UNKNOWN, "XMAP0 Unknown DCB Register Select Value: %02x = %08x\n", m_rex3.m_dcb_reg_select, data);
		break;
	}
}

uint32_t newport_video_device::vc2_read()
{
	switch (m_rex3.m_dcb_reg_select)
	{
	case 0x01: /* Register Read */
		switch (m_vc2.m_reg_idx)
		{
			case 0x00:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Video Entry Pointer, %08x\n", m_vc2.m_vid_entry);
				return m_vc2.m_vid_entry;
			case 0x01:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Cursor Entry Pointer, %08x\n", m_vc2.m_cursor_entry);
				return m_vc2.m_cursor_entry;
			case 0x02:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Cursor X, %08x\n", m_vc2.m_cursor_x);
				return m_vc2.m_cursor_x;
			case 0x03:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Cursor Y, %08x\n", m_vc2.m_cursor_y);
				return m_vc2.m_cursor_y;
			case 0x04:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Current Cursor X, %08x\n", m_vc2.m_cur_cursor_x);
				return m_vc2.m_cur_cursor_x;
			case 0x05:
				LOGMASKED(LOG_VC2, "VC2 Register Read: DID Entry, %08x\n", m_vc2.m_did_entry);
				return m_vc2.m_did_entry;
			case 0x06:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Scanline Length, %08x\n", m_vc2.m_scanline_len);
				return m_vc2.m_scanline_len;
			case 0x07:
				LOGMASKED(LOG_VC2, "VC2 Register Read: RAM Address, %08x\n", m_vc2.m_ram_addr);
				return m_vc2.m_ram_addr;
			case 0x08:
				LOGMASKED(LOG_VC2, "VC2 Register Read: VT Frame Pointer, %08x\n", m_vc2.m_vt_frame_ptr);
				return m_vc2.m_vt_frame_ptr;
			case 0x09:
				LOGMASKED(LOG_VC2, "VC2 Register Read: VT Line Sequence Pointer, %08x\n", m_vc2.m_vt_line_ptr);
				return m_vc2.m_vt_line_ptr;
			case 0x0a:
				LOGMASKED(LOG_VC2, "VC2 Register Read: VT Lines in Run, %08x\n", m_vc2.m_vt_line_run);
				return m_vc2.m_vt_line_run;
			case 0x0b:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Vertical Line Count, %08x\n", m_vc2.m_vt_line_count);
				return m_vc2.m_vt_line_count;
			case 0x0c:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Cursor Table Pointer, %08x\n", m_vc2.m_cursor_table_ptr);
				return m_vc2.m_cursor_table_ptr;
			case 0x0d:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Working Cursor Y, %08x\n", m_vc2.m_work_cursor_y);
				return m_vc2.m_work_cursor_y;
			case 0x0e:
				LOGMASKED(LOG_VC2, "VC2 Register Read: DID Frame Pointer, %08x\n", m_vc2.m_did_frame_ptr);
				return m_vc2.m_did_frame_ptr;
			case 0x0f:
				LOGMASKED(LOG_VC2, "VC2 Register Read: DID Line Pointer, %08x\n", m_vc2.m_did_line_ptr);
				return m_vc2.m_did_line_ptr;
			case 0x10:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Display Control, %08x\n", m_vc2.m_display_ctrl);
				return m_vc2.m_display_ctrl;
			case 0x1f:
				LOGMASKED(LOG_VC2, "VC2 Register Read: Configuration, %08x\n", m_vc2.m_config);
				return m_vc2.m_config;
			default:
				return 0;
		}
		break;
	case 0x03: /* RAM Read */
	{
		LOGMASKED(LOG_VC2, "VC2 RAM Read: %04x = %08x\n", m_vc2.m_ram_addr, m_vc2.m_ram[m_vc2.m_ram_addr]);
		uint16_t ret = m_vc2.m_ram[m_vc2.m_ram_addr];
		m_vc2.m_ram_addr++;
		if (m_vc2.m_ram_addr == 0x8000)
		{
			m_vc2.m_ram_addr = 0x0000;
		}
		return ret;
	}
	default:
		LOGMASKED(LOG_VC2 | LOG_UNKNOWN, "Unknown VC2 Register Read: %02x\n", m_rex3.m_dcb_reg_select);
		return 0;
	}
}

void newport_video_device::vc2_write(uint32_t data)
{
	switch (m_rex3.m_xfer_width)
	{
	case 0x01: /* Register Select */
		switch (m_rex3.m_dcb_reg_select)
		{
		case 0x00:
			m_vc2.m_reg_idx = (uint8_t)data;
			LOGMASKED(LOG_VC2, "VC2 Register Select: %02x\n", m_vc2.m_reg_idx);
			break;
		default:
			LOGMASKED(LOG_VC2 | LOG_UNKNOWN, "Unknown VC2 Register Select: DCB Register %02x, data = %08x\n", m_rex3.m_dcb_reg_select, data);
			break;
		}
		break;
	case 0x02: /* RAM Write */
		switch (m_rex3.m_dcb_reg_select)
		{
		case 0x03:
			LOGMASKED(LOG_VC2, "VC2 RAM Write: %04x = %08x\n", m_vc2.m_ram_addr, (uint16_t)data);
			m_vc2.m_ram[m_vc2.m_ram_addr] = (uint16_t)data;
			m_vc2.m_ram_addr++;
			if (m_vc2.m_ram_addr >= 0x8000)
			{
				m_vc2.m_ram_addr = 0x0000;
			}
			break;
		default:
			LOGMASKED(LOG_VC2 | LOG_UNKNOWN, "Unknown Word Write: DCB Register %02x, data = %08x\n", m_rex3.m_dcb_reg_select, data);
			break;
		}
		break;
	case 0x03: /* Register Write */
		switch (m_rex3.m_dcb_reg_select)
		{
		case 0x00:
			LOGMASKED(LOG_VC2, "VC2 Register Setup:\n");
			m_vc2.m_reg_idx = data >> 24;
			m_vc2.m_reg_data = (uint16_t)(data >> 8);
			switch (m_vc2.m_reg_idx)
			{
				case 0x00:
					m_vc2.m_vid_entry = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Video Entry Pointer, %04x\n", m_vc2.m_vid_entry);
					break;
				case 0x01:
					m_vc2.m_cursor_entry = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Cursor Entry Pointer, %04x\n", m_vc2.m_cursor_entry);
					break;
				case 0x02:
					m_vc2.m_cursor_x = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Cursor X, %04x\n", m_vc2.m_cursor_x);
					break;
				case 0x03:
					m_vc2.m_cursor_y = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Cursor Y, %04x\n", m_vc2.m_cursor_y);
					m_vc2.m_cur_cursor_x = m_vc2.m_cursor_x;
					break;
				case 0x04:
					m_vc2.m_cur_cursor_x = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Current Cursor X, %04x\n", m_vc2.m_cur_cursor_x);
					break;
				case 0x05:
					m_vc2.m_did_entry = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: DID Entry Pointer, %04x\n", m_vc2.m_did_entry);
					break;
				case 0x06:
					m_vc2.m_scanline_len = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Scanline Length, %04x\n", m_vc2.m_scanline_len);
					break;
				case 0x07:
					m_vc2.m_ram_addr = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: RAM Address, %04x\n", m_vc2.m_ram_addr);
					break;
				case 0x08:
					m_vc2.m_vt_frame_ptr = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: VT Frame Table Ptr, %04x\n", m_vc2.m_vt_frame_ptr);
					break;
				case 0x09:
					m_vc2.m_vt_line_ptr = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: VT Line Sequence Pointer, %04x\n", m_vc2.m_vt_line_ptr);
					break;
				case 0x0a:
					m_vc2.m_vt_line_run = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: VT Lines in Run, %04x\n", m_vc2.m_vt_line_run);
					break;
				case 0x0b:
					m_vc2.m_vt_line_count = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Vertical Line Count, %04x\n", m_vc2.m_vt_line_count);
					break;
				case 0x0c:
					m_vc2.m_cursor_table_ptr = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Cursor Table Pointer, %04x\n", m_vc2.m_cursor_table_ptr);
					break;
				case 0x0d:
					m_vc2.m_work_cursor_y = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Working Cursor Y, %04x\n", m_vc2.m_work_cursor_y);
					break;
				case 0x0e:
					m_vc2.m_did_frame_ptr = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: DID Frame Table Pointer, %04x\n", m_vc2.m_did_frame_ptr);
					break;
				case 0x0f:
					m_vc2.m_did_line_ptr = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: DID Line Table Pointer, %04x\n", m_vc2.m_did_line_ptr);
					break;
				case 0x10:
					m_vc2.m_display_ctrl = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Display Control, %04x\n", m_vc2.m_display_ctrl);
					break;
				case 0x1f:
					m_vc2.m_config = m_vc2.m_reg_data;
					LOGMASKED(LOG_VC2, "VC2 Register Write: Configuration, %04x\n", m_vc2.m_config);
					break;
				default:
					LOGMASKED(LOG_VC2 | LOG_UNKNOWN, "VC2 Register Write: Unknown VC2 Register: %02x = %04x\n", m_vc2.m_reg_idx, m_vc2.m_reg_data);
					break;
			}
			break;
		default:
			LOGMASKED(LOG_VC2 | LOG_UNKNOWN, "Unknown VC2 Register Write: %02x = %08x\n", m_rex3.m_dcb_reg_select, data);
			break;
		}
		break;
	default:
		LOGMASKED(LOG_VC2 | LOG_UNKNOWN, "Unknown VC2 Transfer Width: Width %02x, DCB Register %02x, Value %08x\n", m_rex3.m_xfer_width, m_rex3.m_dcb_reg_select, data);
		break;
	}
}

WRITE_LINE_MEMBER(newport_video_device::vblank_w)
{
	if (state)
	{
		m_rex3.m_status |= 0x20;
		if (BIT(m_vc2.m_display_ctrl, 0))
			m_hpc3->raise_local_irq(1, ioc2_device::INT3_LOCAL1_RETRACE);
	}
	else
	{
		m_rex3.m_status &= ~0x20;
	}
}

READ64_MEMBER(newport_video_device::rex3_r)
{
	uint64_t ret = 0;
	switch (offset & ~(0x800/8))
	{
	case 0x0000/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Draw Mode 1 Read: %08x\n", m_rex3.m_draw_mode1);
			ret |= (uint64_t)m_rex3.m_draw_mode1 << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Draw Mode 0 Read: %08x\n", m_rex3.m_draw_mode0);
			ret |= m_rex3.m_draw_mode0;
		}
		break;
	case 0x0008/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Line Stipple Mode Read: %08x\n", m_rex3.m_ls_mode);
			ret |= (uint64_t)m_rex3.m_ls_mode << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Line Stipple Pattern Read: %08x\n", m_rex3.m_ls_pattern);
			ret |= m_rex3.m_ls_pattern;
		}
		break;
	case 0x0010/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Line Stipple Pattern (Save) Read: %08x\n", m_rex3.m_ls_pattern_saved);
			ret |= (uint64_t)m_rex3.m_ls_pattern_saved << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Pattern Register Read: %08x\n", m_rex3.m_z_pattern);
			ret |= m_rex3.m_z_pattern;
		}
		break;
	case 0x0018/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Opaque Pattern / Blendfunc Dest Color Read: %08x\n", m_rex3.m_color_back);
			ret |= (uint64_t)m_rex3.m_color_back << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 VRAM Fastclear Color Read: %08x\n", m_rex3.m_color_vram);
			ret |= m_rex3.m_color_vram;
		}
		break;
	case 0x0020/8:
		LOGMASKED(LOG_REX3, "REX3 AFUNCTION Reference Alpha Read: %08x\n", m_rex3.m_alpha_ref);
		ret |= (uint64_t)m_rex3.m_alpha_ref << 32;
		break;
	case 0x0028/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 0 X Min/Max Read: %08x\n", m_rex3.m_smask_x[0]);
			ret |= (uint64_t)m_rex3.m_smask_x[0] << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 0 Y Min/Max Read: %08x\n", m_rex3.m_smask_y[0]);
			ret |= m_rex3.m_smask_y[0];
		}
		break;
	case 0x0030/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Line/Span Setup Read: %08x\n", m_rex3.m_setup);
			ret |= (uint64_t)m_rex3.m_setup << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 ZPattern Enable Read: %08x\n", m_rex3.m_step_z);
			ret |= m_rex3.m_step_z;
		}
		break;
	case 0x0100/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 X Start Read: %08x\n", m_rex3.m_x_start);
			ret |= (uint64_t)m_rex3.m_x_start << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 YStart Read: %08x\n", m_rex3.m_y_start);
			ret |= m_rex3.m_y_start;
		}
		break;
	case 0x0108/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XEnd Read: %08x\n", m_rex3.m_x_end);
			ret |= (uint64_t)m_rex3.m_x_end << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 YEnd Read: %08x\n", m_rex3.m_y_end);
			ret |= m_rex3.m_y_end;
		}
		break;
	case 0x0110/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XSave Read: %08x\n", m_rex3.m_x_save);
			ret |= (uint64_t)m_rex3.m_x_save << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 XYMove Read: %08x\n", m_rex3.m_xy_move);
			ret |= m_rex3.m_xy_move;
		}
		break;
	case 0x0118/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham D Read: %08x\n", m_rex3.m_bres_d);
			ret |= (uint64_t)m_rex3.m_bres_d << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham S1 Read: %08x\n", m_rex3.m_bres_s1);
			ret |= m_rex3.m_bres_s1;
		}
		break;
	case 0x0120/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham Octant & Incr1 Read: %08x\n", m_rex3.m_bres_octant_inc1);
			ret |= (uint64_t)m_rex3.m_bres_octant_inc1 << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham Octant Rounding Mode & Incr2 Read: %08x\n", m_rex3.m_bres_round_inc2);
			ret |= m_rex3.m_bres_round_inc2;
		}
		break;
	case 0x0128/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham E1 Read: %08x\n", m_rex3.m_bres_e1);
			ret |= (uint64_t)m_rex3.m_bres_e1 << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham S2 Read: %08x\n", m_rex3.m_bres_s2);
			ret |= m_rex3.m_bres_s2;
		}
		break;
	case 0x0130/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 AA Line Weight Table 1/2 Read: %08x\n", m_rex3.m_a_weight0);
			ret |= (uint64_t)m_rex3.m_a_weight0 << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 AA Line Weight Table 2/2 Read: %08x\n", m_rex3.m_a_weight1);
			ret |= m_rex3.m_a_weight1;
		}
		break;
	case 0x0138/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 GL XStart Read: %08x\n", m_rex3.m_x_start_f);
			ret |= (uint64_t)m_rex3.m_x_start_f << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 GL YStart Read: %08x\n", m_rex3.m_y_start_f);
			ret |= m_rex3.m_y_start_f;
		}
		break;
	case 0x0140/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 GL XEnd Read: %08x\n", m_rex3.m_x_end_f);
			ret |= (uint64_t)m_rex3.m_x_end_f << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 GL YEnd Read: %08x\n", m_rex3.m_y_end_f);
			ret |= m_rex3.m_y_end_f;
		}
		break;
	case 0x0148/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XStart (integer) Read: %08x\n", m_rex3.m_x_start_i);
			ret |= (uint64_t)m_rex3.m_x_start_i << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 GL XEnd (copy) Read: %08x\n", m_rex3.m_x_end_f);
			ret |= m_rex3.m_x_end_f;
		}
		break;
	case 0x0150/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XYStart (integer) Read: %08x\n", m_rex3.m_xy_start_i);
			ret |= (uint64_t)m_rex3.m_xy_start_i << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 XYEnd (integer) Read: %08x\n", m_rex3.m_xy_end_i);
			ret |= m_rex3.m_xy_end_i;
		}
		break;
	case 0x0158/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XStartEnd (integer) Read: %08x\n", m_rex3.m_x_start_end_i);
			ret |= (uint64_t)m_rex3.m_x_start_end_i << 32;
		}
		break;
	case 0x0200/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Red/CI Full State Read: %08x\n", m_rex3.m_color_red);
			ret |= (uint64_t)m_rex3.m_color_red << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Alpha Full State Read: %08x\n", m_rex3.m_color_alpha);
			ret |= m_rex3.m_color_alpha;
		}
		break;
	case 0x0208/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Green Full State Read: %08x\n", m_rex3.m_color_green);
			ret |= (uint64_t)m_rex3.m_color_green << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Blue Full State Read: %08x\n", m_rex3.m_color_blue);
			ret |= m_rex3.m_color_blue;
		}
		break;
	case 0x0210/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Red/CI Slope Read: %08x\n", m_rex3.m_slope_red);
			ret |= (uint64_t)m_rex3.m_slope_red << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Alpha Slope Read: %08x\n", m_rex3.m_slope_alpha);
			ret |= m_rex3.m_slope_alpha;
		}
		break;
	case 0x0218/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Green Slope Read: %08x\n", m_rex3.m_slope_green);
			ret |= (uint64_t)m_rex3.m_slope_green << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Blue Slope Read: %08x\n", m_rex3.m_slope_blue);
			ret |= m_rex3.m_slope_blue;
		}
		break;
	case 0x0220/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Write Mask Read: %08x\n", m_rex3.m_write_mask);
			ret |= (uint64_t)m_rex3.m_write_mask << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Packed Color Fractions Read: %08x\n", m_rex3.m_zero_fract);
			ret |= m_rex3.m_zero_fract;
		}
		break;
	case 0x0228/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Color Index Zeros Overflow Read: %08x\n", m_rex3.m_zero_overflow);
			ret |= (uint64_t)m_rex3.m_zero_overflow << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Red/CI Slope (copy) Read: %08x\n", m_rex3.m_slope_red);
			ret |= m_rex3.m_slope_red;
		}
		break;
	case 0x0230/8:
		if (m_rex3.m_read_active)
			m_rex3.m_host_dataport = do_pixel_word_read();
		LOGMASKED(LOG_REX3, "REX3 Host Data Port Read: %08x%08x\n", (uint32_t)(m_rex3.m_host_dataport >> 32), (uint32_t)m_rex3.m_host_dataport);
		ret = m_rex3.m_host_dataport;
		break;
	case 0x0238/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Display Control Bus Mode Read: %08x\n", m_rex3.m_dcb_mode);
			ret |= (uint64_t)m_rex3.m_dcb_mode << 32;
		}
		break;
	case 0x0240/8:
		if (ACCESSING_BITS_32_63)
		{
			switch (m_rex3.m_dcb_slave_select)
			{
			case 0x00:
				ret |= (uint64_t)vc2_read() << 32;
				break;
			case 0x02:
				ret |= (uint64_t)cmap0_read() << 32;
				break;
			case 0x03:
				ret |= (uint64_t)cmap1_read() << 32;
				break;
			case 0x05:
				ret |= (uint64_t)xmap0_read() << 32;
				break;
			case 0x06:
				ret |= (uint64_t)xmap1_read() << 32;
				break;
			default:
				LOGMASKED(LOG_REX3, "REX3 Display Control Bus Data MSW Read: %08x\n", m_rex3.m_dcb_data_msw);
				ret |= (uint64_t)m_rex3.m_dcb_data_msw << 32;
				break;
			}
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Display Control Bus Data LSW Read: %08x\n", m_rex3.m_dcb_data_lsw);
			ret |= m_rex3.m_dcb_data_lsw;
		}
		break;
	case 0x1300/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 1 X Min/Max Read: %08x\n", m_rex3.m_smask_x[1]);
			ret |= (uint64_t)m_rex3.m_smask_x[1] << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 1 Y Min/Max Read: %08x\n", m_rex3.m_smask_y[1]);
			ret |= m_rex3.m_smask_y[1];
		}
		break;
	case 0x1308/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 2 X Min/Max Read: %08x\n", m_rex3.m_smask_x[2]);
			ret |= (uint64_t)m_rex3.m_smask_x[2] << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 2 Y Min/Max Read: %08x\n", m_rex3.m_smask_y[2]);
			ret |= m_rex3.m_smask_y[2];
		}
		break;
	case 0x1310/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 3 X Min/Max Read: %08x\n", m_rex3.m_smask_x[3]);
			ret |= (uint64_t)m_rex3.m_smask_x[3] << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 3 Y Min/Max Read: %08x\n", m_rex3.m_smask_y[3]);
			ret |= m_rex3.m_smask_y[3];
		}
		break;
	case 0x1318/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 4 X Min/Max Read: %08x\n", m_rex3.m_smask_x[4]);
			ret |= (uint64_t)m_rex3.m_smask_x[4] << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 4 Y Min/Max Read: %08x\n", m_rex3.m_smask_y[4]);
			ret |= m_rex3.m_smask_y[4];
		}
		break;
	case 0x1320/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Top of Screen Scanline Read: %08x\n", m_rex3.m_top_scanline);
			ret |= (uint64_t)m_rex3.m_top_scanline << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 XY Window Read: %08x\n", m_rex3.m_xy_window);
			ret |= m_rex3.m_xy_window;
		}
		break;
	case 0x1328/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Clipping Mode Read: %08x\n", m_rex3.m_clip_mode);
			ret |= (uint64_t)m_rex3.m_clip_mode << 32;
		}
		break;
	case 0x1330/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Config Read: %08x\n", m_rex3.m_config);
			ret |= (uint64_t)m_rex3.m_config << 32;
		}
		break;
	case 0x1338/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Status Read: %08x\n", m_rex3.m_status);
			uint32_t old_status = m_rex3.m_status;
			m_rex3.m_status = 0;
			m_hpc3->lower_local_irq(1, ioc2_device::INT3_LOCAL1_RETRACE);
			ret |= (uint64_t)(old_status | 3) << 32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 User Status Read: %08x\n", m_rex3.m_status);
			ret |= m_rex3.m_status;
		}
		break;
	default:
		LOGMASKED(LOG_REX3 | LOG_UNKNOWN, "Unknown REX3 Read: %08x (%08x%08x)\n", 0x1f0f0000 + (offset << 2), (uint32_t)(mem_mask >> 32), (uint32_t)mem_mask);
		return 0;
	}
	return ret;
}

void newport_video_device::write_pixel(uint32_t x, uint32_t y, uint8_t color)
{
	if (m_rex3.m_clip_mode & 0x1f)
	{
		for (uint8_t bit = 0; bit < 5; bit++)
		{
			if (!BIT(m_rex3.m_clip_mode, bit))
				continue;
			if (x < ((m_rex3.m_smask_x[bit] >> 16) & 0x0fff))
				return;
			if (x > (m_rex3.m_smask_x[bit] & 0x0fff))
				return;
			if (y < ((m_rex3.m_smask_y[bit] >> 16) & 0x0fff))
				return;
			if (y > (m_rex3.m_smask_y[bit] & 0x0fff))
				return;
		}
	}
	if (x > 1280 || y > 1024)
	{
		logerror("Warning: Attempting to write pixel to %d,%d - rejecting\n", x, y);
		return;
	}
	m_base[y * (1280 + 64) + x] = color;
}

void newport_video_device::do_v_iline(uint16_t x1, uint16_t y1, uint16_t y2, uint8_t color, bool skip_last)
{
	uint16_t window_x = (m_rex3.m_xy_window >> 16) & 0x0fff;
	uint16_t window_y = m_rex3.m_xy_window & 0x0fff;

	x1 += window_x;
	y1 += window_y;
	y2 += window_y;

	m_rex3.m_iter_x = x1;
	m_rex3.m_iter_y = y1;
	int incy = (y2 < y1) ? -1 : 1;

	if (skip_last)
		y2 -= incy;

	do
	{
		write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, color);
		m_rex3.m_iter_y += incy;
	} while (m_rex3.m_iter_y != y2);

	m_rex3.m_iter_x -= window_x;
	m_rex3.m_iter_y -= window_y;

	m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
	m_rex3.m_x_start_i = m_rex3.m_iter_x;
	m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
	m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
}

void newport_video_device::do_h_iline(uint16_t x1, uint16_t y1, uint16_t x2, uint8_t color, bool skip_last)
{
	uint16_t window_x = (m_rex3.m_xy_window >> 16) & 0x0fff;
	uint16_t window_y = m_rex3.m_xy_window & 0x0fff;

	x1 += window_x;
	x2 += window_x;
	y1 += window_y;

	m_rex3.m_iter_x = x1;
	m_rex3.m_iter_y = y1;

	if (skip_last)
		x2--;

	int addr = m_rex3.m_iter_y * (1280 + 64) + m_rex3.m_iter_x;
	do
	{
		write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, color);
		addr++;
		m_rex3.m_iter_x++;
	} while (m_rex3.m_iter_x != x2);

	m_rex3.m_iter_x -= (m_rex3.m_xy_window >> 16) & 0x0fff;
	m_rex3.m_iter_y -= m_rex3.m_xy_window & 0x0fff;

	m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
	m_rex3.m_x_start_i = m_rex3.m_iter_x;
	m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
	m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
}

void newport_video_device::do_iline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, bool skip_last)
{
	uint16_t window_x = (m_rex3.m_xy_window >> 16) & 0x0fff;
	uint16_t window_y = m_rex3.m_xy_window & 0x0fff;

	x1 += window_x;
	y1 += window_y;
	x2 += window_x;
	y2 += window_y;

	unsigned char c1 = 0;
	int incy = 1;

	int dx;
	if (x2 > x1)
		dx = x2 - x1;
	else
		dx = x1 - x2;

	int dy;
	if (y2 > y1)
		dy = y2 - y1;
	else
		dy = y1 - y2;

	int t;
	if (dy > dx)
	{
		t = y2;
		y2 = x2;
		x2 = t;

		t = y1;
		y1 = x1;
		x1 = t;

		t = dx;
		dx = dy;
		dy = t;

		c1 = 1;
	}

	if (x1 > x2)
	{
		t = y2;
		y2 = y1;
		y1 = t;

		t = x1;
		x1 = x2;
		x2 = t;
	}

	int horiz = dy << 1;
	int diago = (dy - dx) << 1;
	int e = (dy << 1) - dx;

	if (y1 <= y2)
		incy = 1;
	else
		incy = -1;

	m_rex3.m_iter_x = x1;
	m_rex3.m_iter_y = y1;

	if (c1)
	{
		do
		{
			write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, color);

			if (e > 0)
			{
				m_rex3.m_iter_y = m_rex3.m_iter_y + incy;
				e = e + diago;
			}
			else
			{
				e = e + horiz;
			}

			m_rex3.m_iter_x++;
		} while (skip_last ? m_rex3.m_iter_x < x2 : m_rex3.m_iter_x <= x2);
	}
	else
	{
		do
		{
			write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, color);

			if (e > 0)
			{
				m_rex3.m_iter_y = m_rex3.m_iter_y + incy;
				e = e + diago;
			}
			else
			{
				e = e + horiz;
			}

			m_rex3.m_iter_x++;
		} while (skip_last ? m_rex3.m_iter_x < x2 : m_rex3.m_iter_x <= x2);
	}

	m_rex3.m_iter_x -= window_x;
	m_rex3.m_iter_y -= window_y;

	m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
	m_rex3.m_x_start_i = m_rex3.m_iter_x;
	m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
	m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
}

uint8_t newport_video_device::do_pixel_read()
{
	if (m_rex3.m_xy_start_i == m_rex3.m_xy_end_i)
		m_rex3.m_read_active = false;
	LOGMASKED(LOG_COMMANDS, "Reading from %04x, %04x\n", m_rex3.m_iter_x, m_rex3.m_iter_y);
	m_rex3.m_bres_octant_inc1 = 0;
	const uint8_t ret = m_base[m_rex3.m_iter_y * (1280 + 64) + m_rex3.m_iter_x];
	m_rex3.m_iter_x++;
	if (m_rex3.m_iter_x > (m_rex3.m_xy_end_i >> 16))
	{
		m_rex3.m_iter_y++;
		m_rex3.m_iter_x = m_rex3.m_x_save;
	}
	m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
	m_rex3.m_x_start_i = m_rex3.m_iter_x;
	m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
	m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
	return ret;
}

uint32_t newport_video_device::do_pixel_word_read()
{
	uint32_t ret = 0;
	uint16_t x_start = (uint16_t)(m_rex3.m_xy_start_i >> 16);
	uint16_t x_end = (uint16_t)(m_rex3.m_xy_end_i >> 16);
	uint16_t width = (x_end - x_start) + 1;
	if (width > 4)
		width = 4;
	uint32_t shift = 24;
	for (uint16_t i = 0; i < width; i++)
	{
		ret |= do_pixel_read() << shift;
		shift -= 8;
	}
	return ret;
}

void newport_video_device::do_rex3_command()
{
	static const char* const s_opcode_str[4] = { "Noop", "Read", "Draw", "Scr2Scr" };
	static const char* const s_adrmode_str[8] = {
		"Span", "Block", "IntLine", "FracLine", "AALine", "Unk5", "Unk6", "Unk7"
	};
	static const char* const s_planes_str[8] = {
		"None", "RGB/CI", "RGBA", "Unk3", "OLAY", "PUP", "CID", "Unk7"
	};
	static const char* const s_drawdepth_str[4] = {
		"4 bits", "8 bits", "12 bits", "24 bits"
	};
	static const char* const s_hostdepth_str[4] = {
		"4 bits (1-2-1 BGR or 4 CI)", "8 bits (3-3-2 BGR or 8 CI)", "12 bits (4-4-4 BGR or 12 CI)", "32 bits (8-8-8-8 ABGR)"
	};
	static const char* const s_compare_str[8] = {
		"Always", "src < dst", "src = dst", "src <= dst", "src > dst", "src != dst", "src >= dst", "Never"
	};
	static const char* const s_sfactor_str[8] = {
		"0", "1", "dstc", "1-dstc", "srca", "1-srca", "Unk6", "Unk7"
	};
	static const char* const s_dfactor_str[8] = {
		"0", "1", "srcc", "1-srcc", "srca", "1-srca", "Unk6", "Unk7"
	};
	static const char* const s_logicop_str[16] = {
		"0", "src & dst", "src & !dst", "src", "!src & dst", "dst", "src ^ dst", "src | dst",
		"!(src | dst)", "!(src ^ dst)", "!dst", "src | !dst", "!src", "!src | dst", "!(src & dst)", "1"
	};

	const uint32_t mode0 = m_rex3.m_draw_mode0;
	const uint32_t mode1 = m_rex3.m_draw_mode1;

	m_rex3.m_iter_x = BIT(mode0, 5) ? (int16_t)(m_rex3.m_xy_start_i >> 16) : m_rex3.m_iter_x;
	m_rex3.m_iter_y = BIT(mode0, 5) ? (int16_t)m_rex3.m_xy_start_i : m_rex3.m_iter_y;
	int16_t end_x = (int16_t)(m_rex3.m_xy_end_i >> 16);
	int16_t end_y = (int16_t)m_rex3.m_xy_end_i;
	int16_t dx = m_rex3.m_iter_x > end_x ? -1 : 1;
	int16_t dy = m_rex3.m_iter_y > end_y ? -1 : 1;

	LOGMASKED(LOG_COMMANDS, "REX3 Command: %08x|%08x - %s %s:\n", mode0, mode1, s_opcode_str[mode0 & 3], s_adrmode_str[(mode0 >> 2) & 7]);
	LOGMASKED(LOG_COMMANDS, "              DoSetup:%d, ColorHost:%d, AlphaHost:%d, StopOnX:%d, StopOnY:%d\n", BIT(mode0, 5), BIT(mode0, 6), BIT(mode0, 7),
		BIT(mode0, 8), BIT(mode0, 9));
	LOGMASKED(LOG_COMMANDS, "              SkipFirst:%d, SkipLast:%d, ZPattEn:%d, LSPattEn:%d, LSAdvLast:%d\n", BIT(mode0, 10), BIT(mode0, 11), BIT(mode0, 12),
		BIT(mode0, 13), BIT(mode0, 14));
	LOGMASKED(LOG_COMMANDS, "              Length32:%d, ZOpaque:%d, LSOpaque:%d, Shade:%d, LROnly:%d\n", BIT(mode0, 15), BIT(mode0, 16), BIT(mode0, 17),
		BIT(mode0, 18), BIT(mode0, 19));
	LOGMASKED(LOG_COMMANDS, "              XYOffset:%d, CIClamp:%d, EndFilter:%d, YStride:%d\n", BIT(mode0, 20), BIT(mode0, 21), BIT(mode0, 22), BIT(mode0, 23));
	LOGMASKED(LOG_COMMANDS, "              Planes:%s, DrawDepth:%s, DblSrc:%d\n", s_planes_str[mode1 & 7], s_drawdepth_str[(mode1 >> 3) & 3], BIT(mode1, 5));
	LOGMASKED(LOG_COMMANDS, "              GL YFlip:%d, RWPacked:%d, HostDepth:%s\n", BIT(mode1, 6), BIT(mode1, 7), s_hostdepth_str[(mode1 >> 8) & 3]);
	LOGMASKED(LOG_COMMANDS, "              RWDouble:%d, SwapEndian:%d, Compare:%s\n", BIT(mode1, 10), BIT(mode1, 11), s_compare_str[(mode1 >> 12) & 7]);
	LOGMASKED(LOG_COMMANDS, "              RGBMode:%d, Dither:%d, FastClear:%d, Blend:%d\n", BIT(mode1, 15), BIT(mode1, 16), BIT(mode1, 17), BIT(mode1, 18));
	LOGMASKED(LOG_COMMANDS, "              SrcFactor:%s, DstFactor:%s, BackBlend:%d, Prefetch:%d\n", s_sfactor_str[(mode1 >> 19) & 7], s_dfactor_str[(mode1 >> 22) & 7],
		BIT(mode1, 25), BIT(mode1, 26));
	LOGMASKED(LOG_COMMANDS, "              BlendAlpha:%d, LogicOp:%s\n", BIT(mode1, 27), s_logicop_str[(mode1 >> 28) & 15]);

	switch (mode0)
	{
	case 0x00000006: // Block, Draw
	{
		LOGMASKED(LOG_COMMANDS, "%04x, %04x = %02x\n", m_rex3.m_iter_x, m_rex3.m_iter_y, m_rex3.m_zero_fract & 0xff);
		m_rex3.m_bres_octant_inc1 = 0;
		write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, m_rex3.m_zero_fract & 0xff);
		m_rex3.m_iter_x++;
		if (m_rex3.m_iter_x > (m_rex3.m_xy_end_i >> 16))
		{
			m_rex3.m_iter_y++;
			m_rex3.m_iter_x = m_rex3.m_x_save;
		}
		m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
		m_rex3.m_x_start_i = m_rex3.m_iter_x;
		m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
		m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
		break;
	}
	case 0x00000046: // ColorHost, Block, Draw
	{
		m_rex3.m_bres_octant_inc1 = 0;
		if (BIT(mode1, 7)) // Packed
		{
			const bool doubleword = BIT(mode1, 10);
			uint16_t remaining_length = ((m_rex3.m_xy_end_i >> 16) - m_rex3.m_iter_x) + 1;
			uint16_t length = doubleword ? 8 : 4;
			if (remaining_length < length)
				length = remaining_length;
			LOGMASKED(LOG_COMMANDS, "%04x, %04x = %08x%08x\n", m_rex3.m_iter_x, m_rex3.m_iter_y, (uint32_t)(m_rex3.m_host_dataport >> 32), (uint32_t)m_rex3.m_host_dataport);
			uint64_t shift = 56;
			for (uint16_t i = 0; i < length; i++)
			{
				write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, (uint8_t)(m_rex3.m_host_dataport >> shift));
				m_rex3.m_iter_x++;
				shift -= 8;
			}
		}
		else
		{
			LOGMASKED(LOG_COMMANDS, "%04x, %04x = %02x\n", m_rex3.m_iter_x, m_rex3.m_iter_y, (uint8_t)(m_rex3.m_host_dataport >> 56));
			write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, m_rex3.m_host_dataport >> 56);
			m_rex3.m_iter_x++;
		}
		if (m_rex3.m_iter_x > (m_rex3.m_xy_end_i >> 16))
		{
			m_rex3.m_iter_y++;
			m_rex3.m_iter_x = m_rex3.m_x_save;
		}
		m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
		m_rex3.m_x_start_i = m_rex3.m_iter_x;
		m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
		m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
		break;
	}
	case 0x00000045: // ColorHost, Block, Read
	{
		m_rex3.m_read_active = true;
		break;
	}
	case 0x00000102: // StopOnX, Span, Draw
	case 0x00000122: // StopOnX, DoSetup, Span, Draw
	{
		end_x += dx;
		end_y += dy;

		uint32_t color = m_rex3.m_zero_fract & 0xff;
		LOGMASKED(LOG_COMMANDS, "%04x, %04x to %04x, %04x = %08x\n", m_rex3.m_iter_x, m_rex3.m_iter_y, end_x, end_y, color);
		for (; m_rex3.m_iter_x != end_x; m_rex3.m_iter_x += dx)
		{
			write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, color);
		}
		m_rex3.m_iter_y++;

		m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
		m_rex3.m_x_start_i = m_rex3.m_iter_x;
		m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
		m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
		break;
	}
	case 0x00000326: // StopOnX, StopOnY, DoSetup, Block, Draw
	{
		end_x += dx;
		end_y += dy;

		uint32_t color = m_rex3.m_zero_fract & 0xff;
		if (BIT(mode1, 17))
		{
			color = m_rex3.m_color_vram & 0xff;
		}
		LOGMASKED(LOG_COMMANDS, "%04x, %04x to %04x, %04x = %08x\n", m_rex3.m_iter_x, m_rex3.m_iter_y, end_x, end_y, m_cmap0.m_palette[color]);
		for (; m_rex3.m_iter_y != end_y; m_rex3.m_iter_y += dy)
		{
			for (m_rex3.m_iter_x = m_rex3.m_x_start_i; m_rex3.m_iter_x != end_x; m_rex3.m_iter_x += dx)
			{
				write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, color);
			}
		}

		m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
		m_rex3.m_x_start_i = m_rex3.m_iter_x;
		m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
		m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
		break;
	}
	case 0x00000327: // StopOnX, StopOnY, DoSetup, Block, Scr2Scr
	{
		int16_t move_x = (int16_t)((m_rex3.m_xy_move >> 16) & 0x0000ffff);
		int16_t move_y = (int16_t)m_rex3.m_xy_move;
		end_x += dx;
		end_y += dy;
		LOGMASKED(LOG_COMMANDS, "%04x, %04x - %04x, %04x to %04x, %04x\n", m_rex3.m_iter_x, m_rex3.m_iter_y, end_x, end_y,
			m_rex3.m_iter_x + move_x, m_rex3.m_iter_y + move_y);
		for (; m_rex3.m_iter_y != end_y; m_rex3.m_iter_y += dy)
		{
			for (m_rex3.m_iter_x = m_rex3.m_x_start_i; m_rex3.m_iter_x != end_x; m_rex3.m_iter_x += dx)
			{
				write_pixel(m_rex3.m_iter_x + move_x, m_rex3.m_iter_y + move_y, m_base[m_rex3.m_iter_y * (1280 + 64) + m_rex3.m_iter_x]);
			}
		}
		break;
	}
	case 0x0000032a: // StopOnX, StopOnY, DoSetup, I_Line, Draw
	case 0x00000b2a: // SkipLast, StopOnX, StopOnY, DoSetup, I_Line, Draw
	{
		LOGMASKED(LOG_COMMANDS, "ILine: %04x, %04x to %04x, %04x = %08x\n", m_rex3.m_iter_x, m_rex3.m_iter_y, end_x, end_y, m_cmap0.m_palette[m_rex3.m_zero_fract]);
		if (m_rex3.m_iter_x == end_x)
		{
			do_v_iline(m_rex3.m_iter_x, m_rex3.m_iter_y, end_y, m_cmap0.m_palette[m_rex3.m_zero_fract], BIT(mode0, 11));
		}
		else if (m_rex3.m_iter_y == end_y)
		{
			do_h_iline(m_rex3.m_iter_x, m_rex3.m_iter_y, end_x, m_cmap0.m_palette[m_rex3.m_zero_fract], BIT(mode0, 11));
		}
		else
		{
			do_iline(m_rex3.m_iter_x, m_rex3.m_iter_y, end_x, end_y, m_cmap0.m_palette[m_rex3.m_zero_fract], BIT(mode0, 11));
		}
		break;
	}
	case 0x00002106: // EnLSPattern, StopOnX, Block, Draw
	case 0x00009106: // Length32, EnZPattern, StopOnX, Block, Draw
	case 0x00022106: // LSOpaque, EnLSPattern, StopOnX, Block, Draw
	case 0x00019106: // ZPOpaque, EnLSPattern, StopOnX, Block, Draw
	{
		const bool opaque = (mode0 == 0x00019106) || (mode0 == 0x00022106);
		const uint32_t pattern = BIT(mode0, 12) ? m_rex3.m_z_pattern : m_rex3.m_ls_pattern;
		const uint8_t foreground = m_rex3.m_zero_fract & 0xff;
		const uint8_t background = m_rex3.m_color_back & 0xff;
		LOGMASKED(LOG_COMMANDS, "%08x at %04x (%04x), %04x (%04x) color %08x\n", pattern, m_rex3.m_xy_start_i >> 16, m_rex3.m_iter_x,
			(uint16_t)m_rex3.m_xy_start_i, m_rex3.m_iter_y, foreground);
		end_x += dx;
		int16_t end = end_x;
		if (BIT(mode0, 15))
		{
			if ((end_x - m_rex3.m_iter_x) >= 32)
			{
				end = m_rex3.m_x_start_i + 31;
			}
		}
		for (; m_rex3.m_iter_x != end; m_rex3.m_iter_x += dx)
		{
			if (pattern & (1 << (31 - (m_rex3.m_iter_x - m_rex3.m_x_start_i))))
			{
				write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, foreground);
			}
			else if (opaque)
			{
				write_pixel(m_rex3.m_iter_x, m_rex3.m_iter_y, background);
			}
		}
		if (BIT(m_rex3.m_bres_octant_inc1, 24))
			m_rex3.m_iter_y--;
		else
			m_rex3.m_iter_y++;
		m_rex3.m_iter_x = m_rex3.m_x_start_i;
		m_rex3.m_xy_start_i = (m_rex3.m_iter_x << 16) | m_rex3.m_iter_y;
		m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
		break;
	}
	default:
		LOGMASKED(LOG_COMMANDS | LOG_UNKNOWN, "Draw command %08x not recognized\n", m_rex3.m_draw_mode0);
		break;
	}
}

WRITE64_MEMBER(newport_video_device::rex3_w)
{
	switch (offset & ~(0x800/8))
	{
	case 0x0000/8:
		if (ACCESSING_BITS_32_63)
		{
			const uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 Draw Mode 1 Write: %08x\n", data32);
			switch (data32 & 7)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Planes Enabled:     None\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Planes Enabled:     R/W RGB/CI\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Planes Enabled:     R/W RGBA\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Planes Enabled:     R/W OLAY\n");
				break;
			case 0x04:
				LOGMASKED(LOG_REX3, "    Planes Enabled:     R/W PUP\n");
				break;
			case 0x05:
				LOGMASKED(LOG_REX3, "    Planes Enabled:     R/W CID\n");
				break;
			default:
				LOGMASKED(LOG_REX3 | LOG_UNKNOWN, "    Unknown Plane Enable Value\n");
				break;
			}
			switch ((data32 & 0x00000018) >> 3)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Plane Draw Depth:    4 bits\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Plane Draw Depth:    8 bits\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Plane Draw Depth:   12 bits\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Plane Draw Depth:   32 bits\n");
				break;
			}
			LOGMASKED(LOG_REX3, "    DBuf Source Buffer: %d\n", BIT(data, 5));
			LOGMASKED(LOG_REX3, "    GL Y Coordinates:   %d\n", BIT(data, 6));
			LOGMASKED(LOG_REX3, "    Enable Pxl Packing: %d\n", BIT(data, 7));
			switch ((data32 & 0x00000300) >> 8)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    HOSTRW Depth:        4 bits\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    HOSTRW Depth:        8 bits\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    HOSTRW Depth:       12 bits\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    HOSTRW Depth:       32 bits\n");
				break;
			}
			LOGMASKED(LOG_REX3, "    DWord Transfers:    %d\n", BIT(data, 10));
			LOGMASKED(LOG_REX3, "    Swap Endianness:    %d\n", BIT(data, 11));
			LOGMASKED(LOG_REX3, "    Compare Src > Dest: %d\n", BIT(data, 12));
			LOGMASKED(LOG_REX3, "    Compare Src = Dest: %d\n", BIT(data, 13));
			LOGMASKED(LOG_REX3, "    Compare Src < Dest: %d\n", BIT(data, 14));
			LOGMASKED(LOG_REX3, "    RGB Mode Select:    %d\n", BIT(data, 15));
			LOGMASKED(LOG_REX3, "    Enable Dithering:   %d\n", BIT(data, 16));
			LOGMASKED(LOG_REX3, "    Enable Fast Clear:  %d\n", BIT(data, 17));
			LOGMASKED(LOG_REX3, "    Enable Blending:    %d\n", BIT(data, 18));
			switch ((data32 & 0x00380000) >> 19)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Src Blend Factor:   0\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Src Blend Factor:   1\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Src Blend Factor:   Normalized Dest (or COLORBACK)\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Src Blend Factor:   1 - Normalized Dest (or COLORBACK)\n");
				break;
			case 0x04:
				LOGMASKED(LOG_REX3, "    Src Blend Factor:   Normalized Src\n");
				break;
			case 0x05:
				LOGMASKED(LOG_REX3, "    Src Blend Factor:   1 - Normalized Src\n");
				break;
			default:
				LOGMASKED(LOG_REX3 | LOG_UNKNOWN, "    Unknown Src Blend Factor: %02x\n", (data32 & 0x00380000) >> 19);
				break;
			}
			switch ((data32 & 0x01c00000) >> 22)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Dest Blend Factor:  0\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Dest Blend Factor:  1\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Dest Blend Factor:  Normalized Dest (or COLORBACK)\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Dest Blend Factor:  1 - Normalized Dest (or COLORBACK)\n");
				break;
			case 0x04:
				LOGMASKED(LOG_REX3, "    Dest Blend Factor:  Normalized Src\n");
				break;
			case 0x05:
				LOGMASKED(LOG_REX3, "    Dest Blend Factor:  1 - Normalized Src\n");
				break;
			default:
				LOGMASKED(LOG_REX3 | LOG_UNKNOWN, "    Unknown Src Blend Factor: %02x\n", (data32 & 0x00380000) >> 19);
				break;
			}
			LOGMASKED(LOG_REX3, "  COLORBACK Dest Blend: %d\n", BIT(data, 25));
			LOGMASKED(LOG_REX3, "   Enable Pxl Prefetch: %d\n", BIT(data, 26));
			LOGMASKED(LOG_REX3, "    SFACTOR Src Alpha:  %d\n", BIT(data, 27));
			switch ((data32 & 0xf0000000) >> 28)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   0\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   Src & Dst\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   Src & ~Dst\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   Src\n");
				break;
			case 0x04:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   ~Src & Dst\n");
				break;
			case 0x05:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   Dst\n");
				break;
			case 0x06:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   Src ^ Dst\n");
				break;
			case 0x07:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   Src | Dst\n");
				break;
			case 0x08:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   ~(Src | Dst)\n");
				break;
			case 0x09:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   ~(Src ^ Dst)\n");
				break;
			case 0x0a:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   ~Dst\n");
				break;
			case 0x0b:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   Src | ~Dst\n");
				break;
			case 0x0c:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   ~Src\n");
				break;
			case 0x0d:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   ~Src | Dst\n");
				break;
			case 0x0e:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   ~(Src & Dst)\n");
				break;
			case 0x0f:
				LOGMASKED(LOG_REX3, "    Logical Op. Type:   1\n");
				break;
			}
			m_rex3.m_draw_mode1 = data32;
		}
		if (ACCESSING_BITS_0_31)
		{
			const uint32_t data32 = (uint32_t)data;
			LOGMASKED(LOG_REX3, "REX3 Draw Mode 0 Write: %08x\n", data32);
			switch (data32 & 3)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Primitive Function: No Op\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Primitive Function: Read From FB\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Primitive Function: Draw To FB\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Primitive Function: Copy FB To FB\n");
				break;
			}
			switch ((data32 & 0x0000001c) >> 2)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Addressing Mode: Span/Point\n");
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Addressing Mode: Block\n");
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Addressing Mode: Bresenham Line, Integer Endpoints\n");
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Addressing Mode: Bresenham Line, Fractional Endpoints\n");
				break;
			case 0x04:
				LOGMASKED(LOG_REX3, "    Addressing Mode: AA Bresenham Line\n");
				break;
			default:
				LOGMASKED(LOG_REX3 | LOG_UNKNOWN, "    Unknown Addressing Mode: %02x\n", (data32 & 0x0000001c) >> 2);
				break;
			}
			LOGMASKED(LOG_REX3, "    Iterator Setup:     %d\n", BIT(data32, 5));
			LOGMASKED(LOG_REX3, "    RGB/CI Draw Source: %d\n", BIT(data32, 6));
			LOGMASKED(LOG_REX3, "     Alpha Draw Source: %d\n", BIT(data32, 7));
			LOGMASKED(LOG_REX3, "    Stop On X:          %d\n", BIT(data32, 8));
			LOGMASKED(LOG_REX3, "    Stop On Y:          %d\n", BIT(data32, 9));
			LOGMASKED(LOG_REX3, "    Skip Start Point:   %d\n", BIT(data32, 10));
			LOGMASKED(LOG_REX3, "    Skip End Point:     %d\n", BIT(data32, 11));
			LOGMASKED(LOG_REX3, "    Enable Patterning:  %d\n", BIT(data32, 12));
			LOGMASKED(LOG_REX3, "    Enable Stippling:   %d\n", BIT(data32, 13));
			LOGMASKED(LOG_REX3, "    Stipple Advance:    %d\n", BIT(data32, 14));
			LOGMASKED(LOG_REX3, "    Limit Draw To 32px: %d\n", BIT(data32, 15));
			LOGMASKED(LOG_REX3, "     Z Opaque Stipple   %d\n", BIT(data32, 16));
			LOGMASKED(LOG_REX3, "    LS Opaque Stipple:  %d\n", BIT(data32, 17));
			LOGMASKED(LOG_REX3, "    Enable Lin. Shade:  %d\n", BIT(data32, 18));
			LOGMASKED(LOG_REX3, "    Left-Right Only:    %d\n", BIT(data32, 19));
			LOGMASKED(LOG_REX3, "    Offset by XYMove:   %d\n", BIT(data32, 20));
			LOGMASKED(LOG_REX3, "    Enable CI Clamping: %d\n", BIT(data32, 21));
			LOGMASKED(LOG_REX3, "    Enable End Filter:  %d\n", BIT(data32, 22));
			LOGMASKED(LOG_REX3, "    Enable Y+2 Stride:  %d\n", BIT(data32, 23));
			m_rex3.m_draw_mode0 = data32;
		}
		break;
	case 0x0008/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Line Stipple Mode Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_ls_mode = (uint32_t)(data >> 32) & 0xfffffff;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Line Stipple Pattern Write: %08x\n", (uint32_t)data);
			m_rex3.m_ls_pattern = (uint32_t)data;
		}
		break;
	case 0x0010/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Line Stipple Pattern (Save) Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_ls_pattern_saved = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Pattern Register Write: %08x\n", (uint32_t)data);
			m_rex3.m_z_pattern = (uint32_t)data;
		}
		break;
	case 0x0018/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Opaque Pattern / Blendfunc Dest Color Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_color_back = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 VRAM Fastclear Color Write: %08x\n", (uint32_t)data);
			m_rex3.m_color_vram = (uint32_t)data;
		}
		break;
	case 0x0020/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 AFUNCTION Reference Alpha Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_alpha_ref = (uint8_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Stall GFIFO Write: %08x\n", data);
			break;
		}
		break;
	case 0x0028/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 0 X Min/Max Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_smask_x[0] = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 0 Y Min/Max Write: %08x\n", (uint32_t)data);
			m_rex3.m_smask_y[0] = (uint32_t)data;
		}
		break;
	case 0x0030/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Line/Span Setup Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_setup = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 ZPattern Enable Write: %08x\n", (uint32_t)data);
			m_rex3.m_step_z = (uint32_t)data;
		}
		break;
	case 0x0038/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Update LSPATTERN/LSRCOUNT\n");
			m_rex3.m_ls_pattern = m_rex3.m_ls_pattern_saved;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Update LSPATSAVE/LSRCNTSAVE\n");
			m_rex3.m_ls_pattern_saved = m_rex3.m_ls_pattern;
		}
		break;
	case 0x0100/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XStart Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_x_start = (uint32_t)(data >> 32) & 0x007ffff80;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 YStart Write: %08x\n", (uint32_t)data);
			m_rex3.m_y_start = (uint32_t)data & 0x007ffff80;
		}
		break;
	case 0x0108/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XEnd Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_x_end = (uint32_t)(data >> 32) & 0x007ffff80;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 YEnd Write: %08x\n", (uint32_t)data);
			m_rex3.m_y_end = (uint32_t)data & 0x007ffff80;
		}
		break;
	case 0x0110/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XSave Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_x_save = (uint16_t)(data >> 32);
			m_rex3.m_x_start_i = m_rex3.m_x_save;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 XYMove Write: %08x\n", (uint32_t)data);
			m_rex3.m_xy_move = (uint32_t)data;
		}
		break;
	case 0x0118/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham D Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_bres_d = (uint32_t)(data >> 32) & 0x7ffffff;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham S1 Write: %08x\n", (uint32_t)data);
			m_rex3.m_bres_s1 = (uint32_t)data & 0x1ffff;
		}
		break;
	case 0x0120/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham Octant & Incr1 Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_bres_octant_inc1 = (uint32_t)(data >> 32) & 0x70fffff;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham Octant Rounding Mode & Incr2 Write: %08x\n", (uint32_t)data);
			m_rex3.m_bres_round_inc2 = data & 0xff1fffff;
		}
		break;
	case 0x0128/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham E1 Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_bres_e1 = (uint16_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Bresenham S2 Write: %08x\n", (uint32_t)data);
			m_rex3.m_bres_s2 = (uint32_t)data & 0x3ffffff;
		}
		break;
	case 0x0130/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 AA Line Weight Table 1/2 Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_a_weight0 = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 AA Line Weight Table 2/2 Write: %08x\n", (uint32_t)data);
			m_rex3.m_a_weight1 = (uint32_t)data;
		}
		break;
	case 0x0138/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 GL XStart Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_x_start_f = (uint32_t)(data >> 32) & 0x7fff80;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 GL YStart Write: %08x\n", (uint32_t)data);
			m_rex3.m_y_start_f = (uint32_t)data & 0x7fff80;
		}
		break;
	case 0x0140/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 GL XEnd Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_x_end_f = (uint32_t)(data >> 32) & 0x7fff80;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 GL YEnd Write: %08x\n", (uint32_t)data);
			m_rex3.m_y_end_f = (uint32_t)data & 0x7fff80;
		}
		break;
	case 0x0148/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 XStart (integer) Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_x_start_i = (uint16_t)(data >> 32);
			m_rex3.m_x_save = m_rex3.m_x_start_i;
			m_rex3.m_x_start = ((m_rex3.m_x_start_i & 0x0000ffff) << 11);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 GL XEnd (copy) Write: %08x\n", (uint32_t)data);
			m_rex3.m_x_end_f = (uint32_t)data & 0x007fff80;
		}
		break;
	case 0x0150/8:
		if (ACCESSING_BITS_32_63)
		{
			const uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 XYStart (integer) Write: %08x\n", data32);
			m_rex3.m_xy_start_i = data32;
			m_rex3.m_x_start_i = data32 >> 16;
			m_rex3.m_x_save = m_rex3.m_x_start_i;
			m_rex3.m_x_start = ((m_rex3.m_xy_start_i & 0xffff0000) >>  5);
			m_rex3.m_y_start = ((m_rex3.m_xy_start_i & 0x0000ffff) << 11);
			m_rex3.m_iter_x = data32 >> 16;
			m_rex3.m_iter_y = (uint16_t)data32;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 XYEnd (integer) Write: %08x\n", (uint32_t)data);
			m_rex3.m_xy_end_i = (uint32_t)data;
			m_rex3.m_x_end = ((m_rex3.m_xy_end_i & 0xffff0000) >>  5);
			m_rex3.m_y_end = ((m_rex3.m_xy_end_i & 0x0000ffff) << 11);
		}
		break;
	case 0x0158/8:
		if (ACCESSING_BITS_32_63)
		{
			const uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 XStartEnd (integer) Write: %08x\n", data32);
			m_rex3.m_x_start_end_i = data32;
			m_rex3.m_xy_end_i   = (uint16_t)m_rex3.m_xy_end_i   | ((m_rex3.m_x_start_end_i & 0x0000ffff) << 16);
			m_rex3.m_xy_start_i = (uint16_t)m_rex3.m_xy_start_i | ( m_rex3.m_x_start_end_i & 0xffff0000);
			m_rex3.m_x_save = m_rex3.m_x_start_i;
			m_rex3.m_x_start = ((m_rex3.m_x_start_end_i & 0xffff0000) >>  5);
			m_rex3.m_x_end   = ((m_rex3.m_x_start_end_i & 0x0000ffff) << 11);
			m_rex3.m_iter_x = data32 >> 16;
		}
		break;
	case 0x0200/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Red/CI Full State Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_color_red = (uint32_t)((data >> 32) & 0xffffff);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Alpha Full State Write: %08x\n", (uint32_t)data);
			m_rex3.m_color_alpha = data & 0xfffff;
		}
		break;
	case 0x0208/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Green Full State Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_color_green = (uint32_t)((data >> 32) & 0xfffff);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Blue Full State Write: %08x\n", (uint32_t)data);
			m_rex3.m_color_blue = data & 0xfffff;
		}
		break;
	case 0x0210/8:
	{
		if (ACCESSING_BITS_32_63)
		{
			uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 Red/CI Slope Write: %08x\n", data32);
			data32 &= 0x807fffff;
			uint32_t temp = 0;
			if (BIT(data32, 31))
			{
				temp  = 0x00800000 - (data32 & 0x7fffff);
				temp |= 0x00800000;
			}
			else
			{
				temp = data32 & 0x7fffff;
			}
			m_rex3.m_slope_red = temp;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Alpha Slope Write: %08x\n", (uint32_t)data);
			data &= 0x8007ffff;
			uint32_t temp = 0;
			if (BIT(data, 31))
			{
				temp  = 0x00080000 - (data & 0x7ffff);
				temp |= 0x00080000;
			}
			else
			{
				temp = data & 0x7ffff;
			}
			m_rex3.m_slope_alpha = temp;
		}
		break;
	}
	case 0x0218/8:
	{
		if (ACCESSING_BITS_32_63)
		{
			uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 Green Slope Write: %08x\n", data32);
			data32 &= 0x8007ffff;
			uint32_t temp = 0;
			if (BIT(data32, 31))
			{
				temp  = 0x00080000 - (data32 & 0x7ffff);
				temp |= 0x00080000;
			}
			else
			{
				temp = data32 & 0x7ffff;
			}
			m_rex3.m_slope_green = temp;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Blue Slope Write: %08x\n", (uint32_t)data);
			data &= 0x8007ffff;
			uint32_t temp = 0;
			if (BIT(data, 31))
			{
				temp  = 0x00080000 - (data & 0x7ffff);
				temp |= 0x00080000;
			}
			else
			{
				temp = data & 0x7ffff;
			}
			m_rex3.m_slope_blue = temp;
		}
		break;
	}
	case 0x0220/8:
		if (ACCESSING_BITS_32_63)
		{
			const uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 Write Mask Write: %08x\n", data32);
			m_rex3.m_write_mask = data32 & 0xffffff;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Packed Color Fractions Write: %08x\n", (uint32_t)data);
			m_rex3.m_zero_fract = (uint32_t)data;
		}
		break;
	case 0x0228/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Color Index Zeros Overflow Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_zero_overflow = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Red/CI Slope (copy) Write: %08x\n", (uint32_t)data);
			m_rex3.m_slope_red = (uint32_t)data;
		}
		break;
	case 0x0230/8:
		LOGMASKED(LOG_REX3, "REX3 Host Data Port Write: %08x%08x & %08x%08x\n", (uint32_t)(data >> 32), (uint32_t)data, (uint64_t)(mem_mask >> 32), (uint32_t)mem_mask);
		COMBINE_DATA(&m_rex3.m_host_dataport);
		break;
	case 0x0238/8:
		if (ACCESSING_BITS_32_63)
		{
			data >>= 32;
			LOGMASKED(LOG_REX3, "REX3 Display Control Bus Mode Write: %08x\n", (uint32_t)data);
			switch (data & 3)
			{
			case 0x00:
				LOGMASKED(LOG_REX3, "    Transfer Width:     4 bytes\n");
				m_rex3.m_xfer_width = 4;
				break;
			case 0x01:
				LOGMASKED(LOG_REX3, "    Transfer Width:     1 bytes\n");
				m_rex3.m_xfer_width = 1;
				break;
			case 0x02:
				LOGMASKED(LOG_REX3, "    Transfer Width:     2 bytes\n");
				m_rex3.m_xfer_width = 2;
				break;
			case 0x03:
				LOGMASKED(LOG_REX3, "    Transfer Width:     3 bytes\n");
				m_rex3.m_xfer_width = 3;
				break;
			}
			LOGMASKED(LOG_REX3, "    DCB Reg Select Adr: %d\n", (data & 0x00000070 ) >> 4);
			LOGMASKED(LOG_REX3, "     DCB Slave Address: %d\n", (data & 0x00000780 ) >> 7);
			LOGMASKED(LOG_REX3, "    Use Sync XFer ACK:  %d\n", (data & 0x00000800 ) >> 11);
			LOGMASKED(LOG_REX3, "    Use Async XFer ACK: %d\n", (data & 0x00001000 ) >> 12);
			LOGMASKED(LOG_REX3, "   GIO CLK Cycle Width: %d\n", (data & 0x0003e000 ) >> 13);
			LOGMASKED(LOG_REX3, "    GIO CLK Cycle Hold: %d\n", (data & 0x007c0000 ) >> 18);
			LOGMASKED(LOG_REX3, "   GIO CLK Cycle Setup: %d\n", (data & 0x0f800000 ) >> 23);
			LOGMASKED(LOG_REX3, "    Swap Byte Ordering: %d\n", (data & 0x10000000 ) >> 28);
			m_rex3.m_dcb_reg_select = (data & 0x00000070) >> 4;
			m_rex3.m_dcb_slave_select = (data & 0x00000780) >> 7;
			m_rex3.m_dcb_mode = data & 0x1fffffff;
		}
		break;
	case 0x0240/8:
		if (ACCESSING_BITS_32_63)
		{
			const uint32_t data32 = (uint32_t)(data >> 32);
			m_rex3.m_dcb_data_msw = data32;
			switch (m_rex3.m_dcb_slave_select)
			{
			case 0x00:
				vc2_write(data32);
				break;
			case 0x01:
				cmap0_write(data32);
				break;
			case 0x04:
				xmap0_write(data32);
				xmap1_write(data32);
				break;
			case 0x05:
				xmap0_write(data32);
				break;
			case 0x06:
				xmap1_write(data32);
				break;
			default:
				LOGMASKED(LOG_REX3 | LOG_UNKNOWN, "REX3 Display Control Bus Data MSW Write: %08x\n", data32);
				break;
			}
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Display Control Bus Data LSW Write: %08x\n", (uint32_t)data);
			m_rex3.m_dcb_data_lsw = (uint32_t)data;
		}
		break;
	case 0x1300/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 1 X Min/Max Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_smask_x[1] = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 1 Y Min/Max Write: %08x\n", (uint32_t)data);
			m_rex3.m_smask_y[1] = (uint32_t)data;
		}
		break;
	case 0x1308/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 2 X Min/Max Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_smask_x[2] = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 2 Y Min/Max Write: %08x\n", (uint32_t)data);
			m_rex3.m_smask_y[2] = (uint32_t)data;
		}
		break;
	case 0x1310/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 3 X Min/Max Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_smask_x[3] = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 3 Y Min/Max Write: %08x\n", (uint32_t)data);
			m_rex3.m_smask_y[3] = (uint32_t)data;
		}
		break;
	case 0x1318/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 4 X Min/Max Write: %08x\n", (uint32_t)(data >> 32));
			m_rex3.m_smask_x[4] = (uint32_t)(data >> 32);
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "REX3 Screenmask 4 Y Min/Max Write: %08x\n", (uint32_t)data);
			m_rex3.m_smask_y[4] = (uint32_t)data;
		}
		break;
	case 0x1320/8:
		if (ACCESSING_BITS_32_63)
		{
			const uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 Top of Screen Scanline Write: %08x\n", data32);
			m_rex3.m_top_scanline = data32 & 0x3ff;
		}
		if (ACCESSING_BITS_0_31)
		{
			const uint32_t data32 = (uint32_t)data;
			LOGMASKED(LOG_REX3, "REX3 XY Window Write: %08x\n", data32);
			m_rex3.m_xy_window = data32;
		}
		break;
	case 0x1328/8:
		if (ACCESSING_BITS_32_63)
		{
			const uint32_t data32 = (uint32_t)(data >> 32);
			LOGMASKED(LOG_REX3, "REX3 Clipping Mode Write: %08x\n", data32);
			m_rex3.m_clip_mode = data32 & 0x1fff;
		}
		if (ACCESSING_BITS_0_31)
		{
			LOGMASKED(LOG_REX3, "Request GFIFO Stall\n");
		}
		break;
	case 0x1330/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "REX3 Config Write: %08x\n", data);
			m_rex3.m_config = data & 0x1fffff;
		}
		break;
	case 0x1340/8:
		if (ACCESSING_BITS_32_63)
		{
			LOGMASKED(LOG_REX3, "Reset DCB Bus and Flush BFIFO\n");
		}
		break;
	default:
		LOGMASKED(LOG_REX3 | LOG_UNKNOWN, "Unknown REX3 Write: %08x (%08x): %08x\n", 0xbf0f0000 + (offset << 2), mem_mask, data);
		break;
	}

	if (offset & 0x00000100)
	{
		do_rex3_command();
	}
}
