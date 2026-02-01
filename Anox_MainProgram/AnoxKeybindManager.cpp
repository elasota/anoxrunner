#include "AnoxKeybindManager.h"

#include "AnoxCommandRegistry.h"

#include "anox/AnoxGame.h"

#include "rkit/Core/CoroutineWrapper.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "rkit/Core/CoroutineCompiler.h"

namespace anox
{
	enum class AnoxKeybindType
	{
		kNone,

		kAsciiChar,
		kPCKeyboard,
	};

	enum class PCKeyboardKey
	{
		kF1,
		kF2,
		kF3,
		kF4,
		kF5,
		kF6,
		kF7,
		kF8,
		kF9,
		kF10,
		kF11,
		kF12,

		kTab,
		kEnter,
		kEscape,
		kSpace,
		kBackspace,
		kUpArrow,
		kDownArrow,
		kLeftArrow,
		kRightArrow,

		kAlt,
		kCtrl,
		kShift,

		kInsert,
		kDelete,
		kPageUp,
		kPageDown,
		kHome,
		kEnd,

		kMouse1,
		kMouse2,
		kMouse3,

		kPrintScreen,
		kScrollLock,

		kPause,
		kSemiColon,
		kMouseWheelUp,
		kMouseWheelDown,

		kNumPadSlash,
		kNumPadAsterisk,
		kNumPadMinus,
		kNumPadPlus,
		kNumPadEnter,
		kNumPadPeriod,

		kNumPad0,
		kNumPad1,
		kNumPad2,
		kNumPad3,
		kNumPad4,
		kNumPad5,
		kNumPad6,
		kNumPad7,
		kNumPad8,
		kNumPad9,

		kCount,
	};

	struct AnoxKeybind
	{
		rkit::ByteString m_cmd;
	};

	// This class exists to handle Quake-style keybind registration
	class AnoxKeybindManager final : public AnoxKeybindManagerBase
	{
	public:
		explicit AnoxKeybindManager(IAnoxGame &game);

		bool ResolveKeyCode(const rkit::ByteStringSliceView &keyName, KeyCode_t &outKeyCode) const override;
		bool ResolveKeyName(KeyCode_t keyCode, char &outSingleChar, rkit::AsciiStringSliceView &outName) const override;

		rkit::Result Register(AnoxCommandRegistryBase &commandRegistry) override;

	private:
		AnoxKeybind *FindKeybind(KeyCode_t keyCode);
		const AnoxKeybind *FindKeybind(KeyCode_t keyCode) const;

		static AnoxKeybindType GetKeycodeType(KeyCode_t keyCode);

		static char GetAsciiKeyCodeChar(KeyCode_t keyCode);
		static PCKeyboardKey GetPCKeyboardKey(KeyCode_t keyCode);

		static KeyCode_t CreateAsciiKeyCode(char c);
		static KeyCode_t CreatePCKeyboardKeyCode(PCKeyboardKey keyCode);

