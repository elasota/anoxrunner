#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	struct ISpan;

	template<class T>
	class Span;
}

namespace anox
{
	class AnoxCommandStackBase
	{
	public:
		virtual ~AnoxCommandStackBase() {}

		virtual rkit::Result Parse(const rkit::Span<const uint8_t>& stream) = 0;
		virtual rkit::Result Push(const rkit::StringSliceView &strView) = 0;
		virtual rkit::Result PushMultiple(const rkit::ISpan<rkit::StringSliceView> &spans) = 0;

		virtual bool Pop(rkit::StringView &outString) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxCommandStackBase> &stack, size_t maxCapacity);
	};
}
