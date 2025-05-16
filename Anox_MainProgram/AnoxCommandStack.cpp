#include "AnoxCommandStack.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Tuple.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class AnoxCommandStack final : public AnoxCommandStackBase
	{
	public:
		explicit AnoxCommandStack(size_t maxCapacity);

		rkit::Result Push(const rkit::StringSliceView &strView) override;
		rkit::Result PushMultiple(const rkit::ISpan<rkit::StringSliceView> &spans) override;

	private:
		rkit::Vector<char> m_contents;
		size_t m_maxCapacity;
	};

	rkit::Result AnoxCommandStack::Push(const rkit::StringSliceView &strView)
	{
		return PushMultiple(rkit::Span<const rkit::StringSliceView>(&strView, 1).ToValueISpan());
	}

	rkit::Result AnoxCommandStack::PushMultiple(const rkit::ISpan<rkit::StringSliceView> &slices)
	{
		size_t sizeAvailable = m_maxCapacity;

		size_t numSlices = slices.Count();

		size_t sizeRequired = 0;
		for (size_t i = 0; i < numSlices; i++)
		{
			const rkit::StringSliceView slice = slices[i];

			if (sizeAvailable < sizeof(size_t) + 1)
				return rkit::ResultCode::kOperationFailed;

			sizeAvailable -= sizeof(size_t) + 1;

			if (sizeAvailable < slice.Length())
				return rkit::ResultCode::kOperationFailed;

			sizeRequired += sizeof(size_t) + 1 + slice.Length();
		}

		size_t insertPos = 0;
		RKIT_CHECK(m_contents.Resize(m_contents.Count() + sizeRequired));

		rkit::Span<char> contentsSpan = m_contents.ToSpan();

		for (size_t i = 0; i < numSlices; i++)
		{
			const rkit::StringSliceView slice = slices[i];

			rkit::CopySpanNonOverlapping(contentsSpan.SubSpan(insertPos, slice.Length()), slice.ToSpan());
			insertPos += slice.Length();
			contentsSpan[insertPos] = 0;
			insertPos++;

			size_t sliceSize = slice.Length();
			rkit::CopySpanNonOverlapping(contentsSpan.SubSpan(insertPos, sizeof(size_t)), rkit::ConstSpan<char>(reinterpret_cast<char *>(&sliceSize), sizeof(size_t)));

			insertPos += sizeof(size_t);
		}

		RKIT_ASSERT(insertPos == m_contents.Count());

		return rkit::ResultCode::kOK;
	}
}
