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
#include "SocketWinsock.h"

#ifndef USE_ASIO
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

namespace ms
{
	bool SocketWinsock::open(const char* iaddr, const char* port)
	{
		WSADATA wsa_info;
		sock = INVALID_SOCKET;

		struct addrinfo* addr_info = NULL;
		struct addrinfo* ptr = NULL;
		struct addrinfo hints;

		int result = WSAStartup(MAKEWORD(2, 2), &wsa_info);

		if (result != 0)
			return false;

		ZeroMemory(&hints, sizeof(hints));

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		result = getaddrinfo(iaddr, port, &hints, &addr_info);

		if (result != 0)
		{
			WSACleanup();

			return false;
		}

		for (ptr = addr_info; ptr != NULL; ptr = ptr->ai_next)
		{
			sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

			if (sock == INVALID_SOCKET)
			{
				WSACleanup();

				return false;
			}

			result = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);

			if (result == SOCKET_ERROR)
			{
				closesocket(sock);

				sock = INVALID_SOCKET;

				continue;
			}

			break;
		}

		freeaddrinfo(addr_info);

		if (sock == INVALID_SOCKET)
		{
			WSACleanup();

			return false;
		}

		size_t received = 0;

		while (received < HANDSHAKE_LEN)
		{
			result = recv(sock, reinterpret_cast<char*>(buffer + received), static_cast<int>(HANDSHAKE_LEN - received), 0);

			if (result <= 0)
			{
				closesocket(sock);
				sock = INVALID_SOCKET;
				WSACleanup();

				return false;
			}

			received += static_cast<size_t>(result);
		}

		return true;
	}

	bool SocketWinsock::close()
	{
		int error = closesocket(sock);

		WSACleanup();

		return error != SOCKET_ERROR;
	}

	bool SocketWinsock::dispatch(const int8_t* bytes, size_t length) const
	{
		size_t sent = 0;

		while (sent < length)
		{
			const int result = send(sock, reinterpret_cast<const char*>(bytes + sent), static_cast<int>(length - sent), 0);

			if (result <= 0)
				return false;

			sent += static_cast<size_t>(result);
		}

		return true;
	}

	size_t SocketWinsock::receive(bool* success)
	{
		timeval timeout = { 0, 0 };
		fd_set sockset = { 0 };

		FD_SET(sock, &sockset);

		const int ready = select(0, &sockset, 0, 0, &timeout);

		if (ready == 0)
			return 0;

		if (ready == SOCKET_ERROR)
		{
			*success = false;

			return 0;
		}

		const int result = recv(sock, reinterpret_cast<char*>(buffer), MAX_PACKET_LENGTH, 0);

		if (result <= 0)
		{
			*success = false;

			return 0;
		}

		return static_cast<size_t>(result);
	}

	const int8_t* SocketWinsock::get_buffer() const
	{
		return buffer;
	}
}
#endif
