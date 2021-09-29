// license:BSD-3-Clause
// copyright-holders:Brice Onken
// thanks-to:Patrick Mackinlay

/*
 * Sony CXD8452AQ WSC-SONIC3 APbus interface and Ethernet controller controller
 *
 * The SONIC3 is an APbus controller designed for interfacing the SONIC Ethernet
 * controller to the APbus while providing DMA capabilities.
 *
 * TODO:
 *  - Determine if address bus translation actually goes through this chip
 *  - Interrupts
 *  - Other functionality
 */

#include "cxd8452aq.h"

#define VERBOSE 1
#include "logmacro.h"

DEFINE_DEVICE_TYPE(CXD8452AQ, cxd8452aq_device, "cxd8452aq", "Sony CXD8452AQ WSC-SONIC3")

cxd8452aq_device::cxd8452aq_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) : 
    device_t(mconfig, CXD8452AQ, tag, owner, clock), 
    device_memory_interface(mconfig, *this),
    main_bus_config("main_bus", ENDIANNESS_BIG, 32, 32, 0),
    sonic_config("sonic", ENDIANNESS_BIG, 32, 32, 0, address_map_constructor(FUNC(cxd8452aq_device::sonic_bus_map), this)),
    m_irq_handler(*this)
{

}

device_memory_interface::space_config_vector cxd8452aq_device::memory_space_config() const
{
    // Uses the same trick that the Jazz MCT-ADR driver uses to translate accesses from the SONIC to the system bus
	return space_config_vector{
		std::make_pair(0, &main_bus_config),
		std::make_pair(1, &sonic_config)
	};
}

void cxd8452aq_device::map(address_map &map)
{
    map(0x00, 0x03).lrw32(NAME([this]() { LOG("read sonic3.sonic  = 0x%x\n", sonic3_reg.sonic); return sonic3_reg.sonic; }), NAME([this](uint32_t data) { LOG("write sonic3.sonic = 0x%x\n", data); sonic3_reg.sonic = sonic3_reg.sonic | data; }));
	map(0x04, 0x07).lrw32(NAME([this]() { LOG("read sonic3.config = 0x%x\n", sonic3_reg.config); return sonic3_reg.config; }), NAME([this](uint32_t data) { LOG("write sonic3.config = 0x%x\n", data); sonic3_reg.config = data; }));
	map(0x08, 0x0b).lrw32(NAME([this]() { LOG("read sonic3.revision = 0x%x\n", sonic3_reg.revision); return sonic3_reg.revision; }), NAME([this](uint32_t data) { LOG("write sonic3.revision = 0x%x, but it is probably write only?\n", data); /* sonic3_reg.revision = data; */ }));
}

void cxd8452aq_device::sonic_bus_map(address_map &map)
{
	map(0x00000000U, 0xffffffffU).rw(FUNC(cxd8452aq_device::sonic_r), FUNC(cxd8452aq_device::sonic_w));
}

uint8_t cxd8452aq_device::sonic_r(offs_t offset, uint8_t mem_mask)
{
    //const auto adjustedOffset = (offset & 0xffff) << 1;
    auto result = space(0).read_byte(offset & 0xfffff);
    LOG("sonic_r[0x%x (0x%x) | 0x%x] = 0x%x\n", offset, offset & 0xfffff, mem_mask, result);
    return result;
}

void cxd8452aq_device::sonic_w(offs_t offset, uint8_t data, uint8_t mem_mask)
{
    //const auto adjustedOffset = (offset & 0xffff) << 1;
	LOG("sonic_w[0x%x (0x%x) | 0x%x]\n", offset, offset & 0xfffff, mem_mask);
    space(0).write_byte(offset & 0xfffff, data);
}

TIMER_CALLBACK_MEMBER(cxd8452aq_device::irq_check)
{
    bool newIrq = sonic3_reg.sonic & 0x1;
    if (m_irq != newIrq)
    {
		LOG("WSC-SONIC3 interrupt changed to %d!\n", newIrq);
		m_irq = newIrq;
		m_irq_handler(newIrq);
	}
}

void cxd8452aq_device::device_add_mconfig(machine_config &config) { }

void cxd8452aq_device::device_start()
{
    m_irq_handler.resolve_safe();
    m_irq_check = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(cxd8452aq_device::irq_check), this));
}

void cxd8452aq_device::device_reset() { }
