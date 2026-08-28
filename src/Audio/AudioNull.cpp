//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client                    //
//	Copyright (C) 2015-2019 Daniel Allendorf, Ryan Payton                       //
//////////////////////////////////////////////////////////////////////////////////
#include "Audio.h"

#include <utility>

namespace ms
{
	Sound::Sound(Name) : id(0) {}
	Sound::Sound(int32_t) : id(0) {}
	Sound::Sound(nl::node) : id(0) {}
	Sound::Sound() : id(0) {}

	void Sound::play() const {}
	void Sound::play(Point<int16_t>) const {}
	Error Sound::init() { return Error::Code::NONE; }
	void Sound::close() {}
	bool Sound::set_sfxvolume(uint8_t) { return true; }
	void Sound::set_listener_position(Point<int16_t> position) { listener_position = position; }
	void Sound::play(size_t) {}
	void Sound::play(size_t, float, float) {}
	size_t Sound::add_sound(nl::node) { return 0; }
	void Sound::add_sound(Name, nl::node) {}
	void Sound::add_sound(std::string, nl::node) {}
	std::string Sound::format_id(int32_t itemid) { return std::to_string(itemid); }

	std::unordered_map<size_t, uint64_t> Sound::samples;
	EnumMap<Sound::Name, size_t> Sound::soundids;
	std::unordered_map<std::string, size_t> Sound::itemids;
	Point<int16_t> Sound::listener_position;

	Music::Music(std::string p) : path(std::move(p)) {}
	void Music::play() const {}
	void Music::play_once() const {}
	Error Music::init() { return Error::Code::NONE; }
	bool Music::set_bgmvolume(uint8_t) { return true; }
}
