/*
	License: Zlib
	see https://github.com/cam900/vgsound_emu/blob/main/LICENSE for more details

	Copyright holder(s): (Author name)
	Template for sound emulation core, also guideline
*/

#ifndef _VGSOUND_EMU_SRC_TEMPLATE_HPP  // _VGSOUND_EMU_ABSOLUTE_PATH_OF_THIS_FILE
#define _VGSOUND_EMU_SRC_TEMPLATE_HPP

#pragma once

#include "../core/util.hpp"
using namespace vgsound_emu;

class template_core
{
		friend class vgsound_emu_mem_intf;	// common memory interface if exists

	private:  // protected: if shares between inheritances
		// place classes and local constants here if exists

		// template voice classes
		class voice_t
		{
			public:
				// constructor
				voice_t(template_core &host)
					: m_host(host)
					, m_something(0)
				{
				}

				// internal state
				void reset();
				void tick();

				// accessors, getters, setters

				// setters
				void set_something(s32 something) { m_something = something; }

				// getters
				s32 something() { return m_something; }

			private:
				// registers
				template_core &m_host;
				s32 m_something = 0;  // register
		};

	public:
		// place constructor and destructor, getter and setter for local variables,
		// off-chip interfaces, update routine here only if can't be local

		// constructor
		template_core(vgsound_emu_mem_intf &intf)
			: m_voice{*this}
			, m_intf(intf)
		{
		}

		// accessors, getters, setters

		// internal state
		void reset();
		void tick();

	protected:
		// place local variables and functions here if shares between inheritances

	private:
		// place local variables and functions here

		std::array<voice_t, 1 /*number of voices*/> m_voice;   // voice classes
		vgsound_emu_mem_intf &m_intf;						   // common memory interface
		std::array<u8 /*type*/, 8 /*size of array*/> m_array;  // std::array for static size array
		std::vector<u8 /*type*/> m_vector;	// std::vector for variable size array
};

#endif
