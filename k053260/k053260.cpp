/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Konami K053260 core

	It's one of Konami's custom PCM playback chip with CPU to CPU communication feature, and built in timer.
	It's architecture is successed from K007232, but it features various enhancements:
	it's expanded to 4 channels, Supports more memory space, 4 bit ADPCM, Built in volume and stereo panning support,
	and Dual chip configurations.
	There's 2 stereo inputs and single stereo output, Both format is YM3012 compatible.

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

	2f      ---- x---   W AUX2 input enable
	        ---- -x--   W AUX1 input enable
	        ---- --x-   W Sound enable
	        ---- ---x   W ROM readbank enable

	* Frequency calculation:
	Frequency: Input clock / (4096 - Pitch)

	** Actually fomula unknown, Use floating point type until explained that.
*/

#include "k053260.hpp"

void k053260_core::tick()
{
	m_out[0] = m_out[1] = 0;
	if (m_ctrl.sound_en)
	{
		for (int i = 0; i < 4; i++)
		{
			m_voice[i].tick(i);
			m_out[0] += m_voice[i].out[0];
			m_out[1] += m_voice[i].out[1];
		}
	}
	// dac clock (YM3012 format)
	if (bitfield(++m_dac.clock, 0, 4) == 0)
	{
		m_intf.write_int(m_dac.state);
		if (bitfield(++m_dac.state, 0) == 0)
		{
			m_ym3012.m_out[bitfield(m_dac.state, 1)] = m_ym3012.m_in[bitfield(m_dac.state, 1)];
			m_ym3012.m_in[bitfield(m_dac.state, 1) ^ 1] = m_out[bitfield(m_dac.state, 1) ^ 1];
		}
	}
}

void k053260_core::voice_t::tick(u8 ne)
{
	if (enable && busy)
	{
		bool update = false;
		// update counter
		if (bitfield(++counter, 0, 12) == 0)
		{
			if (bitpos < 8)
			{
				bitpos += 8;
				addr = bitfield(addr + 1, 0, 21);
				remain--;
			}
			if (adpcm)
			{
				bitpos -= 4;
				update = true;
			}
			else
				bitpos -= 8;
		}
		data = m_host.m_intf.read_sample(bitfield(addr, 0, 21)); // fetch ROM
		if (update)
		{
			const u8 nibble = bitfield(data, bitpos & 4, 4); // get nibble from ROM
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
		// use math for now; actually fomula unknown
		out[0] = (pan >= 0) ? s32(output * cos(f64(pan) * PI / 180)) : 0;
		out[1] = (pan >= 0) ? s32(output * sin(f64(pan) * PI / 180)) : 0;
	}
	else
		out[0] = out[1] = 0;
}

u8 k053260_core::read(u8 address)
{
	address &= 0x3f; // 6 bit for CPU read

	switch (address)
	{
		case 0x0: case 0x1: // Answer from host
			return m_host2snd[address & 1];
			break;
		case 0x29: // Voice playing status
			return (m_voice[0].busy ? 0x1:0x0)|
			       (m_voice[1].busy ? 0x2:0x0)|
			       (m_voice[2].busy ? 0x4:0x0)|
			       (m_voice[3].busy ? 0x8:0x0);
		case 0x2e: // ROM readback
		{
			if (!m_ctrl.rom_read)
				return 0xff;

			const u32 rom_addr = m_voice[0].start + m_voice[0].length;
			m_voice[0].length = bitfield(m_voice[0].length + 1, 0, 16);
			return m_intf.read_sample(rom_addr);
		}
	}
	return 0xff;
}

void k053260_core::write(u8 address, u8 data)
{
	address &= 0x3f; // 6 bit for CPU write

	switch (address)
	{
		case 0x2: case 0x3: // Reply to host
			m_snd2host[address & 1] = data;
			break;
		case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: // voice 0
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: // voice 1
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: // voice 2
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: // voice 3
			m_voice[bitfield(address - 0x8, 3, 2)].write(bitfield(address, 0, 3), data);
			break;
		case 0x28: // keyon/off toggle
			for (int i = 0; i < 4; i++)
			{
				if (bitfield(data, i) && (!m_voice[i].enable)) // rising edge (keyon)
					m_voice[i].keyon();
				else if ((!bitfield(data, i)) && m_voice[i].enable) // falling edge (keyoff)
					m_voice[i].enable = m_voice[i].busy = false;
			}
			break;
		case 0x2a: // loop/adpcm flag
			for (int i = 0; i < 4; i++)
			{
				m_voice[i].loop = bitfield(data, i);
				m_voice[i].adpcm = bitfield(data, i + 4);
			}
			break;
		case 0x2c:
			m_voice[0].pan = pan_dir[bitfield(data, 0, 3)];
			m_voice[1].pan = pan_dir[bitfield(data, 3, 3)];
			break;
		case 0x2d:
			m_voice[2].pan = pan_dir[bitfield(data, 0, 3)];
			m_voice[3].pan = pan_dir[bitfield(data, 3, 3)];
			break;
		case 0x2f:
			m_ctrl.rom_read = bitfield(data, 0);
			m_ctrl.sound_en = bitfield(data, 1);
			m_ctrl.input_en = bitfield(data, 2);
			m_ctrl.dual_chip = bitfield(data, 3);
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
		case 1: // pitch MSB
			pitch = (pitch & ~0x0f00) | (u16(bitfield(data, 0, 4)) << 8);
			break;
		case 2: // source length LSB
			length = (length & ~0x000ff) | data;
			break;
		case 3: // source length MSB
			length = (length & ~0x0ff00) | (u16(data) << 8);
			break;
		case 4: // start address bit 0-7
			start = (start & ~0x0000ff) | data;
			break;
		case 5: // start address bit 8-15
			start = (start & ~0x00ff00) | (u32(data) << 8);
			break;
		case 6: // start address bit 16-20
			start = (start & ~0x1f0000) | (u32(bitfield(data, 16, 5)) << 16);
			break;
		case 7: // volume
			volume = bitfield(data, 0, 7);
			break;
	}
}

// key on trigger (write OR read 0x05/0x11 register)
void k053260_core::voice_t::keyon()
{
	enable = true;
	busy = true;
	counter = bitfield(pitch, 0, 12);
	addr = start;
	remain = length;
	bitpos = 4;
	adpcm_buf = 0;
	out[0] = out[1] = 0;
}

// reset chip
void k053260_core::reset()
{
	for (auto & elem : m_voice)
		elem.reset();

	m_intf.write_int(0);

	std::fill(std::begin(m_host2snd), std::end(m_host2snd), 0);
	std::fill(std::begin(m_snd2host), std::end(m_snd2host), 0);
	m_ctrl.reset();
	m_dac.reset();

	std::fill(std::begin(m_reg), std::end(m_reg), 0);
	std::fill(std::begin(m_out), std::end(m_out), 0);
}

// reset voice
void k053260_core::voice_t::reset()
{
	enable = false;
	busy = false;
	loop = false;
	adpcm = false;
	pitch = 0;
	start = 0;
	length = 0;
	volume = 0;
	pan = -1;
	counter = 0;
	addr = 0;
	remain = 0;
	bitpos = 4;
	data = 0;
	adpcm_buf = 0;
	out[0] = out[1] = 0;
}
