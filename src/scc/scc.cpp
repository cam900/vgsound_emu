/*
	License: Zlib
	see https://gitlab.com/cam900/vgsound_emu/-/blob/main/LICENSE for more details

	Copyright holder(s): cam900
	Contributor(s): Natt Akuma, James Alan Nguyen, Laurens Holst
	Konami SCC emulation core

	Konami SCC means "Sound Creative Chip", it's actually MSX MegaROM/RAM Mapper
   with 5 channel Wavetable sound generator.

	It was first appeared at 1987, F-1 Spirit and Nemesis 2/Gradius 2 for MSX.
   then several MSX cartridges used that until 1990, Metal Gear 2: Solid Snake.
	Even after MSX is discontinued, it was still used at some low-end arcade and
   amusement hardwares. and some Third-party MSX utilities still support this
   due to its market shares.

	There's 2 SCC types:

	K051649 (or simply known as SCC)
	This chip is used for MSX MegaROM Mapper, some arcade machines.
	Channel 4 and 5 must be share waveform, other channels has its own
   waveforms.

	K052539 (also known as SCC+)
	This chip is used for MSX MegaRAM Mapper (Konami Sound Cartridges for
   Snatcher/SD Snatcher). All channels can be has its own waveforms, and also
   has backward compatibility mode with K051649.

	Based on:
		https://www.msx.org/wiki/MegaROM_Mappers
		https://www.msx.org/wiki/Konami_051649
		https://www.msx.org/wiki/Konami_052539
		http://bifi.msxnet.org/msxnet/tech/scc
		http://bifi.msxnet.org/msxnet/tech/soundcartridge

	K051649 Register Layout

	--------------------------------------------------------------------

	4000-bfff MegaROM Mapper

	--------------------------------------------------------------------

	Address   Bit       R/W Description
			  7654 3210

	4000-5fff xxxx xxxx R   Bank page 0
	c000-dfff mirror of 4000-5fff

	6000-7fff xxxx xxxx R   Bank page 1
	e000-ffff mirror of 6000-7fff

	8000-9fff xxxx xxxx R   Bank page 2
	0000-1fff mirror of 8000-9fff

	a000-bfff xxxx xxxx R   Bank page 3
	2000-3fff mirror of a000-bfff

	--------------------------------------------------------------------

	5000-57ff, 7000-77ff, 9000-97ff, b000-b7ff Bank select

	--------------------------------------------------------------------

	Address   Bit       R/W Description
			  7654 3210

	5000      --xx xxxx   W Bank select, Page 0
	5001-57ff Mirror of 5000

	7000      --xx xxxx   W Bank select, Page 1
	7001-77ff Mirror of 7000

	9000      --xx xxxx   W Bank select, Page 2
			  --11 1111   W SCC Enable
	9001-97ff Mirror of 9000

	b000      --xx xxxx   W Bank select, Page 3
	b001-b7ff Mirror of b000

	--------------------------------------------------------------------

	9800-9fff SCC register

	--------------------------------------------------------------------

	9800-987f Waveform

	Address   Bit       R/W Description
			  7654 3210

	9800-981f xxxx xxxx R/W Channel 0 Waveform (32 byte, 8 bit signed)
	9820-983f xxxx xxxx R/W Channel 1 ""
	9840-985f xxxx xxxx R/W Channel 2 ""
	9860-987f xxxx xxxx R/W Channel 3/4 ""

	9880-9889 Pitch

	9880      xxxx xxxx   W Channel 0 Pitch LSB
	9881      ---- xxxx   W Channel 0 Pitch MSB
	9882      xxxx xxxx   W Channel 1 Pitch LSB
	9883      ---- xxxx   W Channel 1 Pitch MSB
	9884      xxxx xxxx   W Channel 2 Pitch LSB
	9885      ---- xxxx   W Channel 2 Pitch MSB
	9886      xxxx xxxx   W Channel 3 Pitch LSB
	9887      ---- xxxx   W Channel 3 Pitch MSB
	9888      xxxx xxxx   W Channel 4 Pitch LSB
	9889      ---- xxxx   W Channel 4 Pitch MSB

	9888-988e Volume

	988a      ---- xxxx   W Channel 0 Volume
	988b      ---- xxxx   W Channel 1 Volume
	988c      ---- xxxx   W Channel 2 Volume
	988d      ---- xxxx   W Channel 3 Volume
	988e      ---- xxxx   W Channel 4 Volume

	988f      ---x ----   W Channel 4 Output enable/disable flag
			  ---- x---   W Channel 3 Output enable/disable flag
			  ---- -x--   W Channel 2 Output enable/disable flag
			  ---- --x-   W Channel 1 Output enable/disable flag
			  ---- ---x   W Channel 0 Output enable/disable flag

	9890-989f Mirror of 9880-988f

	98a0-98bf xxxx xxxx R   Channel 4 Waveform

	98e0      x--- ----   W Waveform rotate flag for channel 4
			  -x-- ----   W Waveform rotate flag for all channels
			  --x- ----   W Reset waveform position after pitch writes
			  ---- --x-   W 8 bit frequency
			  ---- --0x   W 4 bit frequency

	98e1-98ff Mirror of 98e0

	9900-9fff Mirror of 9800-98ff

	--------------------------------------------------------------------

	K052539 Register Layout

	--------------------------------------------------------------------

	4000-bfff MegaRAM Mapper

	--------------------------------------------------------------------

	Address   Bit       R/W Description
			  7654 3210

	4000-5fff xxxx xxxx R/W Bank page 0
	c000-dfff xxxx xxxx R/W ""

	6000-7fff xxxx xxxx R/W Bank page 1
	e000-ffff xxxx xxxx R/W ""

	8000-9fff xxxx xxxx R/W Bank page 2
	0000-1fff xxxx xxxx R/W ""

	a000-bfff xxxx xxxx R/W Bank page 3
	2000-3fff xxxx xxxx R/W ""

	--------------------------------------------------------------------

	5000-57ff, 7000-77ff, 9000-97ff, b000-b7ff Bank select

	--------------------------------------------------------------------

	Address   Bit       R/W Description
			  7654 3210

	5000      xxxx xxxx   W Bank select, Page 0
	5001-57ff Mirror of 5000

	7000      xxxx xxxx   W Bank select, Page 1
	7001-77ff Mirror of 7000

	9000      xxxx xxxx   W Bank select, Page 2
			  --11 1111   W SCC Enable (SCC Compatible mode)
	9001-97ff Mirror of 9000

	b000      xxxx xxxx   W Bank select, Page 3
			  1--- ----   W SCC+ Enable (SCC+ mode)
	b001-b7ff Mirror of b000

	--------------------------------------------------------------------

	bffe-bfff Mapper configuration

	--------------------------------------------------------------------

	Address   Bit       R/W Description
			  7654 3210

	bffe      --x- ----   W SCC operation mode
			  --0- ----   W SCC Compatible mode
			  --1- ----   W SCC+ mode
			  ---x ----   W RAM write/Bank select toggle for all Bank pages
						---0 ----   W Bank select enable
						---1 ----   W RAM write enable
						---0 -x--   W RAM write/Bank select toggle for Bank page
   2
						---0 --x-   W RAM write/Bank select toggle for Bank page
   1
						---0 ---x   W RAM write/Bank select toggle for Bank page
   0 bfff Mirror of bffe

	--------------------------------------------------------------------

	9800-9fff SCC Compatible mode register

	--------------------------------------------------------------------

	9800-987f Waveform

	Address   Bit       R/W Description
			  7654 3210

	9800-981f xxxx xxxx R/W Channel 0 Waveform (32 byte, 8 bit signed)
	9820-983f xxxx xxxx R/W Channel 1 ""
	9840-985f xxxx xxxx R/W Channel 2 ""
	9860-987f xxxx xxxx R/W Channel 3/4 ""

	9880-9889 Pitch

	9880      xxxx xxxx   W Channel 0 Pitch LSB
	9881      ---- xxxx   W Channel 0 Pitch MSB
	9882      xxxx xxxx   W Channel 1 Pitch LSB
	9883      ---- xxxx   W Channel 1 Pitch MSB
	9884      xxxx xxxx   W Channel 2 Pitch LSB
	9885      ---- xxxx   W Channel 2 Pitch MSB
	9886      xxxx xxxx   W Channel 3 Pitch LSB
	9887      ---- xxxx   W Channel 3 Pitch MSB
	9888      xxxx xxxx   W Channel 4 Pitch LSB
	9889      ---- xxxx   W Channel 4 Pitch MSB

	9888-988e Volume

	988a      ---- xxxx   W Channel 0 Volume
	988b      ---- xxxx   W Channel 1 Volume
	988c      ---- xxxx   W Channel 2 Volume
	988d      ---- xxxx   W Channel 3 Volume
	988e      ---- xxxx   W Channel 4 Volume

	988f      ---x ----   W Channel 4 Output enable/disable flag
			  ---- x---   W Channel 3 Output enable/disable flag
			  ---- -x--   W Channel 2 Output enable/disable flag
			  ---- --x-   W Channel 1 Output enable/disable flag
			  ---- ---x   W Channel 0 Output enable/disable flag

	9890-989f Mirror of 9880-988f

	98a0-98bf xxxx xxxx R   Channel 4 Waveform

	98c0      -x-- ----   W Waveform rotate flag for all channels
			  --x- ----   W Reset waveform position after pitch writes
			  ---- --x-   W 8 bit frequency
			  ---- --0x   W 4 bit frequency

	98c1-98df Mirror of 98c0

	9900-9fff Mirror of 9800-98ff

	--------------------------------------------------------------------

	b800-bfff SCC+ mode register

	--------------------------------------------------------------------

	b800-b89f Waveform

	Address   Bit       R/W Description
			  7654 3210

	b800-b81f xxxx xxxx R/W Channel 0 Waveform (32 byte, 8 bit signed)
	b820-b83f xxxx xxxx R/W Channel 1 ""
	b840-b85f xxxx xxxx R/W Channel 2 ""
	b860-b87f xxxx xxxx R/W Channel 3 ""
	b880-b89f xxxx xxxx R/W Channel 3 ""

	b8a0-b8a9 Pitch

	b8a0      xxxx xxxx   W Channel 0 Pitch LSB
	b8a1      ---- xxxx   W Channel 0 Pitch MSB
	b8a2      xxxx xxxx   W Channel 1 Pitch LSB
	b8a3      ---- xxxx   W Channel 1 Pitch MSB
	b8a4      xxxx xxxx   W Channel 2 Pitch LSB
	b8a5      ---- xxxx   W Channel 2 Pitch MSB
	b8a6      xxxx xxxx   W Channel 3 Pitch LSB
	b8a7      ---- xxxx   W Channel 3 Pitch MSB
	b8a8      xxxx xxxx   W Channel 4 Pitch LSB
	b8a9      ---- xxxx   W Channel 4 Pitch MSB

	b8a8-b8ae Volume

	b8aa      ---- xxxx   W Channel 0 Volume
	b8ab      ---- xxxx   W Channel 1 Volume
	b8ac      ---- xxxx   W Channel 2 Volume
	b8ad      ---- xxxx   W Channel 3 Volume
	b8ae      ---- xxxx   W Channel 4 Volume

	b8af      ---x ----   W Channel 4 Output enable/disable flag
			  ---- x---   W Channel 3 Output enable/disable flag
			  ---- -x--   W Channel 2 Output enable/disable flag
			  ---- --x-   W Channel 1 Output enable/disable flag
			  ---- ---x   W Channel 0 Output enable/disable flag

	b8b0-b8bf Mirror of b8a0-b8af

	b8c0      -x-- ----   W Waveform rotate flag for all channels
			  --x- ----   W Reset waveform position after pitch writes
			  ---- --x-   W 8 bit frequency
			  ---- --0x   W 4 bit frequency

	b8c1-b8df Mirror of b8c0

	b900-bfff Mirror of b800-b8ff

	--------------------------------------------------------------------

	SCC Frequency calculation:
	if 8 bit frequency then
		Frequency = Input clock / ((bit 0 to 7 of Pitch input) + 1)
	else if 4 bit frequency then
		Frequency = Input clock / ((bit 8 to 11 of Pitch input) + 1)
	else
		Frequency = Input clock / (Pitch input + 1)

*/

