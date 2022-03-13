/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Konami K053260 core

	It's one of Konami's custom PCM playback chip with CPU to CPU communication feature, and built in timer.
	It's architecture is successed from K007232, but it features various enhancements:
	it's expanded to 4 channels, Supports more memory space, 4 bit ADPCM, Built in volume and stereo panning support,
	and Dual chip configurations.

	Register layout (Read/Write)

	Address Bits      R/W Description
	        7654 3210

	00...03 Communication Registers

	00      xxxx xxxx R   Answer from host CPU LSB
	01      xxxx xxxx R   Answer from host CPU MSB

	02      xxxx xxxx   W Reply to host CPU LSB
	03      xxxx xxxx   W Reply to host CPU MSB

	08...0f Voice 0 Register

	08      xxxx xxxx   W Pitch bit 0-7*
	09      ---- xxxx   W Pitch bit 8-11*

	0a      xxxx xxxx   W Source length bit 0-7 (byte wide)
	0b      xxxx xxxx   W Source length bit 8-15 (byte wide)

	0c      xxxx xxxx   W Start address/ROM readback base bit 0-7
	0d      xxxx xxxx   W Start address/ROM readback base bit 8-15
	0e      ---x xxxx   W Start address/ROM readback base bit 16-20

	0f      -xxx xxxx   W Volume

	10...17 Voice 1 Register
	18...1f Voice 2 Register
	20...27 Voice 3 Register

	28      ---- x---   W Voice 3 Key on/off trigger
	        ---- -x--   W Voice 2 Key on/off trigger
	        ---- --x-   W Voice 1 Key on/off trigger
	        ---- ---x   W Voice 0 Key on/off trigger

	29      ---- x--- R   Voice 3 busy
	        ---- -x-- R   Voice 2 busy
	        ---- --x- R   Voice 1 busy
	        ---- ---x R   Voice 0 busy

	2a      x--- ----   W Voice 3 source format
	        0--- ----     8 bit signed PCM
	        1--- ----     4 bit ADPCM
	        -x-- ----   W Voice 2 source format
	        --x- ----   W Voice 1 source format
	        ---x ----   W Voice 0 source format

	        ---- x---   W Voice 3 Loop enable
	        ---- -x--   W Voice 2 Loop enable
	        ---- --x-   W Voice 1 Loop enable
	        ---- ---x   W Voice 0 Loop enable

	2c      --xx x---   W Voice 1 Pan angle in degrees**
	        --00 0---     Mute
	        --00 1---     0 degrees
	        --01 0---     24 degrees
	        --01 1---     35 degrees
	        --10 0---     45 degrees
	        --10 1---     55 degrees
	        --11 0---     66 degrees
	        --11 1---     90 degrees
	        ---- -xxx   W Voice 0 Pan angle in degrees**

	2d      --xx x---   W Voice 3 Pan angle in degrees**
	        ---- -xxx   W Voice 2 Pan angle in degrees**

	2e      xxxx xxxx R   ROM readback (use Voice 0 register)

	2f      ---- x---   W Dual chip configuration related?
	        ---- -x--   W YM3012 input related?
			---- --x-   W Sound enable
			---- ---x   W ROM readbank enable

	* Frequency calculation:
	Frequency: Input clock / (4096 - Pitch)

	** Actually fomula unknown, Use floating point type until explained that.
*/

#include "k053260.hpp"

void k053260_core::tick()
{
	for (int i = 0; i < 2; i++)
		m_voice[i].tick(i);
}

void k053260_core::voice_t::tick(u8 ne)
{
	if (busy)
	{
		bool update = false;
		// update counter
		if (bitfield(++counter, 0, 12) == 0)
		{
			if (adpcm)
			{
				bitpos += 4;
				if (bitpos >= 8)
				{
					addr = bitfield(addr + 1, 0, 21);
					remain--;
				}
				update = true;
			}
			else
			{
				addr = bitfield(addr + 1, 0, 21);
				remain--;
			}
		}
		data = m_host.m_intf.read_sample(ne, bitfield(addr, 0, 21)); // fetch ROM
		if (update)
		{
			const u8 nibble = bitfield(data, bitpos, 4); // get nibble from ROM
			if (nibble)
				adpcm_buf += bitfield(nibble, 3) ? s8(0x80 >> bitfield(nibble, 0, 3)) : (1 << bitfield(nibble - 1, 0, 3));
		}

		if (remain < 0) // check end flag
		{
			if (loop)
			{
				addr = start;
				adpcm_buf = 0;
			}
			else
				busy = false;
		}
		// calculate output
		s32 output = adpcm ? adpcm_buf : sign_ext<s32>(data, 8) * s32(volume);
		const int pan_dir[8] = {-1,0,24,35,45,55,66,90};
		const int pan_bit = pan_dir[bitfield(pan, 0, 3)];
		// use math for now; actually fomula unknown
		out[0] = (pan_bit >= 0) ? s32(output * cos(f64(pan_bit) * PI / 180)) : 0;
		out[1] = (pan_bit >= 0) ? s32(output * sin(f64(pan_bit) * PI / 180)) : 0;
	}
	else
		out[0] = out[1] = 0;
}

void k053260_core::write(u8 address, u8 data)
{
	address &= 0x3f; // 6 bit for CPU write

	switch (address)
	{
		case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: // voice 0
		case 0x6: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: // voice 1
			m_voice[(address / 6) & 1].write(address % 6, data);
			break;
		case 0xc: // external register with SLEV pin
			m_intf.write_slev(data);
			break;
		case 0xd: // loop flag
			m_voice[0].loop = bitfield(data, 0);
			m_voice[1].loop = bitfield(data, 1);
			break;
		default:
			break;
	}

	m_reg[address] = data;
}

// write registers on each voices
void k053260_core::voice_t::write(u8 address, u8 data)
{
	switch (address)
	{
		case 0: // pitch LSB
			pitch = (pitch & ~0x00ff) | data;
			break;
		case 1: // pitch MSB, divider
			pitch = (pitch & ~0x3f00) | (u16(bitfield(data, 0, 6)) << 8);
			break;
		case 2: // start address bit 0-7
			start = (start & ~0x000ff) | data;
			break;
		case 3: // start address bit 8-15
			start = (start & ~0x0ff00) | (u16(data) << 8);
			break;
		case 4: // start address bit 16
			start = (start & ~0x10000) | (u16(bitfield(data, 16)) << 16);
			break;
		case 5: // keyon trigger
			keyon();
			break;
	}
}

// key on trigger (write OR read 0x05/0x11 register)
void k053260_core::voice_t::keyon()
{
	busy = true;
	counter = bitfield(pitch, 0, 12);
	addr = start;
	remain = length;
	bitpos = 0;
	adpcm_buf = 0;
	out[0] = out[1] = 0;
}

// reset chip
void k053260_core::reset()
{
	for (auto & elem : m_voice)
		elem.reset();

	m_intf.write_slev(0);

	std::fill(std::begin(m_host2snd), std::end(m_host2snd), 0);
	std::fill(std::begin(m_snd2host), std::end(m_snd2host), 0);
	m_ctrl = 0;

	std::fill(std::begin(m_reg), std::end(m_reg), 0);
	std::fill(std::begin(m_out), std::end(m_out), 0);
}

// reset voice
void k053260_core::voice_t::reset()
{
	busy = false;
	loop = false;
	adpcm = false;
	pitch = 0;
	start = 0;
	length = 0;
	volume = 0;
	pan = 0;
	counter = 0;
	addr = 0;
	remain = 0;
	bitpos = 0;
	data = 0;
	adpcm_buf = 0;
	out[0] = out[1] = 0;
}
