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

		rkit::Result Parse(const rkit::Span<const uint8_t> &stream) override;
		rkit::Result Push(const rkit::StringSliceView &strView) override;
		rkit::Result PushMultiple(const rkit::ISpan<rkit::StringSliceView> &spans) override;

		bool Pop(rkit::StringView &outString) override;

	private:
		rkit::Vector<char> m_contentsBuffer;
		size_t m_contentsSize;
		size_t m_maxCapacity;
	};

	AnoxCommandStack::AnoxCommandStack(size_t maxCapacity)
		: m_maxCapacity(maxCapacity)
		, m_contentsSize(0)
	{
	}

	rkit::Result AnoxCommandStack::Parse(const rkit::Span<const uint8_t> &streamRef)
	{
		const rkit::Span<const uint8_t> stream = streamRef;

		size_t startPos = 0;

		rkit::Vector<rkit::StringSliceView> lines;

		size_t endPos = 0;
		bool isInQuote = false;
		while (endPos <= stream.Count())
		{
			bool isTerminal = false;
			bool isCRLF = false;
			if (endPos == stream.Count())
			{
				isTerminal = true;
			}
			else
			{
				const uint8_t *cptr = &stream[endPos];
				char c = *cptr;
				if (c == '\n')
					isTerminal = true;
				else if (c == '\r')
				{
					isTerminal = true;
					if (endPos + 1 < stream.Count() && stream[endPos + 1] == '\n')
						isCRLF = true;
				}
				else if (c == '\"')
					isInQuote = !isInQuote;
				else if (c == ';')
				{
					if (!isInQuote)
						isTerminal = true;
				}
			}

			if (isTerminal)
			{
				rkit::ConstSpan<uint8_t> lineSpan = stream.SubSpan(startPos, endPos - startPos);

				// Trim comments
				if (lineSpan.Count() > 1)
				{
					bool isInQuote = false;
					size_t commentScanDist = lineSpan.Count() - 1;

					for (size_t i = 0; i < commentScanDist; i++)
					{
						if (lineSpan[i] == '\"')
							isInQuote = !isInQuote;
						else
						{
							if (!isInQuote && lineSpan[i] == '/' && lineSpan[i + 1] == '/')
							{
								lineSpan = lineSpan.SubSpan(0, i);
								break;
							}
						}
					}
				}

				// Trim starting whitespace
				while (lineSpan.Count() > 0 && lineSpan[0] <= ' ')
					lineSpan = lineSpan.SubSpan(1);

				// Trim ending whitespace
				while (lineSpan.Count() > 0 && lineSpan[lineSpan.Count() - 1] <= ' ')
					lineSpan = lineSpan.SubSpan(0, lineSpan.Count() - 1);

				// If there's anything left, add it
				if (lineSpan.Count() > 0)
				{
					rkit::StringSliceView slice(lineSpan.ReinterpretCast<const char>());
					RKIT_CHECK(lines.Append(slice));
				}
			}

			endPos++;
			if (isCRLF)
				endPos++;

			if (isTerminal)
			{
				startPos = endPos;
				isInQuote = false;
			}
		}

		rkit::ReverseSpanOrder(lines.ToSpan());

		return rkit::ResultCode::kOK;
	}

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
		RKIT_CHECK(m_contentsBuffer.Resize(m_contentsSize + sizeRequired));
		m_contentsSize += sizeRequired;

		rkit::Span<char> contentsSpan = m_contentsBuffer.ToSpan();

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

		RKIT_ASSERT(insertPos == m_contentsSize);

		return rkit::ResultCode::kOK;
	}


	bool AnoxCommandStack::Pop(rkit::StringView &outString)
	{
		if (m_contentsSize > 0)
		{
			rkit::ConstSpan<char> sizeSpan = m_contentsBuffer.ToSpan().SubSpan(m_contentsSize - sizeof(size_t));
			size_t size = 0;
			rkit::CopySpanNonOverlapping(rkit::Span<char>(reinterpret_cast<char *>(&size), 1), sizeSpan);

			size_t startPos = m_contentsSize - sizeof(size_t) - size - 1;

			outString = rkit::StringView(&m_contentsBuffer[startPos], size);

			m_contentsSize = startPos;

			return true;
		}

		return true;
	}

	rkit::Result AnoxCommandStackBase::Create(rkit::UniquePtr<AnoxCommandStackBase> &stack, size_t maxCapacity)
	{
		return rkit::New<AnoxCommandStack>(stack, maxCapacity);
	}
}
