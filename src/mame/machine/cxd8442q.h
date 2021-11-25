// license:BSD-3-Clause
// copyright-holders:Brice Onken

/*
 * Sony CXD8442Q WSC-FIFOQ APbus FIFO and DMA controller
 *
 * The FIFOQ is an APbus DMA controller designed for interfacing some of the simpler and lower speed peripherals
 * to the APbus while providing DMA capabilities. Each FIFO chip can support up to 4 devices.
 */

#ifndef MAME_MACHINE_CXD8442Q_H
#define MAME_MACHINE_CXD8442Q_H

#pragma once

class cxd8442q_device : public device_t
{
public:
	cxd8442q_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void map(address_map &map);
	void map_fifo_ram(address_map &map);

	auto out_int_callback() { return out_irq.bind(); }

	// FIFO channels
	enum fifo_channel_number
	{
		CH0 = 0,
		CH1 = 1,
		CH2 = 2,
		CH3 = 3
	};

	// DMA emulation
	template <fifo_channel_number ChannelNumber>
	void drq_w(int state) { fifo_channels[ChannelNumber].drq_w(state); };
	template <fifo_channel_number ChannelNumber>
	auto dma_r_cb() { return fifo_channels[ChannelNumber].dma_r_cb(); };
	template <fifo_channel_number ChannelNumber>
	auto dma_w_cb() { return fifo_channels[ChannelNumber].dma_w_cb(); };

protected:
	std::unique_ptr<uint32_t[]> fifo_ram;

	emu_timer *fifo_timer;
	TIMER_CALLBACK_MEMBER(fifo_dma_execute);

	// Interrupts
	devcb_write_line out_irq;
	void device_resolve_objects() override { out_irq.resolve_safe(); }
	void irq_check();

	// device_t overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

	// Class that represents a single FIFO channel. Each instance of the FIFOQ chip has 4 of these.
	class apfifo_channel
	{
	public:
		apfifo_channel(cxd8442q_device &fifo_device) : fifo_device(fifo_device), dma_r_callback(fifo_device), dma_w_callback(fifo_device)
		{
		}

		static constexpr int DMA_EN = 0x1;
		static constexpr int DMA_DIRECTION = 0x2; // 1 = out, 0 = in

		// total bytes avaliable for use by this channel
		uint32_t fifo_size = 0;

		// Start address in FIFO RAM (meaning that the channel uses [address, address + fifo_size])
		uint32_t address = 0;

		// Enables/disables DMA execution and sets the direction
		uint32_t dma_mode = 0;

		// Controls interrupt masking
		uint32_t intctrl = 0;

		// Provides interrupt status
		uint32_t intstat = 0;

		// Count of data to transfer or data recieved
		uint32_t count = 0;

		void reset()
		{
			fifo_size = 0;
			address = 0;
			dma_mode = 0;
			intctrl = 0;
			intstat = 0;
			count = 0;
			drq = false;
		}

		bool dma_cycle();

		auto dma_r_cb() { return dma_r_callback.bind(); }

		auto dma_w_cb() { return dma_w_callback.bind(); }

		void resolve_callbacks()
		{
			dma_r_callback.resolve_safe(0);
			dma_w_callback.resolve_safe();
		}

		// Emulates the FIFO data port
		uint32_t read_data_from_fifo();
		void write_data_to_fifo(uint32_t data);

		void drq_w(int state);

		bool drq_r()
		{
			return drq;
		}

	private:
		// reference to parent device
		cxd8442q_device &fifo_device;

		// DMA callback pointers
		devcb_read8 dma_r_callback;
		devcb_write8 dma_w_callback;

		// state tracking
		bool drq = false;

		// data pointers (within [address, address + fifo_size])
		uint32_t fifo_w_position = 0;
		uint32_t fifo_r_position = 0;
	};

	static constexpr int FIFO_CH_TOTAL = 4;
	apfifo_channel fifo_channels[FIFO_CH_TOTAL];
};

DECLARE_DEVICE_TYPE(CXD8442Q, cxd8442q_device)

#endif // MAME_MACHINE_CXD8442Q_H
