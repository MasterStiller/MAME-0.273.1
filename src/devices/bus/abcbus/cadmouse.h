// license:BSD-3-Clause
// copyright-holders:Peter Bortas
#ifndef MAME_BUS_ABCBUS_CADMOUSE_H
#define MAME_BUS_ABCBUS_CADMOUSE_H

#pragma once

#include "abcbus.h"



//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class abc_cadmouse_device :  public device_t,
							public device_abcbus_card_interface
{
public:
	// construction/destruction
	abc_cadmouse_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// optional information overrides
	virtual const tiny_rom_entry *device_rom_region() const override;

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	// device_abcbus_interface overrides
	virtual void abcbus_cs(uint8_t data) override;
};


// device type definition
DECLARE_DEVICE_TYPE(ABC_CADMOUSE, abc_cadmouse_device)

#endif // MAME_BUS_ABCBUS_CADMOUSE_H
