#pragma once

namespace rkit
{
	struct NoCopy
	{
		NoCopy() {}

	private:
		NoCopy(const NoCopy &) = delete;
		NoCopy &operator=(const NoCopy &) = delete;
	};
}
