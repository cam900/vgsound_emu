/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/blob/vgsound_emu_v1/LICENSE for more details

	Copyright holder(s): cam900
	Ensoniq ES5506 emulation core

	see es550x.cpp for more info
*/

#include "es5506.hpp"

// Internal functions
void es5506_core::tick()
{
	m_voice_update = false;
	m_voice_end = false;
	// CLKIN
	if (m_clkin.tick())
	{
		// BCLK
		if (m_clkin.m_edge.m_changed && (!m_mode.bclk_en)) // BCLK is freely running clock
		{
			if (m_bclk.tick())
			{
				m_intf.bclk(m_bclk.current_edge());
				// Serial output
				if (!m_mode.lrclk_en)
				{
					if (m_bclk.falling_edge())
					{
						// LRCLK
						if (m_lrclk.tick())
						{
							m_intf.lrclk(m_lrclk.current_edge());
							if (m_lrclk.rising_edge())
							{
								m_w_st_curr = m_w_st;
								m_w_end_curr = m_w_end;
							}
							if (m_lrclk.falling_edge()) // update width
								m_lrclk.set_width_latch(m_lr_end);
						}
					}
				}
				// WCLK
				if (!m_mode.wclk_en)
				{
					if (!m_mode.lrclk_en)
					{
						if (m_lrclk.m_edge.m_changed)
							m_wclk = 0;
					}
					if (m_bclk.falling_edge())
					{
						if (m_wclk == m_w_st_curr)
						{
							m_intf.wclk(true);
							if (m_lrclk.current_edge())
							{
								for (int i = 0; i < 6; i++)
								{
									// copy output
									m_output[i] = m_output_temp[i];
									m_output_latch[i] = m_ch[i];
									m_output_temp[i].reset();
									// clamp to 20 bit (upper 3 bits are overflow guard bits)
									m_output_latch[i].m_left = std::clamp(m_output_latch[i].m_left, -0x80000, 0x7ffff);
									m_output_latch[i].m_right = std::clamp(m_output_latch[i].m_right, -0x80000, 0x7ffff);
									// set signed
									if (m_output_latch[i].m_left < 0)
										m_output_temp[i].m_left = -1;
									if (m_output_latch[i].m_right < 0)
										m_output_temp[i].m_right = -1;
								}
							}
							m_wclk_lr = m_lrclk.current_edge();
							m_output_bit = 20;
						}
						if (m_wclk < m_w_end_curr)
						{
							s8 output_bit = --m_output_bit;
							if (m_output_bit >= 0)
							{
								for (int i = 0; i < 6; i++)
								{
									if (m_wclk_lr) // Right output
										m_output_temp[i].m_right = (m_output_temp[i].m_right << 1) | bitfield(m_output_latch[i].m_right, output_bit);
									else // Left output
										m_output_temp[i].m_left = (m_output_temp[i].m_left << 1) | bitfield(m_output_latch[i].m_left, output_bit);
								}
							}
						}
						if (m_wclk == m_w_end_curr)
							m_intf.wclk(false);

						m_wclk++;
					}
				}
			}
		}
		// /CAS, E
		if (m_clkin.falling_edge()) // falling edge triggers /CAS, E clock
		{
			// /CAS
			if (m_cas.tick())
			{
				// single OTTO master mode, /CAS high, E low: get sample address
				// single OTTO early mode, /CAS falling, E high: get sample address
				if (m_cas.falling_edge())
				{
					if (!m_e.current_edge())
					{
						// single OTTO master mode, /CAS low, E low: fetch sample
						if (m_mode.master)
							m_voice[m_voice_cycle].fetch(m_voice_cycle, m_voice_fetch);
					}
					else if (m_e.current_edge())
					{
						// dual OTTO slave mode, /CAS low, E high: fetch sample
						if (m_mode.dual && (!m_mode.master)) // Dual OTTO, slave mode
							m_voice[m_voice_cycle].fetch(m_voice_cycle, m_voice_fetch);
					}
				}
			}
			// E
			if (m_e.tick())
			{
				m_intf.e_pin(m_e.current_edge());
				if (m_e.rising_edge())
				{
					m_host_intf.m_rw = m_host_intf.m_rw_strobe;
					m_host_intf.m_host_access = m_host_intf.m_host_access_strobe;
				}
				else if (m_e.falling_edge())
				{
					m_host_intf.m_host_access = false;
					voice_tick();
				}
				if (m_e.current_edge()) // Host interface
				{
					if (m_host_intf.m_host_access)
					{
						if (m_host_intf.m_rw && (m_e.cycle() == 0)) // Read
						{
							m_hd = read(m_ha);
							m_host_intf.m_host_access = false;
						}
						else if ((!m_host_intf.m_rw) && (m_e.cycle() == 2)) // Write
							write(m_ha, m_hd);
					}
				}
				else if (!m_e.current_edge())
				{
					if (m_e.cycle() == 2)
					{
						// reset host access state
						m_hd = 0;
						m_host_intf.m_host_access_strobe = false;
					}
				}
			}
		}
	}
}

