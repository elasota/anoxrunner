#pragma once

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct Result;
}

namespace anox
{
	struct ICoroutineContext;

	struct ICoroutine
	{
		virtual rkit::Result Run(ICoroutineContext &context, rkit::UniquePtr<ICoroutine> &inOutStackTop) = 0;
	};
}
