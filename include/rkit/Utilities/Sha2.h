#pragma once

#include <cstdint>
#include <cstddef>

namespace rkit { namespace utils
{
	struct Sha256DigestBytes
	{
		static const size_t kSize = 32;

		uint8_t m_data[kSize];
	};

	struct Sha256State
	{
		uint32_t m_state[8];
	};

	struct Sha256InputChunk
	{
		uint32_t m_data[16];
	};

	struct Sha256StreamingState
	{
		Sha256State m_state;

		Sha256InputChunk m_inputChunk;
		uint64_t m_length;
	};

	struct ISha256Calculator
	{
		virtual Sha256State CreateState() const = 0;
		virtual void AddInputChunk(Sha256State &state, const Sha256InputChunk &inputChunk) const = 0;
		virtual Sha256DigestBytes FlushToBytes(const Sha256State &state) const = 0;

		virtual Sha256StreamingState CreateStreamingState() const = 0;
		virtual void AppendStreamingState(Sha256StreamingState &state, const void *data, size_t size) const = 0;
		virtual void FinalizeStreamingState(Sha256StreamingState &state) const = 0;

		virtual Sha256DigestBytes SimpleHashBuffer(const void *data, size_t size) const = 0;
	};
} } // rkit::utils
