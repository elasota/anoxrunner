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
		virtual rkit::Result PushBStr(const rkit::ByteStringSliceView &strView) = 0;
		virtual rkit::Result PushMultiple(const rkit::ISpan<rkit::ByteStringSliceView> &spans) = 0;

		rkit::Result Push(const rkit::StringSliceView &strView);

		virtual bool Pop(rkit::Span<uint8_t> &outString) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxCommandStackBase> &stack, size_t maxCapacity, size_t maxLines);
	};
}

#include "rkit/Core/StringView.h"

namespace anox
{
	inline rkit::Result AnoxCommandStackBase::Push(const rkit::StringSliceView &strView)
	{
		return this->PushBStr(strView.RemoveEncoding());
	}
}
