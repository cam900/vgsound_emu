/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/blob/vgsound_emu_v1/LICENSE for more details

	Copyright holder(s): (Author name)
	Template for sound emulation core
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_TEMPLATE_HPP
#define _VGSOUND_EMU_TEMPLATE_HPP

#pragma once

class template_core
{
	friend class vgsound_emu_mem_intf; // common memory interface
public:
	// constructor
	template_core(vgsound_emu_mem_intf &intf)
		: m_intf(intf)
	{
	}
	// accessors, getters, setters

	// internal state
	void reset();
	void tick();

private:
	// template voice structs
	/*
	struct voice_t
	{
		// constructor
		voice_t(template_core &host) : m_host(host) {}

		// internal state
		void reset();
		void tick();

		// accessors, getters, setters

		// registers
		template_core &m_host;
	};
	voice_t m_voice[Num];
	*/
	vgsound_emu_mem_intf &m_intf; // common memory interface
};

#endif
