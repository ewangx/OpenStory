//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "Session.h"

#include "../Configuration.h"

#include <iostream>

namespace ms
{
	Session::Session()
	{
		connected = false;
	}

	Session::~Session()
	{
		if (connected)
			socket.close();
	}

	bool Session::init(const char* host, const char* port)
	{
		framer.reset();

		// Connect to the server
		connected = socket.open(host, port);

		if (connected)
		{
			// Read keys necessary for communicating with the server
			cryptography = { socket.get_buffer() };
		}

		return connected;
	}

	Error Session::init()
	{
		std::string HOST = Setting<ServerIP>::get().load();
		std::string PORT = Setting<ServerPort>::get().load();

		if (!init(HOST.c_str(), PORT.c_str()))
			return Error::CONNECTION;

		return Error::NONE;
	}

	void Session::reconnect(const char* address, const char* port)
	{
		// Close the current connection and open a new one
		bool success = socket.close();

		if (success)
			init(address, port);
		else
			connected = false;
	}

	void Session::process(const int8_t* bytes, size_t available)
	{
		framer.process(bytes, available,
			[this](const int8_t* header)
			{
				return cryptography.check_length(header);
			},
			[this](int8_t* packet, size_t packet_length)
			{
				cryptography.decrypt(packet, packet_length);

				try
				{
					packetswitch.forward(packet, packet_length);
				}
				catch (const PacketError& error)
				{
					std::cerr << "[Packet] " << error.what() << std::endl;
				}
				catch (const std::exception& error)
				{
					std::cerr << "[Packet] Handler failed: " << error.what() << std::endl;
				}
			});
	}

	void Session::write(int8_t* packet_bytes, size_t packet_length)
	{
		if (!connected)
			return;

		int8_t header[HEADER_LENGTH];
		cryptography.create_header(header, packet_length);
		cryptography.encrypt(packet_bytes, packet_length);

		if (!socket.dispatch(header, HEADER_LENGTH) || !socket.dispatch(packet_bytes, packet_length))
		{
			std::cerr << "[Packet] Send failed; closing connection" << std::endl;
			connected = false;
			framer.reset();
			socket.close();
		}
	}

	void Session::read()
	{
		// Preserve every received byte; TCP may split a packet at any boundary.
		size_t result = socket.receive(&connected);

		if (result > 0)
		{
			// Retrieve buffer from the socket and process it
			const int8_t* bytes = socket.get_buffer();

			try
			{
				process(bytes, result);
			}
			catch (const PacketError& error)
			{
				std::cerr << "[Packet] Closing connection: " << error.what() << std::endl;
				connected = false;
				framer.reset();
				socket.close();
			}
		}
	}

	void Session::reconnect()
	{
		std::string HOST = Setting<ServerIP>::get().load();
		std::string PORT = Setting<ServerPort>::get().load();

		reconnect(HOST.c_str(), PORT.c_str());
	}

	bool Session::is_connected() const
	{
		return connected;
	}
}
