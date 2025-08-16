#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rkit/Core/StaticArray.h"

namespace rkit { namespace mem {
	struct MemMapPageRange
	{
		void *m_base;
		size_t m_size;
	};

	enum class MemMapState
	{
		kReserved,
		kNoAccess,
		kRead,
		kReadWrite,

		kCount,
	};

	struct MemMapPageTypeProperties
	{
		uint8_t m_pageSizePO2;
		uint8_t m_allocationGranularityPO2;

		StaticArray<bool, static_cast<size_t>(MemMapState::kCount)> m_supportsState;
	};

	struct IMemMapDriver
	{
		typedef uint8_t MemPageSizePO2_t;

		virtual ~IMemMapDriver() {}

		virtual size_t GetNumPageTypes() const = 0;
		virtual const MemMapPageTypeProperties &GetPageType(size_t pageTypeIndex) const = 0;

		virtual bool CreateMapping(MemMapPageRange &outPageRange, size_t size, size_t pageTypeIndex, MemMapState initialState) = 0;
		virtual bool CreateMappingAt(MemMapPageRange &outPageRange, void *baseAddress, size_t size, size_t pageTypeIndex, MemMapState initialState) = 0;
		virtual bool ChangeState(void *startAddress, size_t size, size_t pageTypeIndex, MemMapState oldState, MemMapState newState) = 0;
		virtual void ReleaseMapping(const MemMapPageRange &pageRange) = 0;
	};
} }
