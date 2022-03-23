/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Ensoniq ES5504/ES5505/ES5506 emulation core

	After ES5503 DOC's appeared, Ensoniq announces ES5504 DOC II, ES5505 OTIS, ES5506 OTTO.

	These are not just PCM chip; but with built-in 4 pole filters and variable voice limits.

	It can be trades higher sample rate and finer frequency and Tons of voices, or vice-versa.

	These are mainly used with their synthesizers, musical stuffs. It's also mainly paired with ES5510 ESP/ES5511 ESP2 for post processing.
	ES5506 can be paired with itself, It's called Dual chip configuration and Both chips are can be shares same memory spaces.

	ES5505 was also mainly used on Taito's early- to late-90s arcade hardware for their PCM sample based sound system,
	paired with ES5510 ESP for post processing. It's configuration is borrowed from Ensoniq's 32 Voice synths powered by these chips.
	It's difference is external logic to adds per-voice bankswitching looks like what Konami doing on K007232.

	Atari Panther was will be use ES5505, but finally canceled.

	Ensoniq's ISA Sound Card for PC, Soundscape used ES5506, "Elite" model has optional daughterboard with ES5510 for digital effects.

	Related chips:
	ES5530 "OPUS" variant is 2-in-one chip with built-in ES5506 and Sequoia.
	ES5540 "OTTOFX" variant is ES5506 and ES5510 merged in single package.
	ES5548 "OTTO48" variant is used at late-90s ensoniq synths and musical instruments, 2 ES5506s are merged in single package, or with 48 voices in chip?

	Chip difference:
	ES5504 to ES5505:
	Total voice amount is expanded to 32, rather than 25.
	ADC and DAC is completely redesigned. it's has now voice-independent 10 bit and Sony/Burr-Brown format DAC.
	Output channel and Volume is changed to 16 mono to 4 stereo, 12 bit Analog to 8 bit Stereo digital, also Floating point-ish format and independent per left and right output.
	Channel 3 is can be Input/Output.
	Channel output is can be accessible at host for test purpose.
	Max sample memory is expanded to 2MWords (1MWords * 2 Banks)

	ES5505 to ES5506:
	Frequency is more finer now: 11 bit fraction rather than 9 bit.
	Output channel and Volume is changed to 4 stereo to 6 stereo, 8 bit to 16 bit, but only 12 bit is used for calculation; 4 LSB is used for envelope ramping.
	Transwave flag is added - its helpful for transwave process, with interrupt per voices.
	Hardware envelope is added - K1, K2, Volume value is can be modified in run-time. also K1, K2 is expanded to 16 bit for finer envelope ramping.
	Filter calculation resolution is expanded to 18 bit.
	All channels are output, Serial output is now partially programmable.
	Max sample memory is expanded to 8MWords (2MWords * 4 Banks)

	Register format between these chips are incompatible.

*/

#include "es550x.hpp"

// Shared functions
void es550x_shared_core::reset()
{
	m_page = 0;
	m_irqv.reset();
	m_active = max_voices() - 1;
	m_voice_cycle = 0;
}

void es550x_shared_core::es550x_voice_t::reset()
{
	m_cr.reset();
	m_alu.reset();
	m_filter.reset();
}

void es550x_shared_core::es550x_alu_t::loop_exec(bool transwave)
{
	if (m_cr.irqe) // Set IRQ
		m_cr.irq = 1;

	if (m_cr.dir) // Reverse playback
	{
		if (m_cr.lpe) // Loop enable
		{
			if (m_cr.ble) // Bidirectional
			{
				m_cr.dir = 0;
				m_accum = m_start + (m_start - m_accum);
			}
			else// Normal
				m_accum = (m_accum + m_start) - m_end;
		}
		else if (m_cr.ble && transwave) // Transwave
		{
			m_cr.lpe = m_cr.ble = 0;
			m_cr.lei = 1; // Loop end ignore
			m_accum = (m_accum + m_start) - m_end;
		}
		else // Stop
			m_cr.stop0 = 1;
	}
	else
	{
		if (m_cr.lpe) // Loop enable
		{
			if (m_cr.ble) // Bidirectional
			{
				m_cr.dir = 1;
				m_accum = m_end - (m_end - m_accum);
			}
			else // Normal
				m_accum = (m_accum - m_end) + m_start;
		}
		else if (m_cr.ble && transwave) // Transwave
		{
			m_cr.lpe = m_cr.ble = 0;
			m_cr.lei = 1; // Loop end ignore
			m_accum = (m_accum - m_end) + m_start;
		}
		else // Stop
			m_cr.stop0 = 1;
	}
}

void es550x_shared_core::es550x_filter_t::reset()
{
	m_lp.reset();
	m_k2 = 0;
	m_k1 = 0;
	m_o1_1 = 0;
	m_o2_1 = 0;
	m_o2_2 = 0;
	m_o3_1 = 0;
	m_o3_2 = 0;
	m_o4_1 = 0;
}

void es550x_shared_core::es550x_filter_t::tick(s32 in)
{
	s32 coeff_k1 = s32(bitfield(m_k1,4,12)); // 12 MSB used
	s32 coeff_k2 = s32(bitfield(m_k2,4,12)); // 12 MSB used
	// Store previous filter data
	m_o2_2 = m_o2_1;
	m_o3_2 = m_o3_1;

	// First and second stage: LP/K1, LP/K1 Fixed
	m_o1_1 = lp_exec(coeff_k1, in, m_o1_1);
	m_o2_1 = lp_exec(coeff_k1, m_o1_1, m_o2_1);
	switch (m_lp.lp)
	{
		case 0: // LP3 = 0, LP4 = 0: HP/K2, HP/K2
		default:
			m_o3_1 = hp_exec(coeff_k2, m_o2_1, m_o3_1, m_o2_2);
			m_o4_1 = hp_exec(coeff_k2, m_o3_1, m_o4_1, m_o3_2);
		break;
		case 1: // LP3 = 0, LP4 = 1: HP/K2, LP/K1
			m_o3_1 = hp_exec(coeff_k2, m_o2_1, m_o3_1, m_o2_2);
			m_o4_1 = lp_exec(coeff_k1, m_o3_1, m_o4_1);
		break;
		case 2: // LP3 = 1, LP4 = 0: LP/K2, LP/K2
			m_o3_1 = lp_exec(coeff_k2, m_o2_1, m_o3_1);
			m_o4_1 = lp_exec(coeff_k2, m_o3_1, m_o4_1);
		break;
		case 3: // LP3 = 1, LP4 = 1: LP/K2, LP/K1
			m_o3_1 = lp_exec(coeff_k2, m_o2_1, m_o3_1);
			m_o4_1 = lp_exec(coeff_k1, m_o3_1, m_o4_1);
		break;
	}
}

