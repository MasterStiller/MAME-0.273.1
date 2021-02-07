// license:BSD-3-Clause
// copyright-holders:Brice Onken, based on Patrick Mackinlay's NEWS 68k and r3k emulators
// thanks-to:Patrick Mackinlay

/*
 * Sony NEWS R4000/4400-based workstations.
 *
 * Sources and more information:
 *   - http://ozuma.o.oo7.jp/nws5000x.htm
 *   - https://katsu.watanabe.name/doc/sonynews/
 *   - https://web.archive.org/web/20170202100940/www3.videa.or.jp/NEWS/
 *   - https://github.com/NetBSD/src/tree/trunk/sys/arch/newsmips
 *   - https://github.com/briceonk/news-os
 * 
 *  CPU configuration:
 *	- CPU card has a 75MHz crystal, multiplier (if any) TBD
 *	- PRId = 0x450
 * 	- config register = 0x1081E4BF
 *  	[31]    CM = 0 (MC mode off)
 *  	[30:28] EC = 001 (clock frequency divided by 3)
 *  	[27:24] EP = 00000 (doubleword every cycle)
 *  	[23:22] SB = 10 (16 word scache line size)
 *  	[21]    SS = 0 (unified scache)
 *  	[20]    SW = 0 (128-bit data path to scache)
 *  	[19:18] EW = 0 (64-bit system port width)
 *  	[17]    SC = 0 (scache present)
 *  	[16]    SM = 1 (Dirty Shared state disabled)
 *  	[15]    BE = 1 (Big endian)
 *  	[14]    EM = 1 (Parity mode)
 *  	[13]    EB = 1 (Sub-block ordering)
 *  	[12]    Reserved (0)
 *  	[11:9]  IC = 2 (16 KByte icache)
 *  	[8:6]   DC = 2 (16 KByte dcache)
 *  	Per page 90 of the R4400 user guide, the following bits ([5:0]) are mutable by software at runtime.
 *  	[5]     IB = 1 (32 byte icache line)
 *  	[4]     DB = 1 (32 byte icache line)
 *  	[3]     CU = 1 (SC uses cacheable coherent update on write)
 *  	[2:0]   K0 = 7 (cache coherency algo, 7 is reserved)
 *	- Known R4400 config differences between this driver and the physical platform:
 *      - emulated R4400 sets revision to 40 instead of 50. The user manual warns against using the revision field of PRId 
 *        in software, so hopefully that won't cause any deltas in behavior before that can be configured.
 *		- emulated SM (Dirty Shared state) is on by default - however, is SM actually being emulated?
 *		- emulated CU and K0 are all 0 instead of all 1 like on the physical platform. Unlike SM, software can set these.
 *      - In general, the secondary cache isn't emulated, which might influence bits 3:0 of the config register.
 *
 *  General Emulation Status (major chips only, there are additional smaller chips including CPLDs on the boards)
 *  CPU card:
 *   - MIPS R4400: emulated, with the caveats above
 *   - 10x Motorola MCM67A618FN12 SRAMs (secondary cache?): not emulated
 *  Motherboard:
 *   - Sony CXD8490G, CXD8491G, CXD8492G, CXD8489G (unknown ASICs): not emulated
 *   - Main memory: partially emulated (monitor ROM cannot enumerate the emulated RAM correctly)
 *  I/O board:
 *   - Sony CXD8409Q Parallel Interface: not emulated
 *   - National Semi PC8477B Floppy Controller: partially emulated (MAME only has the -A version currently)
 *   - Zilog Z8523010VSC ESCC serial interface: emulated (see following)
 *   - Sony CXD8421Q WSC-ESCC1 serial AP-Bus interface controller: skeleton (ESCC connections, probably DMA, AP-Bus interface, etc. handled by this chip)
 *   - 2x Sony CXD8442Q WSC-FIFO AP-Bus FIFO/interface chips: not emulated (handles AP-bus connections and probably DMA for sound, floppy, etc.)
 *   - National Semi DP83932B-VF SONIC Ethernet controller: not emulated (also, MAME only has the -C version currently)
 *   - Sony CXD8452AQ WSC-SONIC3 SONIC Ethernet AP-Bus interface controller: not emulated
 *   - Sony CXD8418Q WSC-PARK3: not emulated (most likely a gate array based on what the PARK2 was in older gen NEWS systems)
 *   - Sony CXD8403Q DMAC3Q DMA controller: skeleton
 *   - 2x HP 1TV3-0302 SPIFI3 SCSI controllers: skeleton
 *   - ST Micro M58T02-150PC1 Timekeeper RAM: emulated
 *  DSC-39 XB Framebuffer/video card:
 *   - Sony CXD8486Q XB: not emulated (most likely AP-Bus interface)
 *   - 16x NEC D482235G5 Dual Port Graphics Buffers: not emulated
 *   - Brooktree Bt468KG220 RAMDAC: not emulated
 */

