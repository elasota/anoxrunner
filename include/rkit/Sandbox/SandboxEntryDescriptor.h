#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Sandbox/SandboxPtr.h"

namespace rkit::sandbox
{
	struct ExportTable
	{
		SandboxPtr<Utf8Char_t> m_name;
	};

	// The main entry point for the sandbox, contains address size information, needed to parse
	// the import/export tables
	struct BaseEntryDescriptor
	{
		uint16_t m_dataAddressSize;
		uint16_t m_functionPtrSize;

		uint16_t m_exportDescriptorSize;
		uint16_t m_unused;

		uint32_t m_numSysCalls;
		uint32_t m_numExports;
	};

	struct SysCallStub
	{
		uint32_t m_sysCallID;
	};

	struct SysCallDescriptor
	{
		const Utf8Char_t *m_chars;
		uintptr_t m_length;
	};

	struct ExportDescriptor
	{
		typedef void (*ExportFunctionPtr_t)(void *ioContext) noexcept;

		const Utf8Char_t *m_chars;
		uintptr_t m_length;
		ExportFunctionPtr_t m_functionAddress;
	};

	struct EntryDescriptor
	{
		BaseEntryDescriptor m_baseDescriptor;

		const SysCallDescriptor *m_sysCallDescriptorTable;
		const SysCallStub *m_sysCallStubsTable;

		const ExportDescriptor *m_exportDescriptorTable;
		const ExportDescriptor::ExportFunctionPtr_t *m_exportFirstFunctionAddress;
	};
}