// less cycle accurate, but less CPU heavy routine
void es5506_core::tick_perf()
{
	m_voice_update = false;
	m_voice_end = false;
	// output
	if (((!m_mode.lrclk_en) && (!m_mode.bclk_en) && (!m_mode.wclk_en)) && (m_w_st < m_w_end))
	{
		const int output_bits = 20 - (m_w_end - m_w_st);
		if (output_bits < 20)
		{
			for (int c = 0; c < 6; c++)
			{
				m_output[c].m_left  = std::clamp<s32>(m_ch[c].m_left, -0x80000, 0x7ffff) >> output_bits;
				m_output[c].m_right = std::clamp<s32>(m_ch[c].m_right, -0x80000, 0x7ffff) >> output_bits;
			}
		}
	}
	else
	{
		for (int c = 0; c < 6; c++)
		{
			m_output[c].m_left  = 0;
			m_output[c].m_right = 0;
		}
	}

	// update
	// falling edge
	m_e.m_edge.set(false);
	m_intf.e_pin(false);
	m_host_intf.m_host_access = m_host_intf.m_host_access_strobe = false;
	m_voice[m_voice_cycle].fetch(m_voice_cycle, m_voice_fetch);
	voice_tick();
	// rising edge
	m_e.m_edge.set(true);
	m_intf.e_pin(true);
	m_host_intf.m_rw = m_host_intf.m_rw_strobe;
	m_host_intf.m_host_access = m_host_intf.m_host_access_strobe;
	// falling edge
	m_e.m_edge.set(false);
	m_intf.e_pin(false);
	m_host_intf.m_host_access = m_host_intf.m_host_access_strobe = false;
	m_voice[m_voice_cycle].fetch(m_voice_cycle, m_voice_fetch);
	voice_tick();
	// rising edge
	m_e.m_edge.set(true);
	m_intf.e_pin(true);
	m_host_intf.m_rw = m_host_intf.m_rw_strobe;
	m_host_intf.m_host_access = m_host_intf.m_host_access_strobe;
}

void es5506_core::voice_tick()
{
	// Voice updates every 2 E clock cycle (or 4 BCLK clock cycle)
	m_voice_update = bitfield(m_voice_fetch++, 0);
	if (m_voice_update)
	{
		// Update voice
		m_voice[m_voice_cycle].tick(m_voice_cycle);

		// Refresh output
		if ((++m_voice_cycle) > std::clamp<u8>(m_active, 4, 31)) // 5 ~ 32 voices
		{
			m_voice_end = true;
			m_voice_cycle = 0;
			for (auto & elem : m_ch)
				elem.reset();

			for (auto & elem : m_voice)
			{
				const u8 ca = bitfield(elem.m_cr.ca, 0, 3);
				if (ca < 6)
				{
					m_ch[ca].m_left += elem.m_ch.m_left;
					m_ch[ca].m_right += elem.m_ch.m_right;
				}
				elem.m_ch.reset();
			}
		}
		m_voice_fetch = 0;
	}
}

