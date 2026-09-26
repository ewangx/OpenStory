#pragma once

#include <cstdint>

namespace ms
{
	// Equipment reqJob is a bitmask of Explorer job classes, not a job ID.
	inline bool meets_equip_job_requirement(uint16_t job_id, int16_t required_jobs)
	{
		constexpr int32_t JOB_CLASS_DIVISOR = 100;
		constexpr int32_t WARRIOR_CLASS = 1;
		constexpr int32_t PIRATE_CLASS = 5;
		constexpr int32_t CYGNUS_CLASS_OFFSET = 10;
		constexpr int32_t ARAN_FIRST_CLASS = 20;
		constexpr int32_t ARAN_LAST_CLASS = 21;
		constexpr int16_t ALL_JOBS = 0;
		constexpr int32_t FIRST_JOB_FLAG_SHIFT = WARRIOR_CLASS;

		if (required_jobs == ALL_JOBS)
			return true;

		int32_t jobclass = job_id / JOB_CLASS_DIVISOR;
		if (jobclass >= WARRIOR_CLASS + CYGNUS_CLASS_OFFSET && jobclass <= PIRATE_CLASS + CYGNUS_CLASS_OFFSET)
			jobclass -= CYGNUS_CLASS_OFFSET;
		else if (jobclass >= ARAN_FIRST_CLASS && jobclass <= ARAN_LAST_CLASS)
			jobclass = WARRIOR_CLASS;

		return jobclass >= WARRIOR_CLASS && jobclass <= PIRATE_CLASS &&
			(required_jobs & (1 << (jobclass - FIRST_JOB_FLAG_SHIFT))) != 0;
	}
}
