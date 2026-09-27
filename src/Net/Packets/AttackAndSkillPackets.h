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
#pragma once

#include "../OutPacket.h"
#include "../../Character/SkillId.h"

namespace ms
{
	// Notifies the server of an attack
	// The opcode is determined by the attack type
	// Attack::CLOSE = CLOSE_ATTACK(44)
	// Attack::RANGED = RANGED_ATTACK(45)
	// Attack::MAGIC = MAGIC_ATTACK(46)
	class AttackPacket : public OutPacket
	{
	public:
		AttackPacket(const AttackResult& attack) : OutPacket(opcodefor(attack.type))
		{
			skip(1);

			if (attack.skill == SkillId::MESO_EXPLOSION)
			{
				write_meso_explosion(attack);
				return;
			}

			write_byte((attack.mobcount << 4) | attack.hitcount);
			write_int(attack.skill);

			if (attack.charge > 0)
				write_int(attack.charge);

			skip(8);

			write_byte(attack.display);
			write_byte(attack.toleft);
			write_byte(attack.stance);

			skip(1);

			write_byte(attack.speed);

			if (attack.type == Attack::Type::RANGED)
			{
				skip(1);
				write_byte(attack.toleft);
				skip(7);

				// Hurricane/piercing arrow/rapidfire require additional 4 bytes
				if (attack.skill == 5221004 || attack.skill == 5721001 || attack.skill == 4341002 || attack.skill == 4341003)
					skip(4);
			}
			else
			{
				skip(4);
			}

			for (auto& damagetomob : attack.damagelines)
			{
				write_int(damagetomob.first);

				skip(14);

				for (auto& singledamage : damagetomob.second)
				{
					write_int(singledamage.first);
					// Critical hit flag placeholder
				}

				if (attack.skill != 5221004)
					skip(4);
			}
		}

	private:
		// Meso Explosion uses its own CLOSE_ATTACK layout on the wire, which
		// Cosmic decodes in parseMesoExplosion: per-mob damage-count byte
		// (instead of the fixed hitcount / delay), then a trailing exploded
		// meso list and attack delay. Keep this byte-identical to that parser.
		void write_meso_explosion(const AttackResult& attack)
		{
			uint8_t mobcount = static_cast<uint8_t>(attack.damagelines.size());
			uint8_t hitnibble = static_cast<uint8_t>(attack.hitcount & 0xF);

			write_byte(static_cast<int8_t>((mobcount << 4) | hitnibble));
			write_int(attack.skill);

			skip(8);

			write_byte(attack.display);
			write_byte(attack.toleft);
			write_byte(attack.stance);

			// Matches the 6 bytes parseMesoExplosion skips here
			// (normal close-attack speed + padding).
			skip(1);
			write_byte(attack.speed);
			skip(4);

			for (auto& damagetomob : attack.damagelines)
			{
				write_int(damagetomob.first);

				skip(4);
				write_point(Point<int16_t>(0, 0));
				write_point(Point<int16_t>(0, 0));

				uint8_t lines = static_cast<uint8_t>(damagetomob.second.size() & 0xFF);
				write_byte(static_cast<int8_t>(lines));

				for (auto& singledamage : damagetomob.second)
					write_int(singledamage.first);

				skip(4);
			}

			skip(4);

			uint8_t mesos = static_cast<uint8_t>(attack.exploded_drops.size() & 0xFF);
			write_byte(static_cast<int8_t>(mesos));

			for (int32_t oid : attack.exploded_drops)
			{
				write_int(oid);
				skip(1);
			}

			write_short(0);
		}

		static OutPacket::Opcode opcodefor(Attack::Type type)
		{
			switch (type)
			{
			case Attack::Type::CLOSE:
				return OutPacket::Opcode::CLOSE_ATTACK;
			case Attack::Type::RANGED:
				return OutPacket::Opcode::RANGED_ATTACK;
			default:
				return OutPacket::Opcode::MAGIC_ATTACK;
			}
		}
	};

	// Tells the server that the player took damage
	// Opcode: TAKE_DAMAGE(48)
	class TakeDamagePacket : public OutPacket
	{
	public:
		enum From : int8_t
		{
			TOUCH = -1
		};

		TakeDamagePacket(int8_t from, uint8_t element, int32_t damage, int32_t mobid, int32_t oid, uint8_t direction) : OutPacket(OutPacket::Opcode::TAKE_DAMAGE)
		{
			write_time();
			write_byte(from);
			write_byte(element);
			write_int(damage);
			write_int(mobid);
			write_int(oid);
			write_byte(direction);
		}

		// From mob attack result
		TakeDamagePacket(const MobAttackResult& result, From from) : TakeDamagePacket(from, 0, result.damage, result.mobid, result.oid, result.direction) {}
	};

	// Packet which notifies the server of a skill usage
	// Opcode: USE_SKILL(91)
	class UseSkillPacket : public OutPacket
	{
	public:
		UseSkillPacket(int32_t skillid, int32_t level) : OutPacket(OutPacket::Opcode::USE_SKILL)
		{
			write_time();
			write_int(skillid);
			write_byte(static_cast<uint8_t>(level));

			// If monster magnet : some more bytes

			if (skillid % 10000000 == 1004)
				skip(2); // Unknown 2 bytes

			// Optional position data (4 bytes) may follow
		}
	};
}