#include "emu.h"

// Devices
// #define NO_MIPS3
#ifndef NO_MIPS3
#include "cpu/mips/mips3.h"
#else
#include "cpu/mips/r4000.h"
#endif

#include "machine/ram.h"
#include "machine/timekpr.h"
#include "machine/dmac3.h"
#include "machine/news_hid.h"
#include "machine/spifi3.h"
#include "machine/upd765.h"
#include "machine/cxd8421q.h"

// Buses
#include "machine/nscsi_bus.h"
#include "bus/nscsi/cd.h"
#include "bus/nscsi/hd.h"

// Floppy includes
#include "imagedev/floppy.h"
#include "formats/pc_dsk.h"

// MAME infra includes
#include "debugger.h"
#define VERBOSE 1
#include "logmacro.h"

// Uncomment to use "accurate" freerun_timer (too slow??)
// #define USE_ACCURATE_FREERUN

class news_r4k_state : public driver_device
{
public:
    news_r4k_state(machine_config const &mconfig, device_type type, char const *tag)
        : driver_device(mconfig, type, tag),
          m_cpu(*this, "cpu"),
          m_ram(*this, "ram"),
          m_rtc(*this, "rtc"),
          m_escc(*this, "escc1"),
          m_fdc(*this, "fdc"),
          m_hid(*this, "hid"),
          m_dmac(*this, "dmac"),
          m_scsi0(*this, "scsi0:7:spifi3"),
          m_scsi1(*this, "scsi1:7:spifi3"),
          m_scsibus0(*this, "scsi0"),
          m_scsibus1(*this, "scsi1"),
          m_led(*this, "led%u", 0U) {}

    // NWS-5000X
    void nws5000x(machine_config &config);
    void init_nws5000x();

protected:
    // driver_device overrides
    virtual void machine_start() override;
    virtual void machine_reset() override;

    // address maps
    void cpu_map(address_map &map);
    void cpu_map_debug(address_map &map);

    // machine config
    void machine_common(machine_config &config);

    // machine init
    void init_common();

    // Bitmap update (not implemented yet)
    u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect);

    // Interrupts
    // See news5000 section of https://github.com/NetBSD/src/blob/trunk/sys/arch/newsmips/include/adrsmap.h
    void inten_w(offs_t offset, uint32_t data);
    uint32_t inten_r(offs_t offset);
    uint32_t intst_r(offs_t offset);
    void intclr_w(offs_t offset, uint32_t data);
    void generic_irq_w(uint32_t irq, uint32_t mask, int state);

    enum irq0_number : uint32_t
    {
        DMAC = 0x01,
        SONIC = 0x02,
        FDC = 0x10
    };

    enum irq1_number : uint32_t
    {
        KBD = 0x01,
        ESCC = 0x02,
        AUDIO0 = 0x04,
        AUDIO1 = 0x08,
        PARALLEL = 0x20,
        FB = 0x80
    };

    enum irq2_number : uint32_t
    {
        TIMER0 = 0x01,
        TIMER1 = 0x02
    };

    enum irq4_number : uint32_t
    {
        APBUS = 0x01
    };

    template <irq0_number Number>
    void irq_w(int state) { generic_irq_w(0, Number, state); }
    template <irq1_number Number>
    void irq_w(int state) { generic_irq_w(1, Number, state); }
    template <irq2_number Number>
    void irq_w(int state) { generic_irq_w(2, Number, state); }
    template <irq4_number Number>
    void irq_w(int state) { generic_irq_w(4, Number, state); }
    void int_check();
#ifndef NO_MIPS3
    const int interrupt_map[6] = {MIPS3_IRQ0, MIPS3_IRQ1, MIPS3_IRQ2, MIPS3_IRQ3, MIPS3_IRQ4, MIPS3_IRQ5};
#else
    const int interrupt_map[6] = {0, 1, 2, 3, 4, 5};
#endif

    // Devices
    // MIPS R4400 CPU
#ifndef NO_MIPS3
    required_device<r4400be_device> m_cpu;
#else
    required_device<r4400_device> m_cpu;
