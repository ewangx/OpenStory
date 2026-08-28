#include "Net/InPacket.h"
#include "Net/PacketFramer.h"
#include "Net/Cryptography.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	int failures = 0;

	void expect(bool condition, const std::string& message)
	{
		if (!condition)
		{
			std::cerr << "FAIL: " << message << '\n';
			failures++;
		}
	}

	void expect_packet_error(const std::function<void()>& operation, const std::string& message)
	{
		try
		{
			operation();
			expect(false, message);
		}
		catch (const ms::PacketError&)
		{
		}
	}

	std::vector<int8_t> frame(const std::vector<int8_t>& payload)
	{
		const uint32_t length = static_cast<uint32_t>(payload.size());
		std::vector<int8_t> bytes = {
			static_cast<int8_t>(length),
			static_cast<int8_t>(length >> 8),
			static_cast<int8_t>(length >> 16),
			static_cast<int8_t>(length >> 24)
		};
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		return bytes;
	}

	size_t decode_length(const int8_t* header)
	{
		size_t length = 0;

		for (size_t i = 0; i < ms::HEADER_LENGTH; i++)
			length |= static_cast<size_t>(static_cast<uint8_t>(header[i])) << (8 * i);

		return length;
	}

	void test_packet_reader()
	{
		const int8_t numbers[] = { 0x78, 0x56, 0x34, 0x12, -1, -1 };
		ms::InPacket packet(numbers, sizeof(numbers));

		expect(packet.inspect_int() == 0x12345678, "inspect reads little-endian values");
		expect(packet.length() == sizeof(numbers), "inspect does not advance the packet");
		expect(packet.read_int() == 0x12345678, "read returns a little-endian integer");
		expect(packet.read_short() == -1, "read preserves signed values");
		expect(!packet.available(), "reader consumes exactly the available bytes");

		const int8_t short_value[] = { 1, 2, 3 };
		ms::InPacket short_packet(short_value, sizeof(short_value));
		expect_packet_error([&] { short_packet.read_int(); }, "truncated integer is rejected");
		expect(short_packet.length() == sizeof(short_value), "failed integer read is atomic");

		const int8_t short_string[] = { 4, 0, 'o', 'k' };
		ms::InPacket string_packet(short_string, sizeof(short_string));
		expect_packet_error([&] { string_packet.read_string(); }, "truncated string is rejected");
		expect(string_packet.length() == sizeof(short_string), "failed string read is atomic");

		std::vector<int8_t> maximum_string(2 + 65535, 'x');
		maximum_string[0] = -1;
		maximum_string[1] = -1;
		ms::InPacket maximum_string_packet(maximum_string.data(), maximum_string.size());
		expect(maximum_string_packet.read_string().size() == 65535, "maximum-length strings are supported");
	}

	void test_fragmented_and_coalesced_packets()
	{
		ms::PacketFramer framer;
		std::vector<std::vector<int8_t>> packets;
		const auto first = frame({ 1, 0, 10, 11, 12 });
		const auto second = frame({ 2, 0, 20 });
		std::vector<int8_t> wire = first;
		wire.insert(wire.end(), second.begin(), second.end());

		for (const int8_t byte : wire)
		{
			framer.process(&byte, 1, decode_length,
				[&](int8_t* payload, size_t length)
				{
					packets.emplace_back(payload, payload + length);
				});
		}

		expect(packets.size() == 2, "one-byte TCP fragments produce two packets");

		if (packets.size() == 2)
		{
			expect(packets[0] == std::vector<int8_t>({ 1, 0, 10, 11, 12 }), "first fragmented payload is intact");
			expect(packets[1] == std::vector<int8_t>({ 2, 0, 20 }), "second fragmented payload is intact");
		}

		packets.clear();
		framer.process(wire.data(), wire.size(), decode_length,
			[&](int8_t* payload, size_t length)
			{
				packets.emplace_back(payload, payload + length);
			});
		expect(packets.size() == 2, "coalesced TCP data produces every packet");
	}

	void test_malformed_lengths()
	{
		const int8_t header[ms::HEADER_LENGTH] = {};

		ms::PacketFramer too_short;
		expect_packet_error(
			[&]
			{
				too_short.process(header, sizeof(header), [](const int8_t*) { return ms::OPCODE_LENGTH - 1; },
					[](int8_t*, size_t) {});
			},
			"payloads without an opcode are rejected");

		ms::PacketFramer too_large;
		expect_packet_error(
			[&]
			{
				too_large.process(header, sizeof(header), [](const int8_t*) { return ms::MAX_PACKET_LENGTH + 1; },
					[](int8_t*, size_t) {});
			},
			"oversized payloads are rejected");
	}

	void test_encrypted_length_is_unsigned()
	{
		const int8_t header[] = { 0, 0, 0, -128 };
		ms::Cryptography cryptography;
		expect(cryptography.check_length(header) == 32768, "encrypted lengths retain the unsigned 16-bit range");
	}
}

int main()
{
	test_packet_reader();
	test_fragmented_and_coalesced_packets();
	test_malformed_lengths();
	test_encrypted_length_is_unsigned();

	if (failures == 0)
		std::cout << "All network tests passed\n";

	return failures == 0 ? 0 : 1;
}