#include "scc.hpp"

// shared SCC features
void scc_core::tick()
{
	m_out = 0;
	for (auto &elem : m_voice)
	{
		elem.tick();
		m_out += elem.out();
	}
}

void scc_core::voice_t::tick()
{
	if (m_pitch >= 9)  // or voice is halted
	{
		// update counter - Post decrement
		const u16 temp = m_counter;
		if (m_host.m_test.freq_4bit())	// 4 bit frequency mode
		{
			m_counter = (m_counter & ~0x0ff) | (bitfield(bitfield(m_counter, 0, 8) - 1, 0, 8) << 0);
			m_counter = (m_counter & ~0xf00) | (bitfield(bitfield(m_counter, 8, 4) - 1, 0, 4) << 8);
		}
		else
		{
			m_counter = bitfield(m_counter - 1, 0, 12);
		}

		// handle counter carry
		const bool carry = m_host.m_test.freq_8bit()
						   ? (bitfield(temp, 0, 8) == 0)
						   : (m_host.m_test.freq_4bit() ? (bitfield(temp, 8, 4) == 0)
														: (bitfield(temp, 0, 12) == 0));
		if (carry)
		{
			m_addr	  = bitfield(m_addr + 1, 0, 5);
			m_counter = m_pitch;
		}
	}
	// get output
	if (m_enable)
	{
		m_out = (m_wave[m_addr] * m_volume) >> 4;  // scale to 11 bit digital output
	}
	else
	{
		m_out = 0;
	}
}