void es550x_shared_core::irq_exec(es550x_voice_t &voice, u8 index)
{
	const u8 prev = m_irqv.irqb;
	if (voice.m_alu.m_cr.irq)
	{
		if (m_irqv.irqb)
		{
			m_irqv.set(index);
			voice.m_alu.m_cr.irq = 0;
		}
	}
	if (prev != m_irqv.irqb)
		irq_update();
}

// ES5504
void es5504_core::tick()
{
	// Update voice
	m_voice[m_voice_cycle].tick(m_voice_cycle);

	// Update IRQ
	irq_exec(m_voice[m_voice_cycle], m_voice_cycle);

	// Refresh output
	if ((++m_voice_cycle) > std::min<u8>(24, m_active)) // ~ 25 voices
	{
		m_voice_cycle = 0;
		std::fill(std::begin(m_ch), std::end(m_ch), 0);
		for (auto & elem : m_voice)
		{
			m_ch[elem.m_cr.ca] += elem.m_ch;
			elem.m_ch = 0;
		}
	}
}

void es5504_core::voice_t::tick(u8 voice)
{
	m_ch = 0;
	if (m_alu.busy())
	{
		u32 addr = bitfield(m_alu.m_accum, 9, 20);

		// Fetch samples
		s32 sample = m_host.m_intf.read_sample(voice, m_cr.ca, addr++);
		s32 sample2 = m_host.m_intf.read_sample(voice, m_cr.ca, bitfield(addr, 0, 20));

		// Filter execute
		m_filter.tick(m_alu.interpolation(sample, sample2, 9));

		// Send to output
		m_ch = (sign_ext<s32>(m_filter.m_o4_1, 13) * m_volume) >> 12; // Analog multiplied in real chip

		// ALU execute
		if (m_alu.tick(29))
			m_alu.loop_exec(false);

		// ADC check
		adc_exec();
	}
}

// ADC; Correct?
void es5504_core::voice_t::adc_exec()
{
	if (m_cr.adc)
		m_host.m_adc = m_host.m_intf.adc_r() & ~0x7;
}

void es5504_core::reset()
{
	for (auto & elem : m_voice)
		elem.reset();

	m_adc = 0;
	std::fill(std::begin(m_ch), std::end(m_ch), 0);
}

void es5504_core::voice_t::reset()
{
	es550x_shared_core::es550x_voice_t::reset();
	m_volume = 0;
	m_ch = 0;
}

// ES5505
void es5505_core::tick()
{
	// Update voice
	m_voice[m_voice_cycle].tick(m_voice_cycle);

	// Update IRQ
	irq_exec(m_voice[m_voice_cycle], m_voice_cycle);

	// Refresh output
	if ((++m_voice_cycle) > std::clamp<u8>(m_active, 7, 31)) // 8 ~ 32 voices
	{
		m_voice_cycle = 0;
		for (auto & elem : m_ch)
			elem.reset();

		for (auto & elem : m_voice)
		{
			m_ch[bitfield(elem.m_cr.ca, 0, 2)].m_left += elem.m_ch.m_left;
			m_ch[bitfield(elem.m_cr.ca, 0, 2)].m_right += elem.m_ch.m_right;
			elem.m_ch.reset();
		}
	}
}

void es5505_core::voice_t::tick(u8 voice)
{
	m_ch.reset();
	if (m_alu.busy())
	{
		u32 addr = bitfield(m_alu.m_accum, 9, 20);

		// Fetch samples
		s32 sample = m_host.m_intf.read_sample(voice, bitfield(m_cr.bs, 0), addr++);
		s32 sample2 = m_host.m_intf.read_sample(voice, bitfield(m_cr.bs, 0), bitfield(addr, 0, 20));

		// Filter execute
		m_filter.tick(m_alu.interpolation(sample, sample2, 9));

		// Send to output
		m_ch.m_left = volume_calc(m_lvol, sign_ext<s32>(m_filter.m_o4_1, 16));
		m_ch.m_right = volume_calc(m_rvol, sign_ext<s32>(m_filter.m_o4_1, 16));

		// ALU execute
		if (m_alu.tick(29))
			m_alu.loop_exec(false);
	}
}

void es5505_core::reset()
{
	for (auto & elem : m_voice)
		elem.reset();

	m_sermode.reset();
	for (auto & elem : m_ch)
		elem.reset();
}

void es5505_core::voice_t::reset()
{
	es550x_shared_core::es550x_voice_t::reset();
	m_lvol = 0;
	m_rvol = 0;
	m_ch.reset();
}

// ES5506
void es5506_core::tick()
{
	// Serial output
	if ((!m_mode.bclk_en) && (!m_mode.wclk_en) && (!m_mode.lrclk_en) && (m_w_end > m_w_st))
	{
		const int data_bit = std::min<int>(20, std::min<int>((m_w_end - m_w_st), m_lr_end)); // only 20 bits are transferred
		if (data_bit < 20)
		{
			for (u8 i = 0; i < 6; i++)
			{
				m_output[i].m_left = m_ch[i].m_left >> (20 - data_bit);
				m_output[i].m_right = m_ch[i].m_right >> (20 - data_bit);
			}
		}
		else
		{
			for (auto & elem : m_output)
			{
				elem.reset();
			}
		}
	}
	else
	{
		for (auto & elem : m_output)
		{
			elem.reset();
		}
	}

	// Update voice
	m_voice[m_voice_cycle].tick(m_voice_cycle);

	// Update IRQ
	irq_exec(m_voice[m_voice_cycle], m_voice_cycle);

	// Refresh output
	if ((++m_voice_cycle) > std::clamp<u8>(m_active, 4, 31)) // 5 ~ 32 voices
	{
		m_voice_cycle = 0;
		for (auto & elem : m_ch)
			elem.reset();

		for (auto & elem : m_voice)
		{
			const u8 ca = bitfield(elem.m_cr.ca, 0, 3);
			if (ca < 6)
			{
				m_ch[elem.m_cr.ca].m_left += elem.m_ch.m_left;
				m_ch[elem.m_cr.ca].m_right += elem.m_ch.m_right;
			}
			elem.m_ch.reset();
		}
	}
}

