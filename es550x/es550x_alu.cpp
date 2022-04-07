/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Ensoniq ES5504/ES5505/ES5506 Shared Accumulator emulation core

	see es550x.cpp for more info
*/

#include "es550x.hpp"

// Accumulator functions
template<u8 Integer, u8 Fraction, bool Transwave>
void es550x_shared_core::es550x_alu_t<Integer, Fraction, Transwave>::reset()
{
	m_cr.reset();
	m_fc = 0;
	m_start = 0;
	m_end = 0;
	m_accum = 0;
	m_sample[0] = m_sample[1] = 0;
}

template<u8 Integer, u8 Fraction, bool Transwave>
bool es550x_shared_core::es550x_alu_t<Integer, Fraction, Transwave>::busy()
{
	return ((!m_cr.stop0) && (!m_cr.stop1));
}

template<u8 Integer, u8 Fraction, bool Transwave>
bool es550x_shared_core::es550x_alu_t<Integer, Fraction, Transwave>::tick()
{
	if (m_cr.dir)
	{
		m_accum = bitfield(m_accum - m_fc, 0, total_bits);
		return ((!m_cr.lei) && (m_accum < m_start)) ? true : false;
	}
	else
	{
		m_accum = bitfield(m_accum + m_fc, 0, total_bits);
		return ((!m_cr.lei) && (m_accum > m_end)) ? true : false;
	}
}

template<u8 Integer, u8 Fraction, bool Transwave>
void es550x_shared_core::es550x_alu_t<Integer, Fraction, Transwave>::loop_exec()
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
		else if (m_cr.ble && Transwave) // Transwave
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
		else if (m_cr.ble && Transwave) // Transwave
		{
			m_cr.lpe = m_cr.ble = 0;
			m_cr.lei = 1; // Loop end ignore
			m_accum = (m_accum - m_end) + m_start;
		}
		else // Stop
			m_cr.stop0 = 1;
	}
}

template<u8 Integer, u8 Fraction, bool Transwave>
s32 es550x_shared_core::es550x_alu_t<Integer, Fraction, Transwave>::interpolation()
{
	// SF = S1 + ACCfr * (S2 - S1)
	return in + ((bitfield(m_accum, std::min<u8>(0, Fraction - 9), 9) * (m_sample[1] - m_sample[0])) >> 9);
}

template<u8 Integer, u8 Fraction, bool Transwave>
u32 es550x_shared_core::es550x_alu_t<Integer, Fraction, Transwave>::get_accum_integer()
{
	return bitfield(m_accum, Fraction, Integer);
}
