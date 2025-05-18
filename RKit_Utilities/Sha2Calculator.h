#pragma once

#include "rkit/Utilities/Sha2.h"

namespace rkit { namespace utils
{
	class Sha256Calculator final : public ISha256Calculator
	{
	public:
		Sha256State CreateState() const override;
		void AddInputChunk(Sha256State &state, const Sha256InputChunk &inputChunk) const override;
		Sha256DigestBytes FlushToBytes(const Sha256State &state) const override;

		Sha256StreamingState CreateStreamingState() const override;
		void AppendStreamingState(Sha256StreamingState &state, const void *data, size_t size) const override;
		void FinalizeStreamingState(Sha256StreamingState &state) const override;

		Sha256DigestBytes SimpleHashBuffer(const void *data, size_t size) const override;

	private:
		static const uint32_t kConstants[64];

		static uint32_t RotR32(uint32_t v, int bits);
	};
} } // rkit::utils
