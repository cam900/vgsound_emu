# vgsound_emu

Video game sound chip emulation cores. useful for emulators, chiptune trackers, or players.

## License

This software is published under BSD 3-Clause License, See [here](https://github.com/cam900/vgsound_emu/blob/main/LICENSE) for details.

## Folders

- core: core files used in most of emulation cores
- es550x: Ensoniq ES5504, ES5505, ES5506 PCM sound chip families, 25/32 voices with 16/4 stereo/6 stereo output channels
- k005289: Konami K005289, 2 Wavetable channels (or it's Timer/Address generators...?)
- k007232: Konami K007232, 2 PCM channels
- k053260: Konami K053260, 4 PCM or ADPCM channels with CPU to CPU communication feature
- msm6295: OKI MSM6295, 4 ADPCM channels
- n163: Namco 163, NES Mapper with up to 8 Wavetable channels
- scc: Konami SCC, MSX Mappers with 5 Wavetable channels
- vrcvi: Konami VRC VI, NES Mapper with 2 Pulse channels and 1 Sawtooth channel
- x1_010: Seta/Allumer X1-010, 16 Wavetable/PCM channels
- template: Template for sound emulation core

## Contributors

- [cam900](https://github.com/cam900)
- [Natt Akuma](https://github.com/akumanatt)
- [James Alan Nguyen](https://github.com/djtuBIG-MaliceX)