void scc_core::reset()
{
	for (auto &elem : m_voice)
	{
		elem.reset();
	}

	m_test.reset();
	m_out = 0;
	std::fill(m_reg.begin(), m_reg.end(), 0);
}

void scc_core::voice_t::reset()
{
	std::fill(m_wave.begin(), m_wave.end(), 0);
	m_enable  = false;
	m_pitch	  = 0;
	m_volume  = 0;
	m_addr	  = 0;
	m_counter = 0;
	m_out	  = 0;
}

// SCC accessors
u8 scc_core::wave_r(bool is_sccplus, u8 address)
{
	u8 ret		   = 0xff;
	const u8 voice = bitfield(address, 5, 3);
	if (voice > 4)
	{
		return ret;
	}

	u8 wave_addr = bitfield(address, 0, 5);

	if (m_test.rotate())
	{  // rotate flag
		wave_addr = bitfield(wave_addr + m_voice[voice].addr(), 0, 5);
	}

	if (!is_sccplus)
	{
		if (voice == 3)	 // rotate voice 3~4 flag
		{
			if (m_test.rotate4() || m_test.rotate())
			{  // rotate flag
				wave_addr =
				  bitfield(bitfield(address, 0, 5) + m_voice[3 + m_test.rotate()].addr(), 0, 5);
			}
		}
	}
	ret = m_voice[voice].wave(wave_addr);

	return ret;
}