void es5506_core::voice_t::tick(u8 voice)
{
	m_ch.reset();
	if (m_alu.busy())
	{
		u32 addr = bitfield(m_alu.m_accum, 11, 21);

		// Fetch samples
		s32 sample = m_host.m_intf.read_sample(voice, m_cr.bs, addr++);
		s32 sample2 = m_host.m_intf.read_sample(voice, m_cr.bs, bitfield(addr, 0, 21));

		// Filter execute
		m_filter.tick(m_alu.interpolation(sample, sample2, 11));

		// Send to output
		m_ch.m_left = volume_calc(sign_ext<s32>(m_filter.m_o4_1, 18), m_lvol);
		m_ch.m_right = volume_calc(sign_ext<s32>(m_filter.m_o4_1, 18), m_rvol);

		// ALU execute
		if (m_alu.tick(32))
			m_alu.loop_exec(true);
	}
	// Envelope
	if (m_ecount != 0)
	{
		// Left and Right volume
		if (bitfield(m_lvramp, 8, 8) != 0)
			m_lvol = std::clamp<s32>(m_lvol + sign_ext<s32>(bitfield(m_lvramp, 8, 8), 8), 0, 0xffff);
		if (bitfield(m_rvramp, 8, 8) != 0)
			m_rvol = std::clamp<s32>(m_rvol + sign_ext<s32>(bitfield(m_rvramp, 8, 8), 8), 0, 0xffff);

		// Filter coeffcient
		if ((m_k1ramp.ramp != 0) && ((m_k1ramp.slow == 0) || (bitfield(m_filtcount, 0, 3) == 0)))
			m_filter.m_k1 = std::clamp<s32>(m_filter.m_k1 + sign_ext<s32>(m_k1ramp.ramp, 8), 0, 0xffff);
		if ((m_k2ramp.ramp != 0) && ((m_k2ramp.slow == 0) || (bitfield(m_filtcount, 0, 3) == 0)))
			m_filter.m_k2 = std::clamp<s32>(m_filter.m_k2 + sign_ext<s32>(m_k2ramp.ramp, 8), 0, 0xffff);

		m_ecount--;
	}
	m_filtcount = bitfield(m_filtcount + 1, 0, 3);
}

void es5506_core::reset()
{
	for (auto & elem : m_voice)
		elem.reset();

	m_w_st = 0;
	m_w_end = 0;
	m_lr_end = 0;
	m_mode.reset();
	m_read_latch = 0xffffffff;
	m_write_latch = 0xffffffff;
	for (auto & elem : m_ch)
		elem.reset();
	for (auto & elem : m_output)
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
}

// Accessors

// ES5504
u16 es5504_core::read(u8 address, bool cpu_access = false)
{
	u16 ret = 0xffff;
	address = bitfield(address, 0, 4); // 4 bit address for CPU access

	if (address >= 12) // Global registers
	{
		switch (address)
		{
			case 12: // A/D (A to D Convert/Test)
				ret = (ret & ~0xfffb) | (m_adc & 0xfffb);
				break;
			case 13: // ACT (Number of voices)
				ret = (ret & ~0x1f) | bitfield(m_active, 0, 5);
				break;
			case 14: // IRQV (Interrupting voice vector)
				ret = (ret & ~0x9f) | (m_irqv.irqb ? 0x80 : 0) | bitfield(m_irqv.voice, 0, 5);
				if (cpu_access)
				{
					m_irqv.clear();
					if (bitfield(ret, 7) != m_irqv.irqb)
						irq_update();
				}
				break;
			case 15: // PAGE (Page select register)
				ret = (ret & ~0x3f) | bitfield(m_page, 0, 6);
				break;
		}
	}
	else // Voice specific registers
	{
		const u8 voice = bitfield(m_page, 0, 5); // Voice select
		if (voice < 25)
		{
			voice_t &v = m_voice[voice];
			if (bitfield(m_page, 5)) // Page 32 - 56
			{
				switch (address)
				{
					case 1: // O4(n-1) (Filter 4 Temp Register)
						ret = v.m_filter.m_o4_1;
						break;
					case 2: // O3(n-2) (Filter 3 Temp Register #2)
						ret = v.m_filter.m_o3_2;
						break;
					case 3: // O3(n-1) (Filter 3 Temp Register #1)
						ret = v.m_filter.m_o3_1;
						break;
					case 4: // O2(n-2) (Filter 2 Temp Register #2)
						ret = v.m_filter.m_o2_2;
						break;
					case 5: // O2(n-1) (Filter 2 Temp Register #1)
						ret = v.m_filter.m_o2_1;
						break;
					case 6: // O1(n-1) (Filter 1 Temp Register)
						ret = v.m_filter.m_o1_1;
						break;
				}
			}
			else // Page 0 - 24
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						ret = (ret & ~0xff) | 
						      (v.m_alu.m_cr.stop0 ? 0x01 : 0x00)
						    | (v.m_alu.m_cr.stop1 ? 0x02 : 0x00)
						    | (v.m_cr.adc         ? 0x04 : 0x00)
						    | (v.m_alu.m_cr.lpe   ? 0x08 : 0x00)
						    | (v.m_alu.m_cr.ble   ? 0x10 : 0x00)
						    | (v.m_alu.m_cr.irqe  ? 0x20 : 0x00)
						    | (v.m_alu.m_cr.dir   ? 0x40 : 0x00)
						    | (v.m_alu.m_cr.irq   ? 0x80 : 0x00);
						break;
					case 1: // FC (Frequency Control)
						ret = (ret & ~0xfffe) | (v.m_alu.m_fc << 1);
						break;
					case 2: // STRT-H (Loop Start Register High)
						ret = (ret & ~0x1fff) | bitfield(v.m_alu.m_start, 16, 13);
						break;
					case 3: // STRT-L (Loop Start Register Low)
						ret = (ret & ~0xffe0) | (v.m_alu.m_start & 0xffe0);
						break;
					case 4: // END-H (Loop End Register High)
						ret = (ret & ~0x1fff) | bitfield(v.m_alu.m_end, 16, 13);
						break;
					case 5: // END-L (Loop End Register Low)
						ret = (ret & ~0xffe0) | (v.m_alu.m_end & 0xffe0);
						break;
					case 6: // K2 (Filter Cutoff Coefficient #2)
						ret = (ret & ~0xfff0) | (v.m_filter.m_k2 & 0xfff0);
						break;
					case 7: // K1 (Filter Cutoff Coefficient #1)
						ret = (ret & ~0xfff0) | (v.m_filter.m_k1 & 0xfff0);
						break;
					case 8: // Volume
						ret = (ret & ~0xfff0) | ((v.m_volume << 4) & 0xfff0);
						break;
					case 9: // CA (Filter Config, Channel Assign)
						ret = (ret & ~0x3f) | 
						      bitfield(v.m_cr.ca, 0, 4)
						    | (bitfield(v.m_filter.m_lp.lp, 0, 2) << 4);
						break;
					case 10: // ACCH (Accumulator High)
						ret = (ret & ~0x1fff) | bitfield(v.m_alu.m_accum, 16, 13);
						break;
					case 11: // ACCL (Accumulator Low)
						ret = bitfield(v.m_alu.m_accum, 0, 16);
						break;
				}
			}
		}
	}

	return ret;
}

