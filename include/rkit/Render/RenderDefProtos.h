#pragma once

namespace rkit::render
{
	struct StructureMemberDesc;

	template<int TPurpose>
	class StringIndex;

	typedef StringIndex<0> GlobalStringIndex_t;
	typedef StringIndex<1> ConfigStringIndex_t;
}