void scc_core::wave_w(bool is_sccplus, u8 address, u8 data)
{
	if (m_test.rotate())
	{  // write protected
		return;
	}

	const u8 voice = bitfield(address, 5, 3);
	if (voice > 4)
	{
		return;
	}

	const u8 wave_addr = bitfield(address, 0, 5);

	if (!is_sccplus)
	{
		if (((voice >= 3) && m_test.rotate4()) || (voice >= 4))
		{  // Ignore if write protected, or voice 4
			return;
		}
		if (voice >= 3)	 // voice 3, 4 shares waveform
		{
			m_voice[3].set_wave(wave_addr, data);
			m_voice[4].set_wave(wave_addr, data);
		}
		else
		{
			m_voice[voice].set_wave(wave_addr, data);
		}
	}
	else
	{
		m_voice[voice].set_wave(wave_addr, data);
	}
}

void scc_core::freq_vol_enable_w(u8 address, u8 data)
{
	// *0-*f Pitch, Volume, Enable
	address				= bitfield(address, 0, 4);	// mask address to 4 bit
	const u8 voice_freq = bitfield(address, 1, 3);
	switch (address)
	{
		case 0x0:  // 0x*0 Voice 0 Pitch LSB
		case 0x2:  // 0x*2 Voice 1 Pitch LSB
		case 0x4:  // 0x*4 Voice 2 Pitch LSB
		case 0x6:  // 0x*6 Voice 3 Pitch LSB
		case 0x8:  // 0x*8 Voice 4 Pitch LSB
			if (m_test.resetpos())
			{  // Reset address
				m_voice[voice_freq].reset_addr();
			}
			m_voice[voice_freq].set_pitch(data, 0x0ff);
			break;
		case 0x1:  // 0x*1 Voice 0 Pitch MSB
		case 0x3:  // 0x*3 Voice 1 Pitch MSB
		case 0x5:  // 0x*5 Voice 2 Pitch MSB
		case 0x7:  // 0x*7 Voice 3 Pitch MSB
		case 0x9:  // 0x*9 Voice 4 Pitch MSB
			if (m_test.resetpos())
			{  // Reset address
				m_voice[voice_freq].reset_addr();
			}
			m_voice[voice_freq].set_pitch(u16(bitfield(data, 0, 4)) << 8, 0xf00);
			break;
		case 0xa:  // 0x*a Voice 0 Volume
		case 0xb:  // 0x*b Voice 1 Volume
		case 0xc:  // 0x*c Voice 2 Volume
		case 0xd:  // 0x*d Voice 3 Volume
		case 0xe:  // 0x*e Voice 4 Volume
			m_voice[address - 0xa].set_volume(bitfield(data, 0, 4));
			break;
		case 0xf:  // 0x*f Enable/Disable flag
			m_voice[0].set_enable(bitfield(data, 0));
			m_voice[1].set_enable(bitfield(data, 1));
			m_voice[2].set_enable(bitfield(data, 2));
			m_voice[3].set_enable(bitfield(data, 3));
			m_voice[4].set_enable(bitfield(data, 4));
			break;
	}
}

