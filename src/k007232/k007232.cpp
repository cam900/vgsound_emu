/*
	License: Zlib
	see https://gitlab.com/cam900/vgsound_emu/-/blob/main/LICENSE for more details

	Copyright holder(s): cam900
	Konami K007232 core, Based on Reverse Engineering based info at
   https://github.com/furrtek/VGChips/tree/master/Konami/007232

	It's Konami's one of custom PCM sound chip, Used at their arcade hardware at
   mid-80s to early-90s.

	It has 2 channel of PCM, these are has its own output pins...just 7 LSB of
   currently fetched data.

	PCM Sample format is unique, 1 MSB is end marker and 7 LSB is actually
   output. (unsigned format)

	The chip itself is DACless, so Sound output and mixing control needs
   external logics and sound DAC.

	Register layout (Write only)

	Address Bits      Description
			7654 3210

	0       xxxx xxxx Channel 0 Pitch bit 0-7*
	1       --x- ---- Channel 0 4 bit Frequency mode*
			---x ---- Channel 0 8 bit Frequency mode*
			---- xxxx Channel 0 Pitch bit 8-11*

	2       xxxx xxxx Channel 0 Start address bit 0-7
	3       xxxx xxxx Channel 0 Start address bit 8-15
	4       ---- ---x Channel 0 Start address bit 16

	5                 Channel 0 Key on trigger (R/W)

	6       xxxx xxxx Channel 1 Pitch bit 0-7*
	7       --x- ---- Channel 1 4 bit Frequency mode*
			---x ---- Channel 1 8 bit Frequency mode*
			---- xxxx Channel 1 Pitch bit 8-11*

	8       xxxx xxxx Channel 1 Start address bit 0-7
	9       xxxx xxxx Channel 1 Start address bit 8-15
	a       ---- ---x Channel 1 Start address bit 16

	b                 Channel 1 Key on trigger (R/W)

	c       xxxx xxxx External port write (w/SLEV pin, Usually for volume
   control)

	d       ---- --x- Channel 1 Loop enable
			---- ---x Channel 0 Loop enable

	* Frequency calculation: (Guesswork in 8/4 bit Frequency mode)

		if 8 bit Frequency mode then
			Frequency: Input clock / 4 * (256 - (Pitch bit 0 - 7))
		else if 4 bit Frequency mode then
			Frequency: Input clock / 4 * (16 - (Pitch bit 8 - 11))
		else
			Frequency: Input clock / 4 * (4096 - (Pitch bit 0 - 11))
*/

#include "k007232.hpp"

void k007232_core::tick()
{
	for (int i = 0; i < 2; i++)
	{
		m_voice[i].tick(i);
	}
}

void k007232_core::voice_t::tick(u8 ne)
{
	if (m_busy)
	{
		const bool is4bit = bitfield(m_pitch, 13);	// 4 bit frequency divider flag
		const bool is8bit = bitfield(m_pitch, 12);	// 8 bit frequency divider flag

		// update counter
		if (is4bit)
		{
			m_counter = (m_counter & ~0x0ff) | (bitfield(bitfield(m_counter, 0, 8) + 1, 0, 8) << 0);
			m_counter = (m_counter & ~0xf00) | (bitfield(bitfield(m_counter, 8, 4) + 1, 0, 4) << 8);
		}
		else
		{
			m_counter++;
		}

		// handle counter carry
		bool carry =
		  is8bit ? (bitfield(m_counter, 0, 8) == 0)
				 : (is4bit ? (bitfield(m_counter, 8, 4) == 0) : (bitfield(m_counter, 0, 12) == 0));
		if (carry)
		{
			m_counter = bitfield(m_pitch, 0, 12);
			if (is4bit)	 // 4 bit frequency has different behavior for address
			{
				m_addr = (m_addr & ~0x0000f) | (bitfield(bitfield(m_addr, 0, 4) + 1, 0, 4) << 0);
				m_addr = (m_addr & ~0x000f0) | (bitfield(bitfield(m_addr, 4, 4) + 1, 0, 4) << 4);
				m_addr = (m_addr & ~0x00f00) | (bitfield(bitfield(m_addr, 8, 4) + 1, 0, 4) << 8);
				m_addr = (m_addr & ~0x1f000) | (bitfield(bitfield(m_addr, 12, 5) + 1, 0, 5) << 12);
			}
			else
			{
				m_addr = bitfield(m_addr + 1, 0, 17);
			}
		}

		m_data = m_host.m_intf.read_sample(ne, bitfield(m_addr, 0, 17));  // fetch ROM
		if (bitfield(m_data, 7))										  // check end marker
		{
			if (m_loop)
			{
				m_addr = m_start;
			}
			else
			{
				m_busy = false;
			}
		}

		m_out = s8(m_data) - 0x40;	// send to output (ASD/BSD) pin
	}
	else
	{
		m_out = 0;
	}
}

void k007232_core::write(u8 address, u8 data)
{
	address &= 0xf;	 // 4 bit for CPU write

	switch (address)
	{
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:  // voice 0
		case 0x6:
		case 0x7:
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:  // voice 1
			m_voice[(address / 6) & 1].write(address % 6, data);
			break;
		case 0xc:  // external register with SLEV pin
			m_intf.write_slev(data);
			break;
		case 0xd:  // loop flag
			m_voice[0].set_loop(bitfield(data, 0));
			m_voice[1].set_loop(bitfield(data, 1));
			break;
		default: break;
	}

	m_reg[address] = data;
}

// write registers on each voices
void k007232_core::voice_t::write(u8 address, u8 data)
{
	switch (address)
	{
		case 0:	 // pitch LSB
			m_pitch = (m_pitch & ~0x00ff) | data;
			break;
		case 1:	 // pitch MSB, divider
			m_pitch = (m_pitch & ~0x3f00) | (u16(bitfield(data, 0, 6)) << 8);
			break;
		case 2:	 // start address bit 0-7
			m_start = (m_start & ~0x000ff) | data;
			break;
		case 3:	 // start address bit 8-15
			m_start = (m_start & ~0x0ff00) | (u32(data) << 8);
			break;
		case 4:	 // start address bit 16
			m_start = (m_start & ~0x10000) | (u32(bitfield(data, 0)) << 16);
			break;
		case 5:	 // keyon trigger
			keyon();
			break;
	}
}

// key on trigger (write OR read 0x05/0x11 register)
void k007232_core::voice_t::keyon()
{
	m_busy	  = true;
	m_counter = bitfield(m_pitch, 0, 12);
	m_addr	  = m_start;
}

// reset chip
void k007232_core::reset()
{
	for (auto &elem : m_voice)
	{
		elem.reset();
	}

	m_intf.write_slev(0);

	std::fill(m_reg.begin(), m_reg.end(), 0);
}

// reset voice
void k007232_core::voice_t::reset()
{
	m_busy	  = false;
	m_loop	  = false;
	m_pitch	  = 0;
	m_start	  = 0;
	m_counter = 0;
	m_addr	  = 0;
	m_data	  = 0;
	m_out	  = 0;
}
