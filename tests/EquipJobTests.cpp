#include "Character/JobEquipRequirement.h"

#include <iostream>

int main()
{
	int failures = 0;
	const auto expect = [&failures](bool actual, bool expected, const char* case_name)
	{
		if (actual != expected)
		{
			std::cerr << "FAIL: " << case_name << '\n';
			++failures;
		}
	};

	constexpr int16_t ALL_JOBS = 0;
	constexpr int16_t WARRIOR = 1;
	constexpr int16_t MAGICIAN = 2;
	constexpr int16_t THIEF = 8;
	constexpr int16_t PIRATE = 16;

	expect(ms::meets_equip_job_requirement(400, THIEF), true, "Rogue equips thief gear");
	expect(ms::meets_equip_job_requirement(422, THIEF), true, "advanced thief equips thief gear");
	expect(ms::meets_equip_job_requirement(100, THIEF), false, "warrior cannot equip thief gear");
	expect(ms::meets_equip_job_requirement(400, WARRIOR), false, "Rogue cannot equip warrior gear");
	expect(ms::meets_equip_job_requirement(400, MAGICIAN | THIEF), true, "combined job flags include thief");
	expect(ms::meets_equip_job_requirement(200, MAGICIAN | THIEF), true, "combined job flags include magician");
	expect(ms::meets_equip_job_requirement(500, PIRATE), true, "pirate equips pirate gear");
	expect(ms::meets_equip_job_requirement(1410, THIEF), true, "Night Walker equips thief gear");
	expect(ms::meets_equip_job_requirement(2112, WARRIOR), true, "Aran equips warrior gear");
	expect(ms::meets_equip_job_requirement(0, THIEF), false, "beginner cannot equip thief gear");
	expect(ms::meets_equip_job_requirement(0, ALL_JOBS), true, "beginner equips unrestricted gear");
	expect(ms::meets_equip_job_requirement(400, ALL_JOBS), true, "Rogue equips unrestricted gear");

	return failures == 0 ? 0 : 1;
}