#endif

    // Main memory
    required_device<ram_device> m_ram;

    // ST Micro M48T02 Timekeeper NVRAM + RTC
    required_device<m48t02_device> m_rtc;

    // Sony CXD8421Q ESCC1 serial controller (includes a Zilog ESCC)
    required_device<cxd8421q_device> m_escc;

    // SONIC ethernet controller - a different rev, the DP83932C, is emulated in MAME
    // so it hopefully will work with just a little modification.
    // required_device<dp83932c_device> m_net;
    // std::unique_ptr<u16[]> m_net_ram;

    // National Semiconductor PC8477B floppy controller
    DECLARE_FLOPPY_FORMATS(floppy_formats);
    required_device<pc8477a_device> m_fdc;

    // NEWS keyboard and mouse
    required_device<news_hid_hle_device> m_hid;

    // DMAC3 DMA controller
    required_device<dmac3_device> m_dmac;

    // HP SPIFI3 SCSI controller (2x)
    required_device<spifi3_device> m_scsi0;
    required_device<spifi3_device> m_scsi1;
    required_device<nscsi_bus_device> m_scsibus0;
    required_device<nscsi_bus_device> m_scsibus1;

    // LED control
    output_finder<6> m_led;
    void led_state_w(offs_t offset, uint32_t data);
    const std::string LED_MAP[6] = {"LED_POWER", "LED_DISK", "LED_FLOPPY", "LED_SEC", "LED_NET", "LED_CD"};

    // Interrupts and other platform state
    bool m_int_state[6] = {false, false, false, false, false, false};
    uint32_t m_inten[6] = {0, 0, 0, 0, 0, 0};
    uint32_t m_intst[6] = {0, 0, 0, 0, 0, 0};

    // Hardware timer
    // emu_timer *m_itimer;
    emu_timer *m_freerun_timer;
    void itimer_w(u8 data);
    void itimer(void *ptr, s32 param);

    // Freerun timer
    uint32_t freerun_timer_val;
    const int FREERUN_FREQUENCY = 1000000; // one tick per us, see comments in freerun function
    TIMER_CALLBACK_MEMBER(freerun_clock);
    uint32_t freerun_r(offs_t offset);
    void freerun_w(offs_t offset, uint32_t data);

    // APBus control (will be split into a device eventually)
    uint8_t apbus_cmd_r(offs_t offset);
    void apbus_cmd_w(offs_t offset, uint32_t data);

    // Other platform hardware emulation methods
    u32 bus_error();
    uint64_t front_panel_r(offs_t offset);

    // Constants
    const uint32_t ICACHE_SIZE = 16384;
    const uint32_t DCACHE_SIZE = 16384;
    const char *MAIN_MEMORY_DEFAULT = "64M";

    // RAM debug
    bool map_shift = false;
    uint8_t debug_ram_r(offs_t offset);
    void debug_ram_w(offs_t offset, uint8_t data);
};

FLOPPY_FORMATS_MEMBER(news_r4k_state::floppy_formats)
FLOPPY_PC_FORMAT
FLOPPY_FORMATS_END

static void news_scsi_devices(device_slot_interface &device)
{
    device.option_add("harddisk", NSCSI_HARDDISK);
    device.option_add("cdrom", NSCSI_CDROM);
}