void es5506_core::voice_t::fetch(u8 voice, u8 cycle)
{
	m_alu.m_sample[cycle] = m_host.m_intf.read_sample(voice, bitfield(m_cr.bs, 0, 1), bitfield(m_alu.get_accum_integer() + cycle, 0, m_alu.m_integer));
	if (m_cr.cmpd) // Decompress (Upper 8 bit is used for compressed format)
		m_alu.m_sample[cycle] = decompress(bitfield(m_alu.m_sample[cycle], 8, 8));
}

void es5506_core::voice_t::tick(u8 voice)
{
	m_ch.reset();

	// Filter execute
	m_filter.tick(m_alu.interpolation());

	if (m_alu.busy())
	{
		if (!m_mute)
		{
			// Send to output
			m_ch.m_left = volume_calc(m_lvol, sign_ext<s32>(m_filter.m_o4_1, 16));
			m_ch.m_right = volume_calc(m_rvol, sign_ext<s32>(m_filter.m_o4_1, 16));
		}

		// ALU execute
		if (m_alu.tick())
			m_alu.loop_exec();
	}
	// Envelope
	if (m_ecount != 0)
	{
		// Left and Right volume
		if (bitfield(m_lvramp, 0, 8) != 0)
			m_lvol = std::clamp<s32>(m_lvol + sign_ext<s32>(bitfield(m_lvramp, 0, 8), 8), 0, 0xffff);
		if (bitfield(m_rvramp, 0, 8) != 0)
			m_rvol = std::clamp<s32>(m_rvol + sign_ext<s32>(bitfield(m_rvramp, 0, 8), 8), 0, 0xffff);

		// Filter coeffcient
		if ((m_k1ramp.ramp != 0) && ((m_k1ramp.slow == 0) || (bitfield(m_filtcount, 0, 3) == 0)))
			m_filter.m_k1 = std::clamp<s32>(m_filter.m_k1 + sign_ext<s32>(m_k1ramp.ramp, 8), 0, 0xffff);
		if ((m_k2ramp.ramp != 0) && ((m_k2ramp.slow == 0) || (bitfield(m_filtcount, 0, 3) == 0)))
			m_filter.m_k2 = std::clamp<s32>(m_filter.m_k2 + sign_ext<s32>(m_k2ramp.ramp, 8), 0, 0xffff);

		m_ecount--;
	}
	m_filtcount = bitfield(m_filtcount + 1, 0, 3);

	// Update IRQ
	m_alu.irq_exec(m_host.m_intf, m_host.m_irqv, voice);
}

// Compressed format
s16 es5506_core::voice_t::decompress(u8 sample)
{
	u8 exponent = bitfield(sample, 5, 3);
	u8 mantissa = bitfield(sample, 0, 5);
	return (exponent > 0) ?
	        s16(((bitfield(mantissa, 4) ? 0x10 : ~0x1f) | bitfield(mantissa, 0, 4)) << (4 + (exponent - 1))) :
	        s16(((bitfield(mantissa, 4) ? ~0xf : 0) | bitfield(mantissa, 0, 4)) << 4);
}

// volume calculation
s32 es5506_core::voice_t::volume_calc(u16 volume, s32 in)
{
	u8 exponent = bitfield(volume, 12, 4);
	u8 mantissa = bitfield(volume, 4, 8);
	return (in * s32(0x100 | mantissa)) >> (20 - exponent);
}

