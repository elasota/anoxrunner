#include "Sha2Calculator.h"

namespace rkit { namespace utils
{
	const uint32_t Sha256Calculator::kConstants[64] =
	{
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	Sha256State Sha256Calculator::CreateState() const
	{
		Sha256State result;
		result.m_state[0] = 0x6a09e667;
		result.m_state[1] = 0xbb67ae85;
		result.m_state[2] = 0x3c6ef372;
		result.m_state[3] = 0xa54ff53a;
		result.m_state[4] = 0x510e527f;
		result.m_state[5] = 0x9b05688c;
		result.m_state[6] = 0x1f83d9ab;
		result.m_state[7] = 0x5be0cd19;

		return result;
	}

	void Sha256Calculator::AddInputChunk(Sha256State &state, const Sha256InputChunk &inputChunk) const
	{
		uint32_t w[64];

		for (int i = 0; i < 16; i++)
			w[i] = inputChunk.m_data[i];

		for (int i = 16; i < 64; i++)
		{
			const uint32_t s0 = RotR32(w[i - 15], 7) ^ RotR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
			const uint32_t s1 = RotR32(w[i - 2], 17) ^ RotR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}

		uint32_t a = state.m_state[0];
		uint32_t b = state.m_state[1];
		uint32_t c = state.m_state[2];
		uint32_t d = state.m_state[3];
		uint32_t e = state.m_state[4];
		uint32_t f = state.m_state[5];
		uint32_t g = state.m_state[6];
		uint32_t h = state.m_state[7];

		for (int i = 0; i < 64; i++)
		{
			const uint32_t s1 = RotR32(e, 6) ^ RotR32(e, 11) ^ RotR32(e, 25);
			const uint32_t ch = (e & f) ^ ((~e) & g);

			const uint32_t t1 = h + s1 + ch + kConstants[i] + w[i];

			const uint32_t s0 = RotR32(a, 2) ^ RotR32(a, 13) ^ RotR32(a, 22);
			const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			const uint32_t t2 = s0 + maj;

			h = g;
			g = f;
			f = e;
			e = d + t1;
			d = c;
			c = b;
			b = a;
			a = t1 + t2;
		}

		state.m_state[0] += a;
		state.m_state[1] += b;
		state.m_state[2] += c;
		state.m_state[3] += d;
		state.m_state[4] += e;
		state.m_state[5] += f;
		state.m_state[6] += g;
		state.m_state[7] += h;
	}

	Sha256DigestBytes Sha256Calculator::FlushToBytes(const Sha256State &state) const
	{
		Sha256DigestBytes result;
		for (int wi = 0; wi < 8; wi++)
		{
			for (int bi = 0; bi < 4; bi++)
			{
				const uint8_t b = static_cast<uint8_t>((state.m_state[wi] >> (24 - bi * 8)) & 0xffu);
				result.m_data[wi * 4 + bi] = b;
			}
		}

		return result;
	}

	Sha256StreamingState Sha256Calculator::CreateStreamingState() const
	{
		Sha256StreamingState result;
		result.m_state = CreateState();

		for (int i = 0; i < 16; i++)
			result.m_inputChunk.m_data[i] = 0;

		result.m_length = 0;

		return result;
	}

	void Sha256Calculator::AppendStreamingState(Sha256StreamingState &stateRef, const void *data, size_t size) const
	{
		Sha256InputChunk inputChunk = stateRef.m_inputChunk;
		Sha256State state = stateRef.m_state;
		uint64_t length = stateRef.m_length;

		for (size_t i = 0; i < size; i++)
		{
			uint8_t b = static_cast<const uint8_t *>(data)[i];

			int byteInWord = static_cast<int>(i % 4);
			int wordIndex = static_cast<int>((i / 4) % 16);

			inputChunk.m_data[wordIndex] |= static_cast<uint32_t>(b) << (24 - byteInWord * 8);

			length++;

			if (length % 64 == 0)
			{
				AddInputChunk(state, inputChunk);

				for (int wi = 0; wi < 16; wi++)
					inputChunk.m_data[wi] = 0;

				length = 0;
			}
		}

		stateRef.m_inputChunk = inputChunk;
		stateRef.m_length = length;
		stateRef.m_state = state;
	}

	void Sha256Calculator::FinalizeStreamingState(Sha256StreamingState &state) const
	{
		const size_t kMaxPadding = 8 + 2 + 64;
		uint8_t padBytes[kMaxPadding];
		size_t padLength = 0;

		uint64_t codedLength = state.m_length;

		for (int i = 0; i < 8; i++)
		{
			padLength++;
			padBytes[kMaxPadding - padLength] = static_cast<uint8_t>(codedLength & 0xffu);
			codedLength >>= 8;
		}

		padLength++;
		padBytes[kMaxPadding - padLength] = 0;

		while ((state.m_length + padLength) % 64 != 0)
		{
			padLength++;
			padBytes[kMaxPadding - padLength] = 0;
		}

		padBytes[kMaxPadding - padLength] = 0x80;

		AppendStreamingState(state, padBytes + kMaxPadding - padLength, padLength);
	}

	Sha256DigestBytes Sha256Calculator::SimpleHashBuffer(const void *data, size_t size) const
	{
		Sha256StreamingState sstate = CreateStreamingState();

		AppendStreamingState(sstate, data, size);
		FinalizeStreamingState(sstate);

		return FlushToBytes(sstate.m_state);
	}

	uint32_t Sha256Calculator::RotR32(uint32_t v, int bits)
	{
		const uint32_t lowBits = v >> bits;
		const uint32_t highBits = (v << (32 - bits)) & static_cast<uint32_t>(0xffffffffu);

		return highBits | lowBits;
	}
} } // rkit::utils