void news_r4k_state::machine_common(machine_config &config)
{
    // CPU setup
#ifndef NO_MIPS3
    R4400BE(config, m_cpu, 75_MHz_XTAL);
    m_cpu->set_icache_size(ICACHE_SIZE);
    m_cpu->set_dcache_size(DCACHE_SIZE);
    m_cpu->set_secondary_cache_line_size(0x40);         // because config[23:22] = 0b10
    m_cpu->set_system_clock((75_MHz_XTAL).value() / 3); // because config[30:28] = 0b001
#else
    R4400(config, m_cpu, 75_MHz_XTAL);
#endif

    m_cpu->set_addrmap(AS_PROGRAM, &news_r4k_state::cpu_map);

    // Main memory
    RAM(config, m_ram);
    m_ram->set_default_size(MAIN_MEMORY_DEFAULT);

    // Timekeeper IC
    M48T02(config, m_rtc);

    // ESCC setup
    CXD8421Q(config, m_escc, 0);
    m_escc->out_int_callback().set(FUNC(news_r4k_state::irq_w<ESCC>));

    // Keyboard and mouse
    // Unlike 68k and R3000 NEWS machines, the keyboard and mouse seem to share an interrupt
    // See https://github.com/NetBSD/src/blob/trunk/sys/arch/newsmips/apbus/ms_ap.c#L103
    // where the mouse interrupt handler is initialized using the Keyboard interrupt.
    NEWS_HID_HLE(config, m_hid);
    m_hid->irq_out<news_hid_hle_device::KEYBOARD>().set(FUNC(news_r4k_state::irq_w<KBD>));
    m_hid->irq_out<news_hid_hle_device::MOUSE>().set(FUNC(news_r4k_state::irq_w<KBD>));

    // Floppy controller - National Semiconductor PC8477B
    // TODO: find out the difference between B and A - only A is emulated in MAME ATM
    // TODO: frequency? datasheet implies only 24MHz is valid. There is a 24MHz crystal on the I/O board, so this is probably right
    //       but it needs to be confirmed before locking it in with the XTAL macro
    PC8477A(config, m_fdc, 24'000'000, pc8477a_device::mode_t::PS2);
    /*
    TODO: how does AP-bus/FIFO chip/etc deal with interrupts?
    m_fdc->intrq_wr_callback().set(m_dmac, FUNC(dmac3_device::irq<1>));
	m_fdc->drq_wr_callback().set(m_dmac, FUNC(dmac3_device::drq<1>));
    */
    FLOPPY_CONNECTOR(config, "fdc:0", "35hd", FLOPPY_35_HD, true, floppy_formats).enable_sound(false);

    // DMA controller
    DMAC3(config, m_dmac, 0);
    m_dmac->set_bus(m_cpu, 0);
    m_dmac->out_int_cb().set(FUNC(news_r4k_state::irq_w<DMAC>));

    // Create SCSI buses
    NSCSI_BUS(config, m_scsibus0);
    NSCSI_BUS(config, m_scsibus1);

    // Create SCSI connectors
    NSCSI_CONNECTOR(config, "scsi0:0", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi0:1", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi0:2", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi0:3", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi0:4", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi0:5", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi0:6", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi1:0", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi1:1", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi1:2", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi1:3", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi1:4", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi1:5", news_scsi_devices, nullptr);
    NSCSI_CONNECTOR(config, "scsi1:6", news_scsi_devices, nullptr);

    // Connect SPIFI3s to the buses
    NSCSI_CONNECTOR(config, "scsi0:7").option_set("spifi3", SPIFI3).clock(16'000'000).machine_config([this](device_t *device) {
        // TODO: Actual clock and SCSI config (see news_r3k for what this might look like in the future)
    });

    NSCSI_CONNECTOR(config, "scsi1:7").option_set("spifi3", SPIFI3).clock(16'000'000).machine_config([this](device_t *device) {
        // TODO: Actual clock and SCSI config (see news_r3k for what this might look like in the future)
    });
}

void news_r4k_state::nws5000x(machine_config &config) { machine_common(config); }

/*
 * cpu_map
 * 
 * Assign the address map for the CPU
 * References:
 *  - https://github.com/NetBSD/src/blob/trunk/sys/arch/newsmips/include/adrsmap.h
 *  - MROM device table
 */