void es5506_core::reset()
{
	es550x_shared_core::reset();
	for (auto & elem : m_voice)
		elem.reset();

	m_read_latch = 0xffffffff;
	m_write_latch = 0xffffffff;
	m_w_st = 0;
	m_w_end = 0;
	m_lr_end = 0;
	m_w_st_curr = 0;
	m_w_end_curr = 0;
	m_mode.reset();
	m_bclk.reset();
	m_lrclk.reset(32);
	m_wclk = 0;
	m_wclk_lr = false;
	m_output_bit = 0;
	for (auto & elem : m_ch)
		elem.reset();
	for (auto & elem : m_output)
		elem.reset();
	for (auto & elem : m_output_temp)
		elem.reset();
	for (auto & elem : m_output_latch)
		elem.reset();
}

void es5506_core::voice_t::reset()
{
	es550x_shared_core::es550x_voice_t::reset();
	m_lvol = 0;
	m_lvramp = 0;
	m_rvol = 0;
	m_rvramp = 0;
	m_ecount = 0;
	m_k2ramp.reset();
	m_k1ramp.reset();
	m_filtcount = 0;
	m_ch.reset();
	m_mute = false;
}

// Accessors
u8 es5506_core::host_r(u8 address)
{
	if (!m_host_intf.m_host_access)
	{
		m_ha = address;
		if (m_e.rising_edge()) // update directly
			m_hd = read(m_ha, true);
		else
		{
			m_host_intf.m_rw_strobe = true;
			m_host_intf.m_host_access_strobe = true;
		}
	}
	return m_hd;
}

void es5506_core::host_w(u8 address, u8 data)
{
	if (!m_host_intf.m_host_access)
	{
		m_ha = address;
		m_hd = data;
		if (m_e.rising_edge()) // update directly
			write(m_ha, m_hd, true);
		else
		{
			m_host_intf.m_rw_strobe = false;
			m_host_intf.m_host_access_strobe = true;
		}
	}
}

u8 es5506_core::read(u8 address, bool cpu_access)
{
	const u8 byte = bitfield(address, 0, 2); // byte select
	const u8 shift = 24 - (byte << 3);
	if (byte != 0) // Return already latched register if not highest byte is accessing
		return bitfield(m_read_latch, shift, 8);

	address = bitfield(address, 2, 4); // 4 bit address for CPU access

	// get read register
	m_read_latch = regs_r(m_page, address, cpu_access);

	return bitfield(m_read_latch, 24, 8);
}

void es5506_core::write(u8 address, u8 data, bool cpu_access)
{
	const u8 byte = bitfield(address, 0, 2); // byte select
	const u8 shift = 24 - (byte << 3);
	address = bitfield(address, 2, 4); // 4 bit address for CPU access

	// Update register latch
	m_write_latch = (m_write_latch & ~(0xff << shift)) | (u32(data) << shift);

	if (byte != 3) // Wait until lowest byte is writed
		return;

	regs_w(m_page, address, m_write_latch, cpu_access);

	// Reset latch
	m_write_latch = 0;
}

