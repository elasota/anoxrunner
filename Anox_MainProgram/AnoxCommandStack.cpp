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
		AnoxCommandStack();

		rkit::Result Init(size_t maxCapacity, size_t maxLines);

		rkit::Result Parse(const rkit::Span<const uint8_t> &stream) override;
		rkit::Result Push(const rkit::StringSliceView &strView) override;
		rkit::Result PushMultiple(const rkit::ISpan<rkit::StringSliceView> &spans) override;

		bool Pop(rkit::StringView &outString) override;

	private:
		rkit::Vector<char> m_contentsBuffer;
		rkit::Vector<rkit::StringView> m_lines;
		size_t m_contentsSize;
		size_t m_numLines;
	};

	AnoxCommandStack::AnoxCommandStack()
		: m_contentsSize(0)
		, m_numLines(0)
	{
	}

	rkit::Result AnoxCommandStack::Init(size_t maxCapacity, size_t maxLines)
	{
		RKIT_CHECK(m_contentsBuffer.Resize(maxCapacity));
		RKIT_CHECK(m_lines.Resize(maxLines));

		return rkit::ResultCode::kOK;
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

		RKIT_CHECK(PushMultiple(lines.ToSpan().ToValueISpan()));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxCommandStack::Push(const rkit::StringSliceView &strView)
	{
		return PushMultiple(rkit::Span<const rkit::StringSliceView>(&strView, 1).ToValueISpan());
	}

	rkit::Result AnoxCommandStack::PushMultiple(const rkit::ISpan<rkit::StringSliceView> &slices)
	{
		size_t sizeAvailable = m_contentsBuffer.Count() - m_contentsSize;
		size_t linesAvailable = m_lines.Count() - m_numLines;

		size_t numSlices = slices.Count();

		if (linesAvailable < numSlices)
		{
			rkit::log::Error("Not enough command stack space available");
			return rkit::ResultCode::kOperationFailed;
		}

		size_t sizeRequired = 0;
		for (size_t i = 0; i < numSlices; i++)
		{
			const rkit::StringSliceView slice = slices[i];

			if (sizeAvailable < 1)
			{
				rkit::log::Error("Not enough command stack space available");
				return rkit::ResultCode::kOperationFailed;
			}

			sizeAvailable -= 1;

			if (sizeAvailable < slice.Length())
			{
				rkit::log::Error("Not enough command stack space available");
				return rkit::ResultCode::kOperationFailed;
			}

			sizeRequired += 1 + slice.Length();
		}

		size_t insertPos = 0;
		RKIT_CHECK(m_contentsBuffer.Resize(m_contentsSize + sizeRequired));
		m_contentsSize += sizeRequired;

		rkit::Span<char> contentsSpan = m_contentsBuffer.ToSpan();

		for (size_t i = 0; i < numSlices; i++)
		{
			const rkit::StringSliceView slice = slices[i];
			const rkit::Span<char> targetSpan = contentsSpan.SubSpan(insertPos, slice.Length());

			rkit::CopySpanNonOverlapping(targetSpan, slice.ToSpan());
			insertPos += slice.Length();
			contentsSpan[insertPos] = 0;
			insertPos++;

			m_lines[m_numLines++] = rkit::StringView(targetSpan.Ptr(), targetSpan.Count());
		}

		RKIT_ASSERT(insertPos == m_contentsSize);

		return rkit::ResultCode::kOK;
	}


	bool AnoxCommandStack::Pop(rkit::StringView &outString)
	{
		if (m_numLines > 0)
		{
			--m_numLines;
			outString = m_lines[m_numLines];

			return true;
		}

		return false;
	}

	rkit::Result AnoxCommandStackBase::Create(rkit::UniquePtr<AnoxCommandStackBase> &outStack, size_t maxCapacity, size_t maxLines)
	{
		rkit::UniquePtr<AnoxCommandStack> stack;
		RKIT_CHECK(rkit::New<AnoxCommandStack>(stack));

		RKIT_CHECK(stack->Init(maxCapacity, maxLines));

		outStack = std::move(stack);

		return rkit::ResultCode::kOK;
	}
}