void es5504_core::write(u8 address, u16 data, bool cpu_access = false)
{
	address = bitfield(address, 0, 4); // 4 bit address for CPU access

	if (address >= 12) // Global registers
	{
		switch (address)
		{
			case 12: // A/D (A to D Convert/Test)
				if (bitfield(m_adc, 0)) // Writable ADC
				{
					m_adc = (m_adc & 7) | (data & ~7);
					m_intf.adc_w(m_adc & ~7);
				}
				m_adc = (m_adc & ~3) | (data & 3);
				break;
			case 13: // ACT (Number of voices)
				m_active = std::min<u8>(24, bitfield(data, 0, 5));
				break;
			case 14: // IRQV (Interrupting voice vector)
				// Read only
				break;
			case 15: // PAGE (Page select register)
				m_page = bitfield(data, 0, 6);
				break;
		}
	}
	else // Voice specific registers
	{
		const u8 voice = bitfield(m_page, 0, 5); // Voice select
		if (voice < 25)
		{
			voice_t &v = m_voice[voice];
			if (bitfield(m_page, 5)) // Page 32 - 56
			{
				switch (address)
				{
					case 1: // O4(n-1) (Filter 4 Temp Register)
						v.m_filter.m_o4_1 = sign_ext<s32>(data, 16);
						break;
					case 2: // O3(n-2) (Filter 3 Temp Register #2)
						v.m_filter.m_o3_2 = sign_ext<s32>(data, 16);
						break;
					case 3: // O3(n-1) (Filter 3 Temp Register #1)
						v.m_filter.m_o3_1 = sign_ext<s32>(data, 16);
						break;
					case 4: // O2(n-2) (Filter 2 Temp Register #2)
						v.m_filter.m_o2_2 = sign_ext<s32>(data, 16);
						break;
					case 5: // O2(n-1) (Filter 2 Temp Register #1)
						v.m_filter.m_o2_1 = sign_ext<s32>(data, 16);
						break;
					case 6: // O1(n-1) (Filter 1 Temp Register)
						v.m_filter.m_o1_1 = sign_ext<s32>(data, 16);
						break;
				}
			}
			else // Page 0 - 24
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						v.m_alu.m_cr.stop0 = bitfield(data, 0);
						v.m_alu.m_cr.stop1 = bitfield(data, 1);
						v.m_cr.adc = bitfield(data, 2);
						v.m_alu.m_cr.lpe = bitfield(data, 3);
						v.m_alu.m_cr.ble = bitfield(data, 4);
						v.m_alu.m_cr.irqe = bitfield(data, 5);
						v.m_alu.m_cr.dir = bitfield(data, 6);
						v.m_alu.m_cr.irq = bitfield(data, 7);
						break;
					case 1: // FC (Frequency Control)
						v.m_alu.m_fc = bitfield(data, 1, 15);
						break;
					case 2: // STRT-H (Loop Start Register High)
						v.m_alu.m_start = (v.m_alu.m_start & ~0x1fff0000) | (bitfield<u32>(data, 0, 13) << 16);
						break;
					case 3: // STRT-L (Loop Start Register Low)
						v.m_alu.m_start = (v.m_alu.m_start & ~0xffe0) | (data & 0xffe0);
						break;
					case 4: // END-H (Loop End Register High)
						v.m_alu.m_end = (v.m_alu.m_end & ~0x1fff0000) | (bitfield<u32>(data, 0, 13) << 16);
						break;
					case 5: // END-L (Loop End Register Low)
						v.m_alu.m_end = (v.m_alu.m_end & ~0xffe0) | (data & 0xffe0);
						break;
					case 6: // K2 (Filter Cutoff Coefficient #2)
						v.m_filter.m_k2 = data & 0xfff0;
						break;
					case 7: // K1 (Filter Cutoff Coefficient #1)
						v.m_filter.m_k1 = data & 0xfff0;
						break;
					case 8: // Volume
						v.m_volume = bitfield(data, 4, 12);
						break;
					case 9: // CA (Filter Config, Channel Assign)
						v.m_cr.ca = bitfield(data, 0, 4);
						v.m_filter.m_lp.lp = bitfield(data, 4, 2);
						break;
					case 10: // ACCH (Accumulator High)
						v.m_alu.m_accum = (v.m_alu.m_accum & ~0x1fff0000) | (bitfield<u32>(data, 0, 13) << 16);
						break;
					case 11: // ACCL (Accumulator Low)
						v.m_alu.m_accum = (v.m_alu.m_accum & ~0xffff) | data;
						break;
				}
			}
		}
	}
}