		rkit::Result Cmd_Bind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);
		rkit::Result Cmd_Unbind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);
		rkit::Result Cmd_Set(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);
		rkit::Result Cmd_Alias(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);

		static rkit::Result ConcatenateArgs(const rkit::ISpan<rkit::ByteStringView> &args, size_t firstArg, rkit::ByteString &storageStr, rkit::ByteStringView &outStrView);

		IAnoxGame &m_game;
		rkit::StaticArray<AnoxKeybind, 128> m_asciiCharKeys;
		rkit::StaticArray<AnoxKeybind, static_cast<size_t>(PCKeyboardKey::kCount)> m_pcKeyboardKeys;

		static const int kKeyCodeTypeShift = 30;
		static const int kKeyCodeTypeBits = 2;

		static const char *ms_pcKeyboardKeyNames[];
		static const char ms_denormalizedKeys[];
		static const char ms_normalizedKeys[];
	};

	const char *AnoxKeybindManager::ms_pcKeyboardKeyNames[] =
	{
		"f1",
		"f2",
		"f3",
		"f4",
		"f5",
		"f6",
		"f7",
		"f8",
		"f9",
		"f10",
		"f11",
		"f12",

		"tab",
		"enter",
		"escape",
		"space",
		"backspace",
		"uparrow",
		"downarrow",
		"leftarrow",
		"rightarrow",

		"alt",
		"ctrl",
		"shift",

		"ins",
		"del",
		"pgup",
		"pgdn",
		"home",
		"end",

		"mouse1",
		"mouse2",
		"mouse3",

		"printscreen",
		"scrollock",

		"pause",
		"semicolon",
		"mwheelup",
		"mwheeldown",

		// These use Q2 values
		"kp_slash",
		"kp_star",
		"kp_minus",
		"kp_plus",
		"kp_enter",
		"kp_del",

		"kp_ins",
		"kp_end",
		"kp_downarrow",
		"kp_pgdn",
		"kp_leftarrow",
		"kp_5",
		"kp_rightarrow",
		"kp_home",
		"kp_uparrow",
		"kp_pgup",
	};

	// These should only be used for compatibility with Q2-style key bindings,
	// they are not portable between keyboard layouts!
	const char AnoxKeybindManager::ms_denormalizedKeys[] = "!@#$%^&*()_+" "|"  "?`:";
	const char AnoxKeybindManager::ms_normalizedKeys[] =   "1234567890-=" "\\" "/~;";

	AnoxKeybindManager::AnoxKeybindManager(IAnoxGame &game)
		: m_game(game)
	{
		static_assert(sizeof(ms_normalizedKeys) == sizeof(ms_denormalizedKeys), "Normalized/denormalized key array mismatch");
		static_assert(sizeof(AnoxKeybindManager::ms_pcKeyboardKeyNames) / sizeof(AnoxKeybindManager::ms_pcKeyboardKeyNames[0]) == static_cast<size_t>(PCKeyboardKey::kCount), "Mismatched PC key count");
	}

	bool AnoxKeybindManager::ResolveKeyCode(const rkit::ByteStringSliceView &keyName, KeyCode_t &outKeyCode) const
	{
		if (keyName.Length() == 1)
		{
			uint8_t keyChar = keyName[0];

			if (keyChar >= 'A' && keyChar <= 'Z')
				keyChar += ('a' - 'A');
			else if (keyChar >= 32 && keyChar <= 126)
			{
				for (size_t i = 0; ms_denormalizedKeys[i] != 0; i++)
				{
					if (ms_denormalizedKeys[i] == keyChar)
					{
						keyChar = ms_normalizedKeys[i];
						break;
					}
				}

				if (keyChar == ';')
					outKeyCode = CreatePCKeyboardKeyCode(PCKeyboardKey::kSemiColon);
				else
					outKeyCode = CreateAsciiKeyCode(keyChar);
			}
			else
				return false;

			return true;
		}

		for (size_t i = 0; i < static_cast<size_t>(PCKeyboardKey::kCount); i++)
		{
			const char *candidateStr = ms_pcKeyboardKeyNames[i];
			if (keyName.EqualsNoCase(rkit::AsciiStringView::FromCString(candidateStr).RemoveEncoding()))
			{
				outKeyCode = CreatePCKeyboardKeyCode(static_cast<PCKeyboardKey>(i));
				return true;
			}
		}

		return false;
	}

	bool AnoxKeybindManager::ResolveKeyName(uint32_t keyCode, char &outSingleChar, rkit::AsciiStringSliceView &outName) const
	{
		return false;
	}

	rkit::Result AnoxKeybindManager::Cmd_Bind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 1)
		{
			rkit::log::Error(u8"Usage: bind <key> <parameters>");
			return rkit::ResultCode::kOK;
		}

		KeyCode_t keyCode = 0;
		if (!ResolveKeyCode(args[0], keyCode))
		{
			rkit::log::ErrorFmt(u8"Unknown key {}", args[0]);
			return rkit::ResultCode::kOK;
		}

		AnoxKeybind *keyBind = this->FindKeybind(keyCode);
		if (!keyBind)
			return rkit::ResultCode::kInternalError;

		if (args.Count() == 1)
		{
			rkit::log::LogInfoFmt(u8"Keybind for '{}': {}", args[0], keyBind->m_cmd);
		}
		else
		{
			rkit::ByteString storageStr;
			rkit::ByteStringView concatenated;

			RKIT_CHECK(ConcatenateArgs(args, 1, storageStr, concatenated));

			RKIT_CHECK(keyBind->m_cmd.Set(concatenated));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxKeybindManager::Cmd_Unbind(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 1)
		{
			rkit::log::Error(u8"Usage: unbind <key>");
			return rkit::ResultCode::kOK;
		}

		KeyCode_t keyCode = 0;
		if (!ResolveKeyCode(args[0], keyCode))
		{
			rkit::log::ErrorFmt(u8"Unknown key {}", args[0]);
			return rkit::ResultCode::kOK;
		}

		AnoxKeybind *keyBind = this->FindKeybind(keyCode);
		if (!keyBind)
			return rkit::ResultCode::kInternalError;

		keyBind->m_cmd.Clear();

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxKeybindManager::Cmd_Set(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 2)
		{
			rkit::log::Error(u8"Usage: set <variable> <value> [u | s]");
			return rkit::ResultCode::kOK;
		}

		AnoxCommandRegistryBase *cmdRegistry = m_game.GetCommandRegistry();

		if (args.Count() == 3)
		{
			return rkit::ResultCode::kNotYetImplemented;
		}
		else if (args.Count() == 2)
		{
			const AnoxRegisteredConsoleVar *cvar = cmdRegistry->FindConsoleVar(args[0]);

			bool setOK = false;
			if (cvar)
			{
				RKIT_CHECK(cmdRegistry->TrySetCVar(*cvar, args[1], setOK));
			}

			if (!setOK)
				rkit::log::ErrorFmt(u8"Failed to set '{0}' to '{1}'", args[0], args[1]);

			return rkit::ResultCode::kOK;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxKeybindManager::Cmd_Alias(AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 2)
		{
			rkit::log::Error(u8"Usage: alias <name> <commands>");
			return rkit::ResultCode::kOK;
		}

		rkit::ByteString storageStr;
		rkit::ByteStringView aliasCommand;

		RKIT_CHECK(ConcatenateArgs(args, 1, storageStr, aliasCommand));

		RKIT_CHECK(m_game.GetCommandRegistry()->RegisterAlias(args[0], aliasCommand));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxKeybindManager::ConcatenateArgs(const rkit::ISpan<rkit::ByteStringView> &args, size_t firstArg, rkit::ByteString &storageStr, rkit::ByteStringView &outStrView)
	{
		if (args.Count() <= firstArg)
		{
			outStrView = rkit::ByteStringView();
		}
		else if (args.Count() == firstArg + 1)
		{
			outStrView = args[firstArg];
		}
		else
		{
			rkit::Vector<uint8_t> concatenatedCmd;
			for (size_t i = firstArg; i < args.Count(); i++)
			{
				if (concatenatedCmd.Count() > 0)
				{
					RKIT_CHECK(concatenatedCmd.Append(' '));
				}

				rkit::ByteString tempStr;
				rkit::ByteStringView token = args[i];

				if (AnoxCommandRegistryBase::RequiresEscape(token))
				{
					RKIT_CHECK(AnoxCommandRegistryBase::EscapeToken(tempStr, token));
					token = tempStr;
				}

				RKIT_CHECK(concatenatedCmd.Append(token.ToSpan()));
			}

			RKIT_CHECK(storageStr.Set(concatenatedCmd.ToSpan()));
			outStrView = storageStr;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxKeybindManager::Register(AnoxCommandRegistryBase &commandRegistry)
	{
		rkit::coro::MethodWrapper<AnoxKeybindManager, AnoxRegisteredCommand::MethodStarter_t::Signature_t> wrapper(this);

		RKIT_CHECK(commandRegistry.RegisterCommand(u8"bind", wrapper.Create<&AnoxKeybindManager::Cmd_Bind>()));
		RKIT_CHECK(commandRegistry.RegisterCommand(u8"unbind", wrapper.Create<&AnoxKeybindManager::Cmd_Unbind>()));
		RKIT_CHECK(commandRegistry.RegisterCommand(u8"alias", wrapper.Create<&AnoxKeybindManager::Cmd_Alias>()));
		RKIT_CHECK(commandRegistry.RegisterCommand(u8"set", wrapper.Create<&AnoxKeybindManager::Cmd_Set>()));

		return rkit::ResultCode::kOK;
	}

	AnoxKeybind *AnoxKeybindManager::FindKeybind(KeyCode_t keyCode)
	{
		AnoxKeybindType keyBindType = GetKeycodeType(keyCode);

		switch (keyBindType)
		{
		case AnoxKeybindType::kAsciiChar:
			return &m_asciiCharKeys[GetAsciiKeyCodeChar(keyCode)];
		case AnoxKeybindType::kPCKeyboard:
			return &m_pcKeyboardKeys[static_cast<size_t>(GetPCKeyboardKey(keyCode))];
		default:
			return nullptr;
		}
	}

	const AnoxKeybind *AnoxKeybindManager::FindKeybind(KeyCode_t keyCode) const
	{
		return const_cast<AnoxKeybindManager *>(this)->FindKeybind(keyCode);
	}

	AnoxKeybindType AnoxKeybindManager::GetKeycodeType(KeyCode_t keyCode)
	{
		return static_cast<AnoxKeybindType>((keyCode >> kKeyCodeTypeShift) & ((1 << kKeyCodeTypeBits) - 1));;
	}

	char AnoxKeybindManager::GetAsciiKeyCodeChar(KeyCode_t keyCode)
	{
		RKIT_ASSERT(GetKeycodeType(keyCode) == AnoxKeybindType::kAsciiChar);
		return static_cast<char>(keyCode & 0x7f);
	}

	PCKeyboardKey AnoxKeybindManager::GetPCKeyboardKey(KeyCode_t keyCode)
	{
		RKIT_ASSERT(GetKeycodeType(keyCode) == AnoxKeybindType::kPCKeyboard);
		return static_cast<PCKeyboardKey>(keyCode & 0xff);
	}

	AnoxKeybindManager::KeyCode_t AnoxKeybindManager::CreateAsciiKeyCode(char c)
	{
		KeyCode_t keyCode = static_cast<uint8_t>(c);
		keyCode |= static_cast<KeyCode_t>(AnoxKeybindType::kAsciiChar) << kKeyCodeTypeShift;
		return keyCode;
	}

	AnoxKeybindManager::KeyCode_t AnoxKeybindManager::CreatePCKeyboardKeyCode(PCKeyboardKey pcKey)
	{
		KeyCode_t keyCode = static_cast<KeyCode_t>(pcKey);
		keyCode |= static_cast<KeyCode_t>(AnoxKeybindType::kPCKeyboard) << kKeyCodeTypeShift;
		return keyCode;
	}

	rkit::Result AnoxKeybindManagerBase::Create(rkit::UniquePtr<AnoxKeybindManagerBase> &outManager, IAnoxGame &game)
	{
		rkit::UniquePtr<AnoxKeybindManager> manager;
		RKIT_CHECK(rkit::New<AnoxKeybindManager>(manager, game));

		outManager = std::move(manager);

		return rkit::ResultCode::kOK;
	}
}