void news_r4k_state::cpu_map(address_map &map)
{
    map.unmap_value_high();

    // NEWS firmware
    map(0x1fc00000, 0x1fc3ffff).rom().region("mrom", 0);  // Monitor ROM
    map(0x1f3c0000, 0x1f3c03ff).rom().region("idrom", 0); // IDROM

    // Front panel DIP switches - TODO: mirror length
    map(0x1f3d0000, 0x1f3d0007).r(FUNC(news_r4k_state::front_panel_r));

    // Hardware timers
    // map(0x1f800000, 0x1f800000); // TIMER0
    map(0x1f840000, 0x1f840003).rw(FUNC(news_r4k_state::freerun_r), FUNC(news_r4k_state::freerun_w)); // FREERUN

    // Timekeeper NVRAM and RTC
    map(0x1f880000, 0x1f881fff).rw(m_rtc, FUNC(m48t02_device::read), FUNC(m48t02_device::write)).umask32(0x000000ff);

    // Interrupt ports
    map(0x1f4e0000, 0x1f4e0017).w(FUNC(news_r4k_state::intclr_w));                                // Clear
    map(0x1fa00000, 0x1fa00017).rw(FUNC(news_r4k_state::inten_r), FUNC(news_r4k_state::inten_w)); // Enable
    map(0x1fa00020, 0x1fa00037).r(FUNC(news_r4k_state::intst_r));                                 // Status

    // Port to shut off system (write a 0 to this)
    // map(0x1fc40000, 0x1fc40003)

    // LEDs
    map(0x1f3f0000, 0x1f3f0017).w(FUNC(news_r4k_state::led_state_w));

    // WSC-ESCC1 (CXD8421Q) serial controller
    map(0x1e940000, 0x1e94000f).rw(m_escc, FUNC(cxd8421q_device::ch_read<cxd8421q_device::CHB>), FUNC(cxd8421q_device::ch_write<cxd8421q_device::CHB>));
    map(0x1e950000, 0x1e95000f).rw(m_escc, FUNC(cxd8421q_device::ch_read<cxd8421q_device::CHA>), FUNC(cxd8421q_device::ch_write<cxd8421q_device::CHA>));
    // TODO: FIFO mapping

    // Sonic network controller
    // Potential references: https://git.qemu.org/?p=qemu.git;a=blob;f=hw/net/dp8393x.c;h=674b04b3547cdf312620a13c2f183e0ecfab24fb;hb=HEAD
    //                       https://github.com/NetBSD/src/blob/trunk/sys/arch/newsmips/apbus/if_sn_ap.c
    //                       https://github.com/NetBSD/src/blob/trunk/sys/arch/newsmips/apbus/if_snreg.h
    // map(0x1e600000, 0x1e600000);

    // DMAC3 DMA Controller
    map(0x14c20000, 0x14c3ffff).m(m_dmac, FUNC(dmac3_device::map_dma_ram));
    map(0x1e200000, 0x1e200017).m(m_dmac, FUNC(dmac3_device::map<dmac3_device::CTRL0>));
    map(0x1e300000, 0x1e300017).m(m_dmac, FUNC(dmac3_device::map<dmac3_device::CTRL1>));

    // SPIFI SCSI controllers
    // This mapping should probably go through the DMAC3 to match the platform setup.
    // The DMAC has to swap modes when talking to the SPIFI.
    map(0x1e280000, 0x1e2800ff).m(m_scsi0, FUNC(spifi3_device::map)); // TODO: actual end address, need command buffer space too
    map(0x1e380000, 0x1e3800ff).m(m_scsi1, FUNC(spifi3_device::map)); // TODO: actual end address, need command buffer space too

    // xb (Sony DSC-39 video card)
    // map(0x14900000, 0x14900000);

    // sb (AIF5 audio + FIFO transfer + MB87077 volume)
    // map(0x1ed00000, 0x1ed00000);

    // HID (kb + ms)
    map(0x1f900000, 0x1f900027).m(m_hid, FUNC(news_hid_hle_device::map_apbus));

    // lp (printer port??)
    // map(0x1ed30000, 0x1ed30000);

    // fd (floppy disk) - note that the FIFO address is here.
    // map(0x1ed20000, 0x1ed20000);
    // fd controller register mapping
    // to be fully hardware accurate, these shouldn't be umasked.
    // instead, they should be duplicated across each 32-bit segment to emulate the open address lines
    // (i.e. status register A and B values of 56 c0 look like 56565656 c0c0c0c0)
    // but, anything that uses these *should* just use the LSBs (famous last words)
    map(0x1ed60000, 0x1ed6001f).m(m_fdc, FUNC(pc8477a_device::map)).umask32(0x000000ff);
    // TODO: Floppy aux registers
    map(0x1ed60200, 0x1ed6020f).noprw();

    // Assign debug mappings
    cpu_map_debug(map);
}

/*
 * cpu_map_debug
 *
 * Method with temporary address map assignments. Everything in this function can be moved to the main memory
 * map function once it is understood. This separates the "real" mapping from the hacks required to get the
 * monitor ROM to boot.
 */