// ES5505
u16 es5505_core::read(u8 address, bool cpu_access = false)
{
	u16 ret = 0xffff;
	address = bitfield(address, 0, 4); // 4 bit address for CPU access

	if (address >= 13) // Global registers
	{
		switch (address)
		{
			case 13: // ACT (Number of voices)
				ret = (ret & ~0x1f) | bitfield(m_active, 0, 5);
				break;
			case 14: // IRQV (Interrupting voice vector)
				ret = (ret & ~0x9f) | (m_irqv.irqb ? 0x80 : 0) | bitfield(m_irqv.voice, 0, 5);
				if (cpu_access)
				{
					m_irqv.clear();
					if (bitfield(ret, 7) != m_irqv.irqb)
						irq_update();
				}
				break;
			case 15: // PAGE (Page select register)
				ret = (ret & ~0x7f) | bitfield(m_page, 0, 7);
				break;
		}
	}
	else
	{
		if (bitfield(m_page, 6)) // Channel registers
		{
			switch (address)
			{
				case 0: // CH0L (Channel 0 Left)
				case 2: // CH1L (Channel 1 Left)
				case 4: // CH2L (Channel 2 Left)
					if (!cpu_access) // CPU can't read here
						ret = m_ch[bitfield(address, 0, 2)].m_left;
					break;
				case 1: // CH0R (Channel 0 Right)
				case 3: // CH1R (Channel 1 Right)
				case 5: // CH2R (Channel 2 Right)
					if (!cpu_access) // CPU can't read here
						ret = m_ch[bitfield(address, 0, 2)].m_right;
					break;
				case 6: // CH3L (Channel 3 Left)
					if ((!cpu_access) || m_sermode.adc)
						ret = m_ch[3].m_left;
					break;
				case 7: // CH3R (Channel 3 Right)
					if ((!cpu_access) || m_sermode.adc)
						ret = m_ch[3].m_right;
					break;
				case 8: // SERMODE (Serial Mode)
					ret = (ret & ~0xf807) |
					      (m_sermode.adc ? 0x01 : 0x00)
					    | (m_sermode.test ? 0x02 : 0x00)
					    | (m_sermode.sony_bb ? 0x04 : 0x00)
					    | (bitfield(m_sermode.msb, 0, 5) << 11);
					break;
				case 9: // PAR (Port A/D Register)
					ret = (ret & ~0x3f) | (m_intf.adc_r() & ~0x3f);
					break;
			}
		}
		else // Voice specific registers
		{
			const u8 voice = bitfield(m_page, 0, 5); // Voice select
			voice_t &v = m_voice[voice];
			if (bitfield(m_page, 5)) // Page 32 - 63
			{
				switch (address)
				{
					case 1: // O4(n-1) (Filter 4 Temp Register)
						ret = v.m_filter.m_o4_1;
						break;
					case 2: // O3(n-2) (Filter 3 Temp Register #2)
						ret = v.m_filter.m_o3_2;
						break;
					case 3: // O3(n-1) (Filter 3 Temp Register #1)
						ret = v.m_filter.m_o3_1;
						break;
					case 4: // O2(n-2) (Filter 2 Temp Register #2)
						ret = v.m_filter.m_o2_2;
						break;
					case 5: // O2(n-1) (Filter 2 Temp Register #1)
						ret = v.m_filter.m_o2_1;
						break;
					case 6: // O1(n-1) (Filter 1 Temp Register)
						ret = v.m_filter.m_o1_1;
						break;
				}
			}
			else // Page 0 - 31
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						ret = (ret & ~0xfff) | 
						      (v.m_alu.m_cr.stop0     ? 0x01 : 0x00)
						    | (v.m_alu.m_cr.stop1     ? 0x02 : 0x00)
						    | (bitfield(v.m_cr.bs, 0) ? 0x04 : 0x00)
						    | (v.m_alu.m_cr.lpe       ? 0x08 : 0x00)
						    | (v.m_alu.m_cr.ble       ? 0x10 : 0x00)
						    | (v.m_alu.m_cr.irqe      ? 0x20 : 0x00)
						    | (v.m_alu.m_cr.dir       ? 0x40 : 0x00)
						    | (v.m_alu.m_cr.irq       ? 0x80 : 0x00)
						    | (bitfield(v.m_cr.ca, 0, 2) << 8)
						    | (bitfield(v.m_filter.m_lp.lp, 0, 2) << 10);
						break;
					case 1: // FC (Frequency Control)
						ret = (ret & ~0xfffe) | (bitfield(v.m_alu.m_fc, 0, 15) << 1);
						break;
					case 2: // STRT-H (Loop Start Register High)
						ret = (ret & ~0x1fff) | bitfield(v.m_alu.m_start, 16, 13);
						break;
					case 3: // STRT-L (Loop Start Register Low)
						ret = (ret & ~0xffe0) | (v.m_alu.m_start & 0xffe0);
						break;
					case 4: // END-H (Loop End Register High)
						ret = (ret & ~0x1fff) | bitfield(v.m_alu.m_end, 16, 13);
						break;
					case 5: // END-L (Loop End Register Low)
						ret = (ret & ~0xffe0) | (v.m_alu.m_end & 0xffe0);
						break;
					case 6: // K2 (Filter Cutoff Coefficient #2)
						ret = (ret & ~0xfff0) | (v.m_filter.m_k2 & 0xfff0);
						break;
					case 7: // K1 (Filter Cutoff Coefficient #1)
						ret = (ret & ~0xfff0) | (v.m_filter.m_k1 & 0xfff0);
						break;
					case 8: // LVOL (Left Volume)
						ret = (ret & ~0xff00) | ((v.m_lvol << 8) & 0xff00);
						break;
					case 9: // RVOL (Right Volume)
						ret = (ret & ~0xff00) | ((v.m_rvol << 8) & 0xff00);
						break;
					case 10: // ACCH (Accumulator High)
						ret = (ret & ~0x1fff) | bitfield(v.m_alu.m_accum, 16, 13);
						break;
					case 11: // ACCL (Accumulator Low)
						ret = bitfield(v.m_alu.m_accum, 0, 16);
						break;
				}
			}
		}
	}

	return ret;
}

