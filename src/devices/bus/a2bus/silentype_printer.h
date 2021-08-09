// license:BSD-3-Clause
// copyright-holders:
/*
 *  silentype printer
 *
 */
#include "screen.h"
#include "bitmap_printer.h"
#ifndef MAME_MACHINE_SILENTYPE_PRINTER_H
#define MAME_MACHINE_SILENTYPE_PRINTER_H

#pragma once

class silentype_printer_device : public device_t
{
public:
	silentype_printer_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
protected:
	silentype_printer_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

public:

	void update_printhead(uint8_t data);
	void update_pf_stepper(uint8_t data);
	void update_cr_stepper(uint8_t data);

	DECLARE_READ_LINE_MEMBER( margin_switch_input ) { return (m_xpos <= 0); }

protected:
	// device-level overrides

	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_reset_after_children() override;
	virtual ioport_constructor device_input_ports() const override;
	virtual void device_add_mconfig(machine_config &config) override;

private:

	uint8_t m_serial_data_out;
	uint8_t m_serial_clock_out;
	uint8_t m_store_clock_out;

	uint8_t *m_rom;
	uint8_t m_ram[256];

	bitmap_rgb32 m_bitmap;

	int m_xpos = 250;
	int m_ypos = 0;
	uint16_t m_shift_reg = 0;
	uint16_t m_parallel_reg = 0;
	int m_romenable = 0;  // start off disabled

	required_device<screen_device> m_screen;
	required_device<bitmap_printer_device> m_bitmap_printer;
//	required_device<bitmap_printer_device> m_bitmap_printer;

	int right_offset = 0;
	int left_offset = 3;

	double headtemp[7] = {0.0}; // initialize to zero - nan bugs
	int heattime = 3000;   // time in usec to hit max temp  (smaller numbers mean faster)
	int decaytime = 1000;  // time in usec to cool off

	int hstepperlast = 0;
	int vstepperlast = 0;
	int lastheadbits = 0;
	int xdirection;
	int newpageflag;

	int page_count=0;

	double last_update_time = 0.0;  // strange behavior if we don't initialize

 private:
	uint32_t screen_update_silentype(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	const int dpi=60;
	const int PAPER_WIDTH = 8.5 * dpi;  // 8.5 inches wide at 60 dpi
	const int PAPER_HEIGHT = 11 * dpi;   // 11  inches high at 60 dpi
	const int PAPER_SCREEN_HEIGHT = 384; // match the height of the apple II driver
	const int distfrombottom = 50;  // print position from bottom of screen

	uint32_t BITS(uint32_t x, u8 m, u8 n) {return ( ((x) >> (n)) & ( ((uint32_t) 1 << ((m) - (n) + 1)) - 1));}

	int wrap(int x, int mod) {if (x<0) return (x + ((-1 * (x / mod)) + 1) * mod) % mod; else return x % mod;}
	void write_snapshot_to_file(std::string directory, std::string name);

	void adjust_headtemp(u8 pin_status, double time_elapsed,  double& temp);
	void darken_pixel(double headtemp, unsigned int& pixel);
	void bitmap_clear_band(bitmap_rgb32 &bitmap, int from_line, int to_line, u32 color);

public:
    device_t* getrootdev();
    std::string fixchar(std::string in, char from, char to);
    std::string fixcolons(std::string in);
    std::string sessiontime();
    std::string tagname();
    std::string simplename();
    void setprintername(std::string name){ m_lp_luaprintername = name; }
    std::string getprintername(){ return m_lp_luaprintername; }
    void initprintername(){ setprintername(sessiontime()+std::string(" ")+tagname()); }

//    void initluaprinter(bitmap_rgb32 &mybitmap);
//  void setsnapshotdir(std::string dir){ m_lp_snapshotdir = dir; };
//   std::string getsnapshotdir(std::string dir){ return m_lp_snapshotdir; };

    std::string m_lp_luaprintername;
    std::string m_lp_snapshotdir;
    time_t m_lp_session_time;

};

DECLARE_DEVICE_TYPE(SILENTYPE_PRINTER, silentype_printer_device)

#endif // MAME_MACHINE_SILENTYPE_PRINTER_H