void k051649_scc_core::scc_w(bool is_sccplus, u8 address, u8 data)
{
	const u8 voice = bitfield(address, 5, 3);
	switch (voice)
	{
		case 0b000:	 // 0x00-0x1f Voice 0 Waveform
		case 0b001:	 // 0x20-0x3f Voice 1 Waveform
		case 0b010:	 // 0x40-0x5f Voice 2 Waveform
		case 0b011:	 // 0x60-0x7f Voice 3/4 Waveform
			wave_w(false, address, data);
			break;
		case 0b100:	 // 0x80-0x9f Pitch, Volume, Enable
			freq_vol_enable_w(address, data);
			break;
		case 0b111:	 // 0xe0-0xff Test register
			m_test.set_freq_4bit(bitfield(data, 0));
			m_test.set_freq_8bit(bitfield(data, 1));
			m_test.set_resetpos(bitfield(data, 5));
			m_test.set_rotate(bitfield(data, 6));
			m_test.set_rotate4(bitfield(data, 7));
			break;
	}
	m_reg[address] = data;
}

void k052539_scc_core::scc_w(bool is_sccplus, u8 address, u8 data)
{
	const u8 voice = bitfield(address, 5, 3);
	if (is_sccplus)
	{
		switch (voice)
		{
			case 0b000:	 // 0x00-0x1f Voice 0 Waveform
			case 0b001:	 // 0x20-0x3f Voice 1 Waveform
			case 0b010:	 // 0x40-0x5f Voice 2 Waveform
			case 0b011:	 // 0x60-0x7f Voice 3 Waveform
			case 0b100:	 // 0x80-0x9f Voice 4 Waveform
				wave_w(true, address, data);
				break;
			case 0b101:	 // 0xa0-0xbf Pitch, Volume, Enable
				freq_vol_enable_w(address, data);
				break;
			case 0b110:	 // 0xc0-0xdf Test register
				m_test.set_freq_4bit(bitfield(data, 0));
				m_test.set_freq_8bit(bitfield(data, 1));
				m_test.set_resetpos(bitfield(data, 5));
				m_test.set_rotate(bitfield(data, 6));
				break;
			default: break;
		}
	}
	else
	{
		switch (voice)
		{
			case 0b000:	 // 0x00-0x1f Voice 0 Waveform
			case 0b001:	 // 0x20-0x3f Voice 1 Waveform
			case 0b010:	 // 0x40-0x5f Voice 2 Waveform
			case 0b011:	 // 0x60-0x7f Voice 3/4 Waveform
				wave_w(false, address, data);
				break;
			case 0b100:	 // 0x80-0x9f Pitch, Volume, Enable
				freq_vol_enable_w(address, data);
				break;
			case 0b110:	 // 0xc0-0xdf Test register
				m_test.set_freq_4bit(bitfield(data, 0));
				m_test.set_freq_8bit(bitfield(data, 1));
				m_test.set_resetpos(bitfield(data, 5));
				m_test.set_rotate(bitfield(data, 6));
				break;
			default: break;
		}
	}
	m_reg[address] = data;
}