void news_r4k_state::cpu_map_debug(address_map &map)
{
    // After spending some quality time with the monitor ROM in the debugger, I did find a horrible hack that
    // gets the MROM to both enumerate 64MB of memory and pass memtest, by only enabling 0x2000000-0x3ffffff
    // after a certain point in the boot process. While this seems to kinda work???????, there are still issues
    // (like `ss -r` not showing the register values if it is dumping them to memory before printing them perhaps)
    // that might be related. I also still don't know if there is actually some magic going on with the memory map,
    // or if I am just not smart enough to figure out the "real" mapping that would make everything just work.
    // At least it is progress :)
    map(0x0, 0x7ffffff).rw(FUNC(news_r4k_state::debug_ram_r), FUNC(news_r4k_state::debug_ram_w));
    map(0x1440003c, 0x1440003f).lw32(NAME([this](offs_t offset, uint32_t data) {
        if (data == 0x10001)
        {
            LOG("Enabling map shift!\n");
            map_shift = true;
        }
        else
        {
            LOG("Disabling map shift!\n");
            map_shift = false;
        }
    }));
    // I have suspicions about addresses near these playing into the memory configuration
    //map(0x14400004, 0x14400007).lr32(NAME([this](offs_t offset) { return 0x3ff17; }));

    // APBus region
    map(0x1f520000, 0x1f520013).rw(FUNC(news_r4k_state::apbus_cmd_r), FUNC(news_r4k_state::apbus_cmd_w));
    // map(0x1f520004, 0x1f520007); // WBFLUSH
    // map(0x14c00004, 0x14c00007).ram(); // some kind of AP-bus register? Fully booted 5000X yields: 14c00004: 00007316
    // map(0x14c0000c, 0x14c0000c); // APBUS_INTMSK - interrupt mask
    // map(0x14c00014, 0x14c00014); // APBUS_INTST - interrupt status
    // map(0x14c0001c, 0x14c0001c); // APBUS_BER_A - Bus error address
    // map(0x14c00034, 0x14c00034); // APBUS_CTRL - configuration control
    // map(0x1400005c, 0x1400005c); // APBUS_DER_A - DMA error address
    // map(0x14c0006c, 0x14c0006c); // APBUS_DER_S - DMA error slot
    // map(0x14c00084, 0x14c00084); // APBUS_DMA - unmapped DMA coherency
    // map(0x14c20000, 0x14c40000); // APBUS_DMAMAP - DMA mapping RAM

    map(0x1e980000, 0x1e9fffff).ram();                                // is this mirrored?
    map(0x1fe00000, 0x1fffffff).ram();                                // determine mirror of this RAM - it is smaller than this size
    map(0x1f3e0000, 0x1f3efff0).lr8(NAME([this](offs_t offset) {
            if (offset % 4 == 2) { return 0x6f; }
            else if (offset % 4 == 3) { return 0xe0; }
            else { return 0x0;} })); // monitor ROM doesn't boot without this
}

uint8_t news_r4k_state::debug_ram_r(offs_t offset)
{
    uint8_t result = 0xff;
    if ((offset <= 0x1ffffff) || (map_shift && offset <= 0x3ffffff) || (!map_shift && offset >= 0x7f00000))
    {
        result = m_ram->read(offset);
    }
    else
    {
        LOG("Unmapped RAM read attempted at offset 0x%x\n", offset);
    }
    return result;
}

void news_r4k_state::debug_ram_w(offs_t offset, uint8_t data)
{
    if ((offset <= 0x1ffffff) || (map_shift && offset <= 0x3ffffff) || (!map_shift && offset >= 0x7f00000))
    {
        m_ram->write(offset, data);
    }
    else
    {
        LOG("Unmapped RAM write attempted at offset 0x%x (data: 0x%x)\n", offset, data);
    }
}

void news_r4k_state::machine_start()
{
    // Init front panel LEDs
    m_led.resolve();

    save_item(NAME(m_inten));
    save_item(NAME(m_intst));
    save_item(NAME(m_int_state));

    // Allocate freerunning clock (disabled for now)
    m_freerun_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(news_r4k_state::freerun_clock), this));
}

TIMER_CALLBACK_MEMBER(news_r4k_state::freerun_clock) { freerun_timer_val++; }

void news_r4k_state::machine_reset()
{
    freerun_timer_val = 0;
    m_freerun_timer->adjust(attotime::zero, 0, attotime::from_hz(FREERUN_FREQUENCY));
}

void news_r4k_state::init_common()
{
    // map the configured ram (temporarily not using this)
    //m_cpu->space(0).install_ram(0x00000000, m_ram->mask(), m_ram->pointer());
    //m_cpu->space(0).install_ram(0x03f00000, 0x3f00000 + m_ram->mask(), m_ram->pointer());
    //m_cpu->space(0).install_ram(0x07f00000, 0x7f00000 + m_ram->mask(), m_ram->pointer());
}

void news_r4k_state::init_nws5000x()
{
    init_common();
}

uint64_t news_r4k_state::front_panel_r(offs_t offset)
{
    ioport_value dipsw = this->ioport("FRONT_PANEL")->read();
    dipsw |= 0xff00; // Matches physical platform
    return ((uint64_t)dipsw << 32) | dipsw;
}

void news_r4k_state::led_state_w(offs_t offset, uint32_t data)
{
    if (m_led[offset] != data)
    {
        LOG(LED_MAP[offset] + ": " + (data ? "ON" : "OFF") + "\n");
        m_led[offset] = data;
    }
}

uint8_t news_r4k_state::apbus_cmd_r(offs_t offset)
{
    // ugly hack, like everything else about this driver right now
    // these values came from my NWS-5000X after it booted to the monitor
    // so this is pretending to be fully initialized. Needless to say, that
    // *might* cause problems with the monitor
    uint8_t value = 0x0;
    if (offset == 7)
    {
        value = 0x1;
    }
    else if (offset == 11)
    {
        value = 0x1;
    }
    else if (offset == 15)
    {
        value = 0xc8;
    }
    else if (offset == 19)
    {
        value = 0x32;
    }
    LOG("APBus read triggered at offset 0x%x, returing 0x%x\n", offset, value);
    return value;
}