u32 es5506_core::regs_r(u8 page, u8 address, bool cpu_access)
{
	u32 read_latch = 0xffffffff;
	if (address >= 13) // Global registers
	{
		switch (address)
		{
			case 13: // POT (Pot A/D Register)
				read_latch = (read_latch & ~0x3ff) | bitfield(m_intf.adc_r(), 0, 10);
				break;
			case 14: // IRQV (Interrupting voice vector)
				read_latch = (read_latch & ~0x9f) | (m_irqv.irqb ? 0x80 : 0) | bitfield(m_irqv.voice, 0, 5);
				if (cpu_access)
				{
					m_irqv.clear();
					if (bitfield(read_latch, 7) != m_irqv.irqb)
						m_voice[m_irqv.voice].m_alu.irq_update(m_intf, m_irqv);
				}
				break;
			case 15: // PAGE (Page select register)
				read_latch = (read_latch & ~0x7f) | bitfield(m_page, 0, 7);
				break;
		}
	}
	else
	{
		if (bitfield(page, 6)) // Channel registers are Write only
		{
			if (!cpu_access) // CPU can't read here
			{
				switch (address)
				{
					case 0: // CH0L (Channel 0 Left)
					case 2: // CH1L (Channel 1 Left)
					case 4: // CH2L (Channel 2 Left)
					case 6: // CH3L (Channel 3 Left)
					case 8: // CH4L (Channel 4 Left)
					case 10: // CH5L (Channel 5 Left)
						read_latch = m_ch[bitfield(address, 1, 3)].m_left;
						break;
					case 1: // CH0R (Channel 0 Right)
					case 3: // CH1R (Channel 1 Right)
					case 5: // CH2R (Channel 2 Right)
					case 7: // CH3R (Channel 3 Right)
					case 9: // CH4R (Channel 4 Right)
					case 11: // CH5R (Channel 5 Right)
						read_latch = m_ch[bitfield(address, 1, 3)].m_right;
						break;
				}
			}
		}
		else
		{
			const u8 voice = bitfield(page, 0, 5); // Voice select
			voice_t &v = m_voice[voice];
			if (bitfield(page, 5)) // Page 32 - 63
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						read_latch = (read_latch & ~0xffff) | 
						          (v.m_alu.m_cr.stop0 ? 0x0001 : 0x0000)
										| (v.m_alu.m_cr.stop1 ? 0x0002 : 0x0000)
										| (v.m_alu.m_cr.lei   ? 0x0004 : 0x0000)
										| (v.m_alu.m_cr.lpe   ? 0x0008 : 0x0000)
										| (v.m_alu.m_cr.ble   ? 0x0010 : 0x0000)
										| (v.m_alu.m_cr.irqe  ? 0x0020 : 0x0000)
										| (v.m_alu.m_cr.dir   ? 0x0040 : 0x0000)
										| (v.m_alu.m_cr.irq   ? 0x0080 : 0x0000)
										| (bitfield(v.m_filter.m_lp, 0, 2) << 8)
										| (bitfield(v.m_cr.ca, 0, 3) << 10)
										| (v.m_cr.cmpd        ? 0x2000 : 0x0000)
										| (bitfield(v.m_cr.bs, 0, 2) << 14);
						break;
					case 1: // START (Loop Start Register)
						read_latch = (read_latch & ~0xfffff800) | (v.m_alu.m_start & 0xfffff800);
						break;
					case 2: // END (Loop End Register)
						read_latch = (read_latch & ~0xffffff80) | (v.m_alu.m_end & 0xffffff80);
						break;
					case 3: // ACCUM (Accumulator Register)
						read_latch = v.m_alu.m_accum;
						break;
					case 4: // O4(n-1) (Filter 4 Temp Register)
						if (cpu_access)
							read_latch = (read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o4_1, 0, 18);
						else
							read_latch = v.m_filter.m_o4_1;
						break;
					case 5: // O3(n-2) (Filter 3 Temp Register #2)
						if (cpu_access)
							read_latch = (read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o3_2, 0, 18);
						else
							read_latch = v.m_filter.m_o3_2;
						break;
					case 6: // O3(n-1) (Filter 3 Temp Register #1)
						if (cpu_access)
							read_latch = (read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o3_1, 0, 18);
						else
							read_latch = v.m_filter.m_o3_1;
						break;
					case 7: // O2(n-2) (Filter 2 Temp Register #2)
						if (cpu_access)
							read_latch = (read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o2_2, 0, 18);
						else
							read_latch = v.m_filter.m_o2_2;
						break;
					case 8: // O2(n-1) (Filter 2 Temp Register #1)
						if (cpu_access)
							read_latch = (read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o2_1, 0, 18);
						else
							read_latch = v.m_filter.m_o2_1;
						break;
					case 9: // O1(n-1) (Filter 1 Temp Register)
						if (cpu_access)
							read_latch = (read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o1_1, 0, 18);
						else
							read_latch = v.m_filter.m_o1_1;
						break;
					case 10: // W_ST (Word Clock Start Register)
						read_latch = (read_latch & ~0x7f) | bitfield(m_w_st, 0, 7);
						break;
					case 11: // W_END (Word Clock End Register)
						read_latch = (read_latch & ~0x7f) | bitfield(m_w_end, 0, 7);
						break;
					case 12: // LR_END (Left/Right Clock End Register)
						read_latch = (read_latch & ~0x7f) | bitfield(m_lr_end, 0, 7);
						break;
				}
			}
			else // Page 0 - 31
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						read_latch = (read_latch & ~0xffff) | 
						          (v.m_alu.m_cr.stop0 ? 0x0001 : 0x0000)
										| (v.m_alu.m_cr.stop1 ? 0x0002 : 0x0000)
										| (v.m_alu.m_cr.lei   ? 0x0004 : 0x0000)
										| (v.m_alu.m_cr.lpe   ? 0x0008 : 0x0000)
										| (v.m_alu.m_cr.ble   ? 0x0010 : 0x0000)
										| (v.m_alu.m_cr.irqe  ? 0x0020 : 0x0000)
										| (v.m_alu.m_cr.dir   ? 0x0040 : 0x0000)
										| (v.m_alu.m_cr.irq   ? 0x0080 : 0x0000)
										| (bitfield(v.m_filter.m_lp, 0, 2) << 8)
										| (bitfield(v.m_cr.ca, 0, 3) << 10)
										| (v.m_cr.cmpd        ? 0x2000 : 0x0000)
										| (bitfield(v.m_cr.bs, 0, 2) << 14);
						break;
					case 1: // FC (Frequency Control)
						read_latch = (read_latch & ~0x1ffff) | bitfield(v.m_alu.m_fc, 0, 17);
						break;
					case 2: // LVOL (Left Volume)
						read_latch = (read_latch & ~0xffff) | bitfield(v.m_lvol, 0, 16);
						break;
					case 3: // LVRAMP (Left Volume Ramp)
						read_latch = (read_latch & ~0xff00) | (bitfield(v.m_lvramp, 0, 8) << 8);
						break;
					case 4: // RVOL (Right Volume)
						read_latch = (read_latch & ~0xffff) | bitfield(v.m_rvol, 0, 16);
						break;
					case 5: // RVRAMP (Right Volume Ramp)
						read_latch = (read_latch & ~0xff00) | (bitfield(v.m_rvramp, 0, 8) << 8);
						break;
					case 6: // ECOUNT (Envelope Counter)
						read_latch = (read_latch & ~0x01ff) | bitfield(v.m_ecount, 0, 9);
						break;
					case 7: // K2 (Filter Cutoff Coefficient #2)
						read_latch = (read_latch & ~0xffff) | bitfield(v.m_filter.m_k2, 0, 16);
						break;
					case 8: // K2RAMP (Filter Cutoff Coefficient #2 Ramp)
						read_latch = (read_latch & ~0xff01) | (bitfield(v.m_k2ramp.ramp, 0, 8) << 8) | (v.m_k2ramp.slow ? 0x0001 : 0x0000);
						break;
					case 9: // K1 (Filter Cutoff Coefficient #1)
						read_latch = (read_latch & ~0xffff) | bitfield(v.m_filter.m_k1, 0, 16);
						break;
					case 10: // K1RAMP (Filter Cutoff Coefficient #1 Ramp)
						read_latch = (read_latch & ~0xff01) | (bitfield(v.m_k1ramp.ramp, 0, 8) << 8) | (v.m_k1ramp.slow ? 0x0001 : 0x0000);
						break;
					case 11: // ACT (Number of voices)
						read_latch = (read_latch & ~0x1f) | bitfield(m_active, 0, 5);
						break;
					case 12: // MODE (Global Mode)
						read_latch = (read_latch & ~0x1f) |
						          (m_mode.lrclk_en ? 0x01 : 0x00)
										| (m_mode.wclk_en  ? 0x02 : 0x00)
										| (m_mode.bclk_en  ? 0x04 : 0x00)
										| (m_mode.master   ? 0x08 : 0x00)
										| (m_mode.dual     ? 0x10 : 0x00);
						break;
				}
			}
		}
	}

	return read_latch;
}

