#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	struct ISpan;
}

namespace anox
{
	class AnoxCommandStackBase
	{
	public:
		virtual ~AnoxCommandStackBase() {}

		virtual rkit::Result Push(const rkit::StringSliceView &strView) = 0;
		virtual rkit::Result PushMultiple(const rkit::ISpan<rkit::StringSliceView> &spans) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxCommandStackBase> &stack, size_t maxCapacity);
	};
}