void es5505_core::write(u8 address, u16 data, bool cpu_access = false)
{
	address = bitfield(address, 0, 4); // 4 bit address for CPU access

	if (address >= 12) // Global registers
	{
		switch (address)
		{
			case 13: // ACT (Number of voices)
				m_active = std::clamp<u8>(bitfield(data, 0, 5), 7, 31);
				break;
			case 14: // IRQV (Interrupting voice vector)
				// Read only
				break;
			case 15: // PAGE (Page select register)
				m_page = bitfield(data, 0, 7);
				break;
		}
	}
	else // Voice specific registers
	{
		if (bitfield(m_page, 6)) // Channel registers
		{
			switch (address)
			{
				case 0: // CH0L (Channel 0 Left)
					if (m_sermode.test)
						m_ch[0].m_left = data;
					break;
				case 1: // CH0R (Channel 0 Right)
					if (m_sermode.test)
						m_ch[0].m_right = data;
					break;
				case 2: // CH1L (Channel 1 Left)
					if (m_sermode.test)
						m_ch[1].m_left = data;
					break;
				case 3: // CH1R (Channel 1 Right)
					if (m_sermode.test)
						m_ch[1].m_right = data;
					break;
				case 4: // CH2L (Channel 2 Left)
					if (m_sermode.test)
						m_ch[2].m_left = data;
					break;
				case 5: // CH2R (Channel 2 Right)
					if (m_sermode.test)
						m_ch[2].m_right = data;
					break;
				case 6: // CH3L (Channel 3 Left)
					if (m_sermode.test)
						m_ch[3].m_left = data;
					break;
				case 7: // CH3R (Channel 3 Right)
					if (m_sermode.test)
						m_ch[3].m_right = data;
					break;
				case 8: // SERMODE (Serial Mode)
					m_sermode.adc = bitfield(data, 0);
					m_sermode.test = bitfield(data, 1);
					m_sermode.sony_bb = bitfield(data, 2);
					m_sermode.msb = bitfield(data, 11, 5);
					break;
				case 9: // PAR (Port A/D Register)
					// Read only
					break;
			}
		}
		else // Voice specific registers
		{
			const u8 voice = bitfield(m_page, 0, 5); // Voice select
			voice_t &v = m_voice[voice];
			if (bitfield(m_page, 5)) // Page 32 - 56
			{
				switch (address)
				{
					case 1: // O4(n-1) (Filter 4 Temp Register)
						v.m_filter.m_o4_1 = sign_ext<s32>(data, 16);
						break;
					case 2: // O3(n-2) (Filter 3 Temp Register #2)
						v.m_filter.m_o3_2 = sign_ext<s32>(data, 16);
						break;
					case 3: // O3(n-1) (Filter 3 Temp Register #1)
						v.m_filter.m_o3_1 = sign_ext<s32>(data, 16);
						break;
					case 4: // O2(n-2) (Filter 2 Temp Register #2)
						v.m_filter.m_o2_2 = sign_ext<s32>(data, 16);
						break;
					case 5: // O2(n-1) (Filter 2 Temp Register #1)
						v.m_filter.m_o2_1 = sign_ext<s32>(data, 16);
						break;
					case 6: // O1(n-1) (Filter 1 Temp Register)
						v.m_filter.m_o1_1 = sign_ext<s32>(data, 16);
						break;
				}
			}
			else // Page 0 - 24
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						v.m_alu.m_cr.stop0 = bitfield(data, 0);
						v.m_alu.m_cr.stop1 = bitfield(data, 1);
						v.m_cr.bs = bitfield(data, 2);
						v.m_alu.m_cr.lpe = bitfield(data, 3);
						v.m_alu.m_cr.ble = bitfield(data, 4);
						v.m_alu.m_cr.irqe = bitfield(data, 5);
						v.m_alu.m_cr.dir = bitfield(data, 6);
						v.m_alu.m_cr.irq = bitfield(data, 7);
						v.m_cr.ca = bitfield(data, 8, 2);
						v.m_filter.m_lp.lp = bitfield(data, 10, 2);
						break;
					case 1: // FC (Frequency Control)
						v.m_alu.m_fc = bitfield(data, 1, 15);
						break;
					case 2: // STRT-H (Loop Start Register High)
						v.m_alu.m_start = (v.m_alu.m_start & ~0x1fff0000) | (bitfield<u32>(data, 0, 13) << 16);
						break;
					case 3: // STRT-L (Loop Start Register Low)
						v.m_alu.m_start = (v.m_alu.m_start & ~0xffe0) | (data & 0xffe0);
						break;
					case 4: // END-H (Loop End Register High)
						v.m_alu.m_end = (v.m_alu.m_end & ~0x1fff0000) | (bitfield<u32>(data, 0, 13) << 16);
						break;
					case 5: // END-L (Loop End Register Low)
						v.m_alu.m_end = (v.m_alu.m_end & ~0xffe0) | (data & 0xffe0);
						break;
					case 6: // K2 (Filter Cutoff Coefficient #2)
						v.m_filter.m_k2 = data & 0xfff0;
						break;
					case 7: // K1 (Filter Cutoff Coefficient #1)
						v.m_filter.m_k1 = data & 0xfff0;
						break;
					case 8: // LVOL (Left Volume)
						v.m_lvol = bitfield(data, 8, 8);
						break;
					case 9: // RVOL (Right Volume)
						v.m_rvol = bitfield(data, 8, 8);
						break;
					case 10: // ACCH (Accumulator High)
						v.m_alu.m_accum = (v.m_alu.m_accum & ~0x1fff0000) | (bitfield<u32>(data, 0, 13) << 16);
						break;
					case 11: // ACCL (Accumulator Low)
						v.m_alu.m_accum = (v.m_alu.m_accum & ~0xffff) | data;
						break;
				}
			}
		}
	}
}

