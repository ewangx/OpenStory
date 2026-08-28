//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client                    //
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton                       //
//                                                                              //
//	This program is free software: you can redistribute it and/or modify        //
//	it under the terms of the GNU Affero General Public License as published by  //
//	the Free Software Foundation, either version 3 of the License, or            //
//	(at your option) any later version.                                         //
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "NetConstants.h"
#include "PacketError.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace ms
{
	class PacketFramer
	{
	public:
		void reset()
		{
			header_pos = 0;
			payload_pos = 0;
			payload_length = 0;
		}

		template <typename LengthDecoder, typename PacketHandler>
		void process(const int8_t* bytes, size_t available, LengthDecoder decode_length, PacketHandler handle_packet)
		{
			while (available > 0)
			{
				if (header_pos < HEADER_LENGTH)
				{
					const size_t count = std::min(HEADER_LENGTH - header_pos, available);
					std::memcpy(header.data() + header_pos, bytes, count);
					header_pos += count;
					bytes += count;
					available -= count;

					if (header_pos < HEADER_LENGTH)
						continue;

					payload_length = decode_length(header.data());

					if (payload_length < OPCODE_LENGTH || payload_length > MAX_PACKET_LENGTH)
						throw PacketError("Invalid packet length " + std::to_string(payload_length));
				}

				const size_t count = std::min(payload_length - payload_pos, available);
				std::memcpy(payload.data() + payload_pos, bytes, count);
				payload_pos += count;
				bytes += count;
				available -= count;

				if (payload_pos == payload_length)
				{
					const size_t complete_length = payload_length;
					reset();
					handle_packet(payload.data(), complete_length);
				}
			}
		}

	private:
		std::array<int8_t, HEADER_LENGTH> header{};
		std::array<int8_t, MAX_PACKET_LENGTH> payload{};
		size_t header_pos = 0;
		size_t payload_pos = 0;
		size_t payload_length = 0;
	};
}