void news_r4k_state::apbus_cmd_w(offs_t offset, uint32_t data)
{
    // map(0x1f520004, 0x1f520007); // WBFLUSH
    // map(0x14c00004, 0x14c00007).ram(); // some kind of AP-bus register? Fully booted 5000X yields: 14c00004: 00007316
    // map(0x14c0000c, 0x14c0000c); // APBUS_INTMSK - interrupt mask
    // map(0x14c00014, 0x14c00014); // APBUS_INTST - interrupt status
    // map(0x14c0001c, 0x14c0001c); // APBUS_BER_A - Bus error address
    // map(0x14c00034, 0x14c00034); // APBUS_CTRL - configuration control
    // map(0x1400005c, 0x1400005c); // APBUS_DER_A - DMA error address
    // map(0x14c0006c, 0x14c0006c); // APBUS_DER_S - DMA error slot
    // map(0x14c00084, 0x14c00084); // APBUS_DMA - unmapped DMA coherency
    // map(0x14c20000, 0x14c40000); // APBUS_DMAMAP - DMA mapping RAM
    LOG("AP-Bus command called, offset 0x%x, set to 0x%x\n", offset, data);
}

uint32_t news_r4k_state::freerun_r(offs_t offset)
{
// With an unscientific method, I calculated the timer value to increment roughly once per us
// NetBSD source code seems to corroborate this (https://github.com/NetBSD/src/blob/229cf3aa2cda57ba5f0c244a75ae83090e59c716/sys/arch/newsmips/newsmips/news5000.c#L259)
// The timer callback seemed to be too slow (although I could easily be doing something wrong)
#ifdef USE_ACCURATE_FREERUN
    return freerun_timer_val;
#else
    return freerun_timer_val << 10;
#endif
}

void news_r4k_state::freerun_w(offs_t offset, uint32_t data)
{
    LOG("freerun_w: Set freerun timer to 0x%x\n", data);
    freerun_timer_val = data;
}

u32 news_r4k_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect) { return 0; }

void news_r4k_state::inten_w(offs_t offset, uint32_t data)
{
    LOG("inten_w: INTEN%d = 0x%x\n", offset, data);
    m_inten[offset] = data;
    int_check();
}

uint32_t news_r4k_state::inten_r(offs_t offset)
{
    LOG("inten_r: INTEN%d = 0x%x\n", offset, m_inten[offset]);
    return m_inten[offset];
}

uint32_t news_r4k_state::intst_r(offs_t offset)
{
    LOG("intst_r: INTST%d = 0x%x\n", offset, m_intst[offset]);
    return m_intst[offset];
}

void news_r4k_state::generic_irq_w(uint32_t irq, uint32_t mask, int state)
{
    LOG("generic_irq_w: INTST%d IRQ %d set to %d\n", irq, mask, state);
    if (state)
    {
        m_intst[irq] |= mask;
    }
    else
    {
        m_intst[irq] &= ~mask;
    }
    int_check();
}

void news_r4k_state::intclr_w(offs_t offset, uint32_t data)
{
    LOG("intclr_w: INTCLR%d = 0x%x\n", offset, data);
    m_intst[offset] &= ~data; // TODO: is this correct?
    int_check();
}

void news_r4k_state::int_check()
{
    // The R4000 has 6 hardware interrupt pins
    // These map to the 6 INTST/EN/CLR groups on the NEWS platform
    // See https://github.com/NetBSD/src/blob/trunk/sys/arch/newsmips/newsmips/news5000.c
    // and https://github.com/NetBSD/src/blob/trunk/sys/arch/newsmips/apbus/apbus.c
    // This still needs to be tested - may or may not be fully accurate.

    for (int i = 0; i < 6; i++)
    {
        bool state = m_intst[i] & m_inten[i];
        if (state != m_int_state[i]) // Interrupt changed state
        {
            m_int_state[i] = state;
            m_cpu->set_input_line(interrupt_map[i], state);
        }
    }
}

u32 news_r4k_state::bus_error()
{
    LOG("bus_error: address access caused bus error\n");
#ifndef NO_MIPS3 // Is there a mips3.h device equivalent?
    LOG("bus_error: not implemented for this CPU type\n");
#else
    m_cpu->bus_error();
#endif
    return 0;
}