// ES5506
u8 es5506_core::read(u8 address, bool cpu_access = false)
{
	const u8 byte = bitfield(address, 0, 2); // byte select
	const u8 shift = 24 - (byte << 3);
	if (byte != 0) // Return already latched register if not highest byte is accessing
		return bitfield(m_read_latch, shift, 8);

	address = bitfield(address, 2, 4); // 4 bit address for CPU access
	m_read_latch = 0xffffffff;
	if (address >= 13) // Global registers
	{
		switch (address)
		{
			case 13: // POT (Pot A/D Register)
				m_read_latch = (m_read_latch & ~0x3ff) | bitfield(m_intf.adc_r(), 0, 10);
				break;
			case 14: // IRQV (Interrupting voice vector)
				m_read_latch = (m_read_latch & ~0x9f) | (m_irqv.irqb ? 0x80 : 0) | bitfield(m_irqv.voice, 0, 5);
				if (cpu_access)
				{
					m_irqv.clear();
					if (bitfield(m_read_latch, 7) != m_irqv.irqb)
						irq_update();
				}
				break;
			case 15: // PAGE (Page select register)
				m_read_latch = (m_read_latch & ~0x7f) | bitfield(m_page, 0, 7);
				break;
		}
	}
	else
	{
		if (bitfield(m_page, 6)) // Channel registers are Write only
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
						m_read_latch = m_ch[bitfield(address, 1, 3)].m_left;
						break;
					case 1: // CH0R (Channel 0 Right)
					case 3: // CH1R (Channel 1 Right)
					case 5: // CH2R (Channel 2 Right)
					case 7: // CH3R (Channel 3 Right)
					case 9: // CH4R (Channel 4 Right)
					case 11: // CH5R (Channel 5 Right)
						m_read_latch = m_ch[bitfield(address, 1, 3)].m_right;
						break;
				}
			}
		}
		else
		{
			const u8 voice = bitfield(m_page, 0, 5); // Voice select
			voice_t &v = m_voice[voice];
			if (bitfield(m_page, 5)) // Page 32 - 63
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						m_read_latch = (m_read_latch & ~0xffff) | 
						          (v.m_alu.m_cr.stop0 ? 0x0001 : 0x0000)
										| (v.m_alu.m_cr.stop1 ? 0x0002 : 0x0000)
										| (v.m_alu.m_cr.lei   ? 0x0004 : 0x0000)
										| (v.m_alu.m_cr.lpe   ? 0x0008 : 0x0000)
										| (v.m_alu.m_cr.ble   ? 0x0010 : 0x0000)
										| (v.m_alu.m_cr.irqe  ? 0x0020 : 0x0000)
										| (v.m_alu.m_cr.dir   ? 0x0040 : 0x0000)
										| (v.m_alu.m_cr.irq   ? 0x0080 : 0x0000)
										| (bitfield(v.m_filter.m_lp.lp, 0, 2) << 8)
										| (bitfield(v.m_cr.ca, 0, 3) << 10)
										| (v.m_cr.cmpd        ? 0x2000 : 0x0000)
										| (bitfield(v.m_cr.bs, 0, 2) << 14);
						break;
					case 1: // START (Loop Start Register)
						m_read_latch = (m_read_latch & ~0xfffff800) | (v.m_alu.m_start & 0xfffff800);
						break;
					case 2: // END (Loop End Register)
						m_read_latch = (m_read_latch & ~0xffffff80) | (v.m_alu.m_end & 0xffffff80);
						break;
					case 3: // ACCUM (Accumulator Register)
						m_read_latch = v.m_alu.m_accum;
						break;
					case 4: // O4(n-1) (Filter 4 Temp Register)
						if (cpu_access)
							m_read_latch = (m_read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o4_1, 0, 18);
						else
							m_read_latch = v.m_filter.m_o4_1;
						break;
					case 5: // O3(n-2) (Filter 3 Temp Register #2)
						if (cpu_access)
							m_read_latch = (m_read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o3_2, 0, 18);
						else
							m_read_latch = v.m_filter.m_o3_2;
						break;
					case 6: // O3(n-1) (Filter 3 Temp Register #1)
						if (cpu_access)
							m_read_latch = (m_read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o3_1, 0, 18);
						else
							m_read_latch = v.m_filter.m_o3_1;
						break;
					case 7: // O2(n-2) (Filter 2 Temp Register #2)
						if (cpu_access)
							m_read_latch = (m_read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o2_2, 0, 18);
						else
							m_read_latch = v.m_filter.m_o2_2;
						break;
					case 8: // O2(n-1) (Filter 2 Temp Register #1)
						if (cpu_access)
							m_read_latch = (m_read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o2_1, 0, 18);
						else
							m_read_latch = v.m_filter.m_o2_1;
						break;
					case 9: // O1(n-1) (Filter 1 Temp Register)
						if (cpu_access)
							m_read_latch = (m_read_latch & ~0x3ffff) | bitfield(v.m_filter.m_o1_1, 0, 18);
						else
							m_read_latch = v.m_filter.m_o1_1;
						break;
					case 10: // W_ST (Word Clock Start Register)
						m_read_latch = (m_read_latch & ~0x7f) | bitfield(m_w_st, 0, 7);
						break;
					case 11: // W_END (Word Clock End Register)
						m_read_latch = (m_read_latch & ~0x7f) | bitfield(m_w_end, 0, 7);
						break;
					case 12: // LR_END (Left/Right Clock End Register)
						m_read_latch = (m_read_latch & ~0x7f) | bitfield(m_lr_end, 0, 7);
						break;
				}
			}
			else // Page 0 - 31
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						m_read_latch = (m_read_latch & ~0xffff) | 
						          (v.m_alu.m_cr.stop0 ? 0x0001 : 0x0000)
										| (v.m_alu.m_cr.stop1 ? 0x0002 : 0x0000)
										| (v.m_alu.m_cr.lei   ? 0x0004 : 0x0000)
										| (v.m_alu.m_cr.lpe   ? 0x0008 : 0x0000)
										| (v.m_alu.m_cr.ble   ? 0x0010 : 0x0000)
										| (v.m_alu.m_cr.irqe  ? 0x0020 : 0x0000)
										| (v.m_alu.m_cr.dir   ? 0x0040 : 0x0000)
										| (v.m_alu.m_cr.irq   ? 0x0080 : 0x0000)
										| (bitfield(v.m_filter.m_lp.lp, 0, 2) << 8)
										| (bitfield(v.m_cr.ca, 0, 3) << 10)
										| (v.m_cr.cmpd        ? 0x2000 : 0x0000)
										| (bitfield(v.m_cr.bs, 0, 2) << 14);
						break;
					case 1: // FC (Frequency Control)
						m_read_latch = (m_read_latch & ~0x1ffff) | bitfield(v.m_alu.m_fc, 0, 17);
						break;
					case 2: // LVOL (Left Volume)
						m_read_latch = (m_read_latch & ~0xffff) | bitfield(v.m_lvol, 0, 16);
						break;
					case 3: // LVRAMP (Left Volume Ramp)
						m_read_latch = (m_read_latch & ~0xff00) | (bitfield(v.m_lvramp, 0, 8) << 8);
						break;
					case 4: // RVOL (Right Volume)
						m_read_latch = (m_read_latch & ~0xffff) | bitfield(v.m_rvol, 0, 16);
						break;
					case 5: // RVRAMP (Right Volume Ramp)
						m_read_latch = (m_read_latch & ~0xff00) | (bitfield(v.m_rvramp, 0, 8) << 8);
						break;
					case 6: // ECOUNT (Envelope Counter)
						m_read_latch = (m_read_latch & ~0x01ff) | bitfield(v.m_ecount, 0, 9);
						break;
					case 7: // K2 (Filter Cutoff Coefficient #2)
						m_read_latch = (m_read_latch & ~0xffff) | bitfield(v.m_filter.m_k2, 0, 16);
						break;
					case 8: // K2RAMP (Filter Cutoff Coefficient #2 Ramp)
						m_read_latch = (m_read_latch & ~0xff01) | (bitfield(v.m_k2ramp.ramp, 0, 8) << 8) | (v.m_k2ramp.slow ? 0x0001 : 0x0000);
						break;
					case 9: // K1 (Filter Cutoff Coefficient #1)
						m_read_latch = (m_read_latch & ~0xffff) | bitfield(v.m_filter.m_k1, 0, 16);
						break;
					case 10: // K1RAMP (Filter Cutoff Coefficient #1 Ramp)
						m_read_latch = (m_read_latch & ~0xff01) | (bitfield(v.m_k1ramp.ramp, 0, 8) << 8) | (v.m_k1ramp.slow ? 0x0001 : 0x0000);
						break;
					case 11: // ACT (Number of voices)
						m_read_latch = (m_read_latch & ~0x1f) | bitfield(m_active, 0, 5);
						break;
					case 12: // MODE (Global Mode)
						m_read_latch = (m_read_latch & ~0x1f) |
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

	return bitfield(m_read_latch, 24, 8);
}

