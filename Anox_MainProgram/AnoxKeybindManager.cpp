#include "AnoxKeybindManager.h"

#include "AnoxCommandRegistry.h"

#include "rkit/Core/CoroutineWrapper.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UniquePtr.h"

#include "rkit/Core/CoroutineCompiler.h"

namespace anox
{
	enum class AnoxKeybindType
	{
		kAsciiChar,
	};

	struct AnoxKeybind
	{
		rkit::String m_cmd;
	};

	// This class exists to handle Quake-style keybind registration
	class AnoxKeybindManager final : public AnoxKeybindManagerBase
	{
	public:
		AnoxKeybindManager();

		bool ResolveKeyCode(const rkit::StringSliceView &keyName, uint32_t &outKeyCode) const override;
		bool ResolveKeyName(uint32_t keyCode, char &outSingleChar, rkit::StringSliceView &outName) const override;

		rkit::Result Register(AnoxCommandRegistryBase &commandRegistry) override;

	private:
		AnoxKeybind *FindKeybind(uint32_t keyCode);
		const AnoxKeybind *FindKeybind(uint32_t keyCode) const;

		rkit::Result Cmd_Bind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::StringView> &args);
		rkit::Result Cmd_Unbind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::StringView> &args);

		rkit::StaticArray<AnoxKeybind, 128> m_asciiChars;
	};

	AnoxKeybindManager::AnoxKeybindManager()
	{
	}

	bool AnoxKeybindManager::ResolveKeyCode(const rkit::StringSliceView &keyName, uint32_t &outKeyCode) const
	{
		return false;
	}

	bool AnoxKeybindManager::ResolveKeyName(uint32_t keyCode, char &outSingleChar, rkit::StringSliceView &outName) const
	{
		return false;
	}

	rkit::Result AnoxKeybindManager::Cmd_Bind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::StringView> &args)
	{
		if (args.Count() >= 2)
		{
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxKeybindManager::Cmd_Unbind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::StringView> &args)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxKeybindManager::Register(AnoxCommandRegistryBase &commandRegistry)
	{
		rkit::coro::MethodWrapper<AnoxKeybindManager, AnoxRegisteredCommand::MethodStarter_t::Signature_t> wrapper(this);

		RKIT_CHECK(commandRegistry.RegisterCommand("bind", wrapper.Create<&AnoxKeybindManager::Cmd_Bind>()));
		RKIT_CHECK(commandRegistry.RegisterCommand("unbind", wrapper.Create<&AnoxKeybindManager::Cmd_Unbind>()));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxKeybindManagerBase::Create(rkit::UniquePtr<AnoxKeybindManagerBase> &outManager)
	{
		rkit::UniquePtr<AnoxKeybindManager> manager;
		RKIT_CHECK(rkit::New<AnoxKeybindManager>(manager));

		outManager = std::move(manager);

		return rkit::ResultCode::kOK;
	}
}