void news_r4k_state::itimer_w(u8 data)
{
    /*
    LOG("itimer_w 0x%02x (%s)\n", data, machine().describe_context());

    // TODO: assume 0xff stops the timer
    u8 const ticks = data + 1;

    m_itimer->adjust(attotime::from_ticks(ticks, 800), 0, attotime::from_ticks(ticks, 800));
    */
}

void news_r4k_state::itimer(void *ptr, s32 param)
{
    /*
    irq_w<TIMER>(ASSERT_LINE);
    */
}

/*
 * NWS-5000X DIP switches
 *
 * 1. Console - switch between serial and bitmap console. Bitmap is not implemented yet.
 * 2. Bitmap Disable - Enable or disable the internal video card
 * 3. Abort/Resume Enable - Unknown
 * 4. Clear NVRAM - Upon boot, clear NVRAM contents and restore default values if set
 * 5. Auto Boot - Upon boot, automatically attempt to boot from the disk specified by the bootdev NVRAM variable
 * 6. Run Diagnostic Test - Attempt to run diagnostic test after ROM monitor has booted
 * 7. External APSlot Probe Disable - If set, do not attempt to probe the expansion APBus slots
 * 8. No Memory Mode - If set, do not use the main memory (limits system to 128KiB)
 */
static INPUT_PORTS_START(nws5000)
PORT_START("FRONT_PANEL")
PORT_DIPNAME(0x01, 0x00, "Console") PORT_DIPLOCATION("FRONT_PANEL:1")
PORT_DIPSETTING(0x00, "Serial Terminal")
PORT_DIPSETTING(0x01, "Bitmap")
PORT_DIPNAME(0x02, 0x00, "Bitmap Disable") PORT_DIPLOCATION("FRONT_PANEL:2")
PORT_DIPSETTING(0x00, "Enable built-in bitmap")
PORT_DIPSETTING(0x02, "Disable built-in bitmap")
PORT_DIPNAME(0x04, 0x00, "Abort/Resume Enable") PORT_DIPLOCATION("FRONT_PANEL:3")
PORT_DIPSETTING(0x00, "Disable Abort/Resume")
PORT_DIPSETTING(0x04, "Enable Abort/Resume")
PORT_DIPNAME(0x08, 0x00, "Clear NVRAM") PORT_DIPLOCATION("FRONT_PANEL:4")
PORT_DIPSETTING(0x00, "Do not clear")
PORT_DIPSETTING(0x08, "Clear NVRAM")
PORT_DIPNAME(0x10, 0x00, "Auto Boot") PORT_DIPLOCATION("FRONT_PANEL:5")
PORT_DIPSETTING(0x00, "Auto Boot Disable")
PORT_DIPSETTING(0x10, "Auto Boot Enable")
PORT_DIPNAME(0x20, 0x00, "Run Diagnostic Test") PORT_DIPLOCATION("FRONT_PANEL:6")
PORT_DIPSETTING(0x00, "No Diagnostic Test")
PORT_DIPSETTING(0x20, "Run Diagnostic Test")
PORT_DIPNAME(0x40, 0x00, "External APSlot Probe Disable") PORT_DIPLOCATION("FRONT_PANEL:7")
PORT_DIPSETTING(0x00, "Enable External APSlot Probe")
PORT_DIPSETTING(0x40, "Disable External APSlot Probe")
PORT_DIPNAME(0x80, 0x00, "No Memory Mode") PORT_DIPLOCATION("FRONT_PANEL:8")
PORT_DIPSETTING(0x00, "Main Memory Enabled")
PORT_DIPSETTING(0x80, "Main Memory Disabled");
INPUT_PORTS_END

// ROM definitions
ROM_START(nws5000x)
ROM_REGION64_BE(0x40000, "mrom", 0)
ROM_SYSTEM_BIOS(0, "nws5000x", "APbus System Monitor Release 3.201")
ROMX_LOAD("mpu-33__ver3.201__1994_sony.rom", 0x00000, 0x40000, CRC(8a6ca2b7) SHA1(72d52e24a554c56938d69f7d279b2e65e284fd59), ROM_BIOS(0))

ROM_REGION64_BE(0x400, "idrom", 0)
ROM_LOAD("idrom.rom", 0x000, 0x400, CRC(89edfebe) SHA1(3f69ebfaf35610570693edf76aa94c10b30de627) BAD_DUMP)
ROM_END

// Machine definitions
//   YEAR  NAME      PARENT COMPAT MACHINE   INPUT    CLASS           INIT           COMPANY FULLNAME                      FLAGS
COMP(1994, nws5000x, 0, 0, nws5000x, nws5000, news_r4k_state, init_nws5000x, "Sony", "NET WORK STATION NWS-5000X", MACHINE_NOT_WORKING | MACHINE_NO_SOUND)