void es5506_core::write(u8 address, u8 data, bool cpu_access = false)
{
	const u8 byte = bitfield(address, 0, 2); // byte select
	const u8 shift = 24 - (byte << 3);
	address = bitfield(address, 2, 4); // 4 bit address for CPU access

	// Update register latch
	m_write_latch = (m_write_latch & ~(0xff << shift)) | (u32(data) << shift);

	if (byte != 3) // Wait until lowest byte is writed
		return;

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
				m_page = bitfield(m_write_latch, 0, 7);
				break;
		}
	}
	else
	{
		if (bitfield(m_page, 6)) // Channel registers are Write only, and for test purposes
		{
			switch (address)
			{
				case 0: // CH0L (Channel 0 Left)
				case 2: // CH1L (Channel 1 Left)
				case 4: // CH2L (Channel 2 Left)
				case 6: // CH3L (Channel 3 Left)
				case 8: // CH4L (Channel 4 Left)
				case 10: // CH5L (Channel 5 Left)
					m_ch[bitfield(address, 1, 3)].m_left = sign_ext<s32>(bitfield(m_write_latch, 0, 23), 23);
					break;
				case 1: // CH0R (Channel 0 Right)
				case 3: // CH1R (Channel 1 Right)
				case 5: // CH2R (Channel 2 Right)
				case 7: // CH3R (Channel 3 Right)
				case 9: // CH4R (Channel 4 Right)
				case 11: // CH5R (Channel 5 Right)
					m_ch[bitfield(address, 1, 3)].m_right = sign_ext<s32>(bitfield(m_write_latch, 0, 23), 23);
					break;
			}
		}
		else
		{
			const u8 voice = bitfield(m_page, 0, 5); // Voice select
			voice_t &v = m_voice[voice];
			if (bitfield(m_page, 5)) // Page 32 - 63
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						v.m_alu.m_cr.stop0 = bitfield(m_write_latch, 0);
						v.m_alu.m_cr.stop1 = bitfield(m_write_latch, 1);
						v.m_alu.m_cr.lei   = bitfield(m_write_latch, 2);
						v.m_alu.m_cr.lpe   = bitfield(m_write_latch, 3);
						v.m_alu.m_cr.ble   = bitfield(m_write_latch, 4);
						v.m_alu.m_cr.irqe  = bitfield(m_write_latch, 5);
						v.m_alu.m_cr.dir   = bitfield(m_write_latch, 6);
						v.m_alu.m_cr.irq   = bitfield(m_write_latch, 7);
						v.m_filter.m_lp.lp = bitfield(m_write_latch, 8, 2);
						v.m_cr.ca          = std::min<u8>(5, bitfield(m_write_latch, 10, 3));
						v.m_cr.cmpd        = bitfield(m_write_latch, 13);
						v.m_cr.bs          = bitfield(m_write_latch, 14, 2);
						break;
					case 1: // START (Loop Start Register)
						v.m_alu.m_start = m_write_latch & 0xfffff800;
						break;
					case 2: // END (Loop End Register)
						v.m_alu.m_end = m_write_latch & 0xffffff80;
						break;
					case 3: // ACCUM (Accumulator Register)
						v.m_alu.m_accum = m_write_latch;
						break;
					case 4: // O4(n-1) (Filter 4 Temp Register)
						v.m_filter.m_o4_1 = sign_ext<s32>(bitfield(m_write_latch, 0, 18), 18);
						break;
					case 5: // O3(n-2) (Filter 3 Temp Register #2)
						v.m_filter.m_o3_2 = sign_ext<s32>(bitfield(m_write_latch, 0, 18), 18);
						break;
					case 6: // O3(n-1) (Filter 3 Temp Register #1)
						v.m_filter.m_o3_1 = sign_ext<s32>(bitfield(m_write_latch, 0, 18), 18);
						break;
					case 7: // O2(n-2) (Filter 2 Temp Register #2)
						v.m_filter.m_o2_2 = sign_ext<s32>(bitfield(m_write_latch, 0, 18), 18);
						break;
					case 8: // O2(n-1) (Filter 2 Temp Register #1)
						v.m_filter.m_o2_1 = sign_ext<s32>(bitfield(m_write_latch, 0, 18), 18);
						break;
					case 9: // O1(n-1) (Filter 1 Temp Register)
						v.m_filter.m_o1_1 = sign_ext<s32>(bitfield(m_write_latch, 0, 18), 18);
						break;
					case 10: // W_ST (Word Clock Start Register)
						m_w_st = bitfield(m_write_latch, 0, 7);
						break;
					case 11: // W_END (Word Clock End Register)
						m_w_end = bitfield(m_write_latch, 0, 7);
						break;
					case 12: // LR_END (Left/Right Clock End Register)
						m_lr_end = bitfield(m_write_latch, 0, 7);
						break;
				}
			}
			else // Page 0 - 31
			{
				switch (address)
				{
					case 0: // CR (Control Register)
						v.m_alu.m_cr.stop0 = bitfield(m_write_latch, 0);
						v.m_alu.m_cr.stop1 = bitfield(m_write_latch, 1);
						v.m_alu.m_cr.lei   = bitfield(m_write_latch, 2);
						v.m_alu.m_cr.lpe   = bitfield(m_write_latch, 3);
						v.m_alu.m_cr.ble   = bitfield(m_write_latch, 4);
						v.m_alu.m_cr.irqe  = bitfield(m_write_latch, 5);
						v.m_alu.m_cr.dir   = bitfield(m_write_latch, 6);
						v.m_alu.m_cr.irq   = bitfield(m_write_latch, 7);
						v.m_filter.m_lp.lp = bitfield(m_write_latch, 8, 2);
						v.m_cr.ca          = std::min<u8>(5, bitfield(m_write_latch, 10, 3));
						v.m_cr.cmpd        = bitfield(m_write_latch, 13);
						v.m_cr.bs          = bitfield(m_write_latch, 14, 2);
						break;
					case 1: // FC (Frequency Control)
						v.m_alu.m_fc = bitfield(m_write_latch, 0, 17);
						break;
					case 2: // LVOL (Left Volume)
						v.m_lvol = bitfield(m_write_latch, 0, 16);
						break;
					case 3: // LVRAMP (Left Volume Ramp)
						v.m_lvramp = bitfield(m_write_latch, 8, 8);
						break;
					case 4: // RVOL (Right Volume)
						v.m_rvol = bitfield(m_write_latch, 0, 16);
						break;
					case 5: // RVRAMP (Right Volume Ramp)
						v.m_rvramp = bitfield(m_write_latch, 8, 8);
						break;
					case 6: // ECOUNT (Envelope Counter)
						v.m_ecount = bitfield(m_write_latch, 0, 9);
						break;
					case 7: // K2 (Filter Cutoff Coefficient #2)
						v.m_filter.m_k2 = bitfield(m_write_latch, 0, 16);
						break;
					case 8: // K2RAMP (Filter Cutoff Coefficient #2 Ramp)
						v.m_k2ramp.slow = bitfield(m_write_latch, 0);
						v.m_k2ramp.ramp = bitfield(m_write_latch, 8, 8);
						break;
					case 9: // K1 (Filter Cutoff Coefficient #1)
						v.m_filter.m_k1 = bitfield(m_write_latch, 0, 16);
						break;
					case 10: // K1RAMP (Filter Cutoff Coefficient #1 Ramp)
						v.m_k1ramp.slow = bitfield(m_write_latch, 0);
						v.m_k1ramp.ramp = bitfield(m_write_latch, 8, 8);
						break;
					case 11: // ACT (Number of voices)
						m_active = std::min<u8>(4, bitfield(m_write_latch, 0, 5));
						break;
					case 12: // MODE (Global Mode)
						m_mode.lrclk_en = bitfield(m_write_latch, 0);
						m_mode.wclk_en  = bitfield(m_write_latch, 1);
						m_mode.bclk_en  = bitfield(m_write_latch, 2);
						m_mode.master   = bitfield(m_write_latch, 3);
						m_mode.dual     = bitfield(m_write_latch, 4);
						break;
				}
			}
		}
	}

	// Reset latch
	m_write_latch = 0;
}