u8 k051649_scc_core::scc_r(bool is_sccplus, u8 address)
{
	const u8 voice = bitfield(address, 5, 3);
	const u8 wave  = bitfield(address, 0, 5);
	u8 ret		   = 0xff;
	switch (voice)
	{
		case 0b000:	 // 0x00-0x1f Voice 0 Waveform
		case 0b001:	 // 0x20-0x3f Voice 1 Waveform
		case 0b010:	 // 0x40-0x5f Voice 2 Waveform
		case 0b011:	 // 0x60-0x7f Voice 3 Waveform
		case 0b101:	 // 0xa0-0xbf Voice 4 Waveform
			ret = wave_r(false, (std::min<u8>(4, voice) << 5) | wave);
			break;
	}
	return ret;
}

u8 k052539_scc_core::scc_r(bool is_sccplus, u8 address)
{
	const u8 voice = bitfield(address, 5, 3);
	const u8 wave  = bitfield(address, 0, 5);
	u8 ret		   = 0xff;
	if (is_sccplus)
	{
		switch (voice)
		{
			case 0b000:	 // 0x00-0x1f Voice 0 Waveform
			case 0b001:	 // 0x20-0x3f Voice 1 Waveform
			case 0b010:	 // 0x40-0x5f Voice 2 Waveform
			case 0b011:	 // 0x60-0x7f Voice 3 Waveform
			case 0b100:	 // 0x80-0x9f Voice 4 Waveform
				ret = wave_r(true, address);
				break;
		}
	}
	else
	{
		switch (voice)
		{
			case 0b000:	 // 0x00-0x1f Voice 0 Waveform
			case 0b001:	 // 0x20-0x3f Voice 1 Waveform
			case 0b010:	 // 0x40-0x5f Voice 2 Waveform
			case 0b011:	 // 0x60-0x7f Voice 3 Waveform
			case 0b101:	 // 0xa0-0xbf Voice 4 Waveform
				ret = wave_r(false, (std::min<u8>(4, voice) << 5) | wave);
				break;
		}
	}
	return ret;
}

// Mapper
void k051649_core::reset()
{
	k051649_scc_core::reset();
	m_mapper.reset();
	m_scc_enable = false;
}

void k052539_core::reset()
{
	k052539_scc_core::reset();
	m_mapper.reset();
	m_scc_enable = false;
	m_is_sccplus = false;
}

void k051649_core::k051649_mapper_t::reset()
{
	m_bank[0] = 0;
	m_bank[1] = 1;
	m_bank[2] = 2;
	m_bank[3] = 3;
}