void es5506_core::regs_w(u8 page, u8 address, u32 data, bool cpu_access)
{
	if (address >= 13) // Global registers
	{
		switch (address)
		{
			case 13: // POT (Pot A/D Register)
				// Read only
				break;
			case 14: // IRQV (Interrupting voice vector)
				// Read only
				break;
			case 15: // PAGE (Page select register)
				m_page = bitfield(data, 0, 7);
				break;
		}
	}
	else
	{
		if (bitfield(page, 6)) // Channel registers are Write only, and for test purposes
		{
			switch (address)
			{
				case 0: // CH0L (Channel 0 Left)
				case 2: // CH1L (Channel 1 Left)
				case 4: // CH2L (Channel 2 Left)
				case 6: // CH3L (Channel 3 Left)
				case 8: // CH4L (Channel 4 Left)
				case 10: // CH5L (Channel 5 Left)
					m_ch[bitfield(address, 1, 3)].m_left = sign_ext<s32>(bitfield(data, 0, 23), 23);
					break;
				case 1: // CH0R (Channel 0 Right)
				case 3: // CH1R (Channel 1 Right)
				case 5: // CH2R (Channel 2 Right)
				case 7: // CH3R (Channel 3 Right)
				case 9: // CH4R (Channel 4 Right)
				case 11: // CH5R (Channel 5 Right)
					m_ch[bitfield(address, 1, 3)].m_right = sign_ext<s32>(bitfield(data, 0, 23), 23);
					break;
			}
		}
		else
		{
			const u8 voice = bitfield(page, 0, 5); // Voice select
			voice_t &v = m_voice[voice];
			if (bitfield(page, 5)) // Page 32 - 63
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						v.m_alu.m_cr.stop0 = bitfield(data, 0);
						v.m_alu.m_cr.stop1 = bitfield(data, 1);
						v.m_alu.m_cr.lei   = bitfield(data, 2);
						v.m_alu.m_cr.lpe   = bitfield(data, 3);
						v.m_alu.m_cr.ble   = bitfield(data, 4);
						v.m_alu.m_cr.irqe  = bitfield(data, 5);
						v.m_alu.m_cr.dir   = bitfield(data, 6);
						v.m_alu.m_cr.irq   = bitfield(data, 7);
						v.m_filter.m_lp    = bitfield(data, 8, 2);
						v.m_cr.ca          = std::min<u8>(5, bitfield(data, 10, 3));
						v.m_cr.cmpd        = bitfield(data, 13);
						v.m_cr.bs          = bitfield(data, 14, 2);
						break;
					case 1: // START (Loop Start Register)
						v.m_alu.m_start = data & 0xfffff800;
						break;
					case 2: // END (Loop End Register)
						v.m_alu.m_end = data & 0xffffff80;
						break;
					case 3: // ACCUM (Accumulator Register)
						v.m_alu.m_accum = data;
						break;
					case 4: // O4(n-1) (Filter 4 Temp Register)
						v.m_filter.m_o4_1 = sign_ext<s32>(bitfield(data, 0, 18), 18);
						break;
					case 5: // O3(n-2) (Filter 3 Temp Register #2)
						v.m_filter.m_o3_2 = sign_ext<s32>(bitfield(data, 0, 18), 18);
						break;
					case 6: // O3(n-1) (Filter 3 Temp Register #1)
						v.m_filter.m_o3_1 = sign_ext<s32>(bitfield(data, 0, 18), 18);
						break;
					case 7: // O2(n-2) (Filter 2 Temp Register #2)
						v.m_filter.m_o2_2 = sign_ext<s32>(bitfield(data, 0, 18), 18);
						break;
					case 8: // O2(n-1) (Filter 2 Temp Register #1)
						v.m_filter.m_o2_1 = sign_ext<s32>(bitfield(data, 0, 18), 18);
						break;
					case 9: // O1(n-1) (Filter 1 Temp Register)
						v.m_filter.m_o1_1 = sign_ext<s32>(bitfield(data, 0, 18), 18);
						break;
					case 10: // W_ST (Word Clock Start Register)
						m_w_st = bitfield(data, 0, 7);
						break;
					case 11: // W_END (Word Clock End Register)
						m_w_end = bitfield(data, 0, 7);
						break;
					case 12: // LR_END (Left/Right Clock End Register)
						m_lr_end = bitfield(data, 0, 7);
						m_lrclk.set_width(m_lr_end);
						break;
				}
			}
			else // Page 0 - 31
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						v.m_alu.m_cr.stop0 = bitfield(data, 0);
						v.m_alu.m_cr.stop1 = bitfield(data, 1);
						v.m_alu.m_cr.lei   = bitfield(data, 2);
						v.m_alu.m_cr.lpe   = bitfield(data, 3);
						v.m_alu.m_cr.ble   = bitfield(data, 4);
						v.m_alu.m_cr.irqe  = bitfield(data, 5);
						v.m_alu.m_cr.dir   = bitfield(data, 6);
						v.m_alu.m_cr.irq   = bitfield(data, 7);
						v.m_filter.m_lp    = bitfield(data, 8, 2);
						v.m_cr.ca          = std::min<u8>(5, bitfield(data, 10, 3));
						v.m_cr.cmpd        = bitfield(data, 13);
						v.m_cr.bs          = bitfield(data, 14, 2);
						break;
					case 1: // FC (Frequency Control)
						v.m_alu.m_fc = bitfield(data, 0, 17);
						break;
					case 2: // LVOL (Left Volume)
						v.m_lvol = bitfield(data, 0, 16);
						break;
					case 3: // LVRAMP (Left Volume Ramp)
						v.m_lvramp = bitfield(data, 8, 8);
						break;
					case 4: // RVOL (Right Volume)
						v.m_rvol = bitfield(data, 0, 16);
						break;
					case 5: // RVRAMP (Right Volume Ramp)
						v.m_rvramp = bitfield(data, 8, 8);
						break;
					case 6: // ECOUNT (Envelope Counter)
						v.m_ecount = bitfield(data, 0, 9);
						break;
					case 7: // K2 (Filter Cutoff Coefficient #2)
						v.m_filter.m_k2 = bitfield(data, 0, 16);
						break;
					case 8: // K2RAMP (Filter Cutoff Coefficient #2 Ramp)
						v.m_k2ramp.slow = bitfield(data, 0);
						v.m_k2ramp.ramp = bitfield(data, 8, 8);
						break;
					case 9: // K1 (Filter Cutoff Coefficient #1)
						v.m_filter.m_k1 = bitfield(data, 0, 16);
						break;
					case 10: // K1RAMP (Filter Cutoff Coefficient #1 Ramp)
						v.m_k1ramp.slow = bitfield(data, 0);
						v.m_k1ramp.ramp = bitfield(data, 8, 8);
						break;
					case 11: // ACT (Number of voices)
						m_active = std::max<u8>(4, bitfield(data, 0, 5));
						break;
					case 12: // MODE (Global Mode)
						m_mode.lrclk_en = bitfield(data, 0);
						m_mode.wclk_en  = bitfield(data, 1);
						m_mode.bclk_en  = bitfield(data, 2);
						m_mode.master   = bitfield(data, 3);
						m_mode.dual     = bitfield(data, 4);
						break;
				}
			}
		}
	}
}