void k052539_core::k052539_mapper_t::reset()
{
	m_bank[0] = 0;
	m_bank[1] = 1;
	m_bank[2] = 2;
	m_bank[3] = 3;
	std::fill(m_ram_enable.begin(), m_ram_enable.end(), false);
}

// Mapper accessors
u8 k051649_core::read(u16 address)
{
	if ((bitfield(address, 11, 5) == 0b10011) && m_scc_enable)
	{
		return scc_r(false, u8(address));
	}

	return m_intf.read_byte((u32(m_mapper.bank(bitfield(address, 13, 2) ^ 2)) << 13) |
							bitfield(address, 0, 13));
}

u8 k052539_core::read(u16 address)
{
	if ((bitfield(address, 11, 5) == 0b10011) && m_scc_enable && (!m_is_sccplus))
	{
		return scc_r(false, u8(address));
	}

	if ((bitfield(address, 11, 5) == 0b10111) && m_scc_enable && m_is_sccplus)
	{
		return scc_r(true, u8(address));
	}

	return m_intf.read_byte((u32(m_mapper.bank(bitfield(address, 13, 2) ^ 2)) << 13) |
							bitfield(address, 0, 13));
}

void k051649_core::write(u16 address, u8 data)
{
	const u16 bank = bitfield(address, 13, 2) ^ 2;
	switch (bitfield(address, 11, 5))
	{
		case 0b01010:  // 0x5000-0x57ff Bank 0
		case 0b01110:  // 0x7000-0x77ff Bank 1
		case 0b10010:  // 0x9000-0x97ff Bank 2
		case 0b10110:  // 0xb000-0xb7ff Bank 3
			m_mapper.set_bank(bank, data);
			m_scc_enable = (bitfield(m_mapper.bank(2), 0, 6) == 0x3f);
			break;
		case 0b10011:  // 0x9800-9fff SCC
			if (m_scc_enable)
			{
				scc_w(false, u8(address), data);
			}
			break;
	}
}

void k052539_core::write(u16 address, u8 data)
{
	u8 prev				  = 0;
	bool update			  = false;
	const u16 bank		  = bitfield(address, 13, 2) ^ 2;
	const bool ram_enable = m_mapper.ram_enable(bank);
	if (ram_enable)
	{
		m_intf.write_byte((u32(m_mapper.bank(bank)) << 13) | bitfield(address, 0, 13), data);
	}
	switch (bitfield(address, 11, 5))
	{
		case 0b01010:  // 0x5000-0x57ff Bank 0
		case 0b01110:  // 0x7000-0x77ff Bank 1
		case 0b10010:  // 0x9000-0x97ff Bank 2
		case 0b10110:  // 0xb000-0xb7ff Bank 3
			if (!ram_enable)
			{
				prev = m_mapper.bank(bank);
				m_mapper.set_bank(bank, data);
				update = prev ^ m_mapper.bank(bank);
			}
			break;
		case 0b10011:  // 0x9800-0x9fff SCC
			if ((!ram_enable) && m_scc_enable && (!m_is_sccplus))
			{
				scc_w(false, u8(address), data);
			}
			break;
		case 0b10111:  // 0xb800-0xbfff SCC+, Mapper configuration
			if (bitfield(address, 1, 10) == 0x3ff)
			{
				m_mapper.set_ram_enable(0, bitfield(data, 4) || bitfield(data, 0));
				m_mapper.set_ram_enable(1, bitfield(data, 4) || bitfield(data, 1));
				m_mapper.set_ram_enable(2, bitfield(data, 4) || bitfield(data, 2));
				m_mapper.set_ram_enable(3, bitfield(data, 4));
				prev		 = m_is_sccplus;
				m_is_sccplus = bitfield(data, 5);
				update		 = prev ^ m_is_sccplus;
			}
			else if ((!ram_enable) && m_scc_enable && m_is_sccplus)
			{
				scc_w(true, u8(address), data);
			}
			break;
	}
	if (update)
	{
		m_scc_enable =
		  m_is_sccplus ? bitfield(m_mapper.bank(3), 7) : (bitfield(m_mapper.bank(2), 0, 6) == 0x3f);
	}
}
