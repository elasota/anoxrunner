#include "ScriptManager.h"

#include "ScriptContext.h"
#include "ScriptEnvironment.h"

#include "anox/Data/APEScript.h"

#include "anox/Label.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/LogDriver.h"

#include "GameObjects/ScriptWindowInstance.h"
#include "AnoxWorldObjectFactory.h"

#include "anox/Game/APECommandDispatcher.generated.h"

namespace anox::game
{
	struct ScriptPackage;

	struct ScriptWindow
	{
		Label m_windowID;
		rkit::ConstSpan<uint8_t> m_commandStream;
		const ScriptPackage *m_package = nullptr;
	};

	struct ScriptExpression
	{
		ScriptOperator m_op = ScriptOperator::Invalid;
		ScriptOperandType m_leftOpType = ScriptOperandType::Invalid;
		ScriptOperandType m_rightOpType = ScriptOperandType::Invalid;

		uint32_t m_leftValue = 0;
		uint32_t m_rightValue = 0;
	};

	struct ScriptSwitchCommand
	{
		uint8_t m_opcode = 0;
		uint32_t m_fmtValue = 0;
		uint32_t m_strValue = 0;	// Also instr jump count for if/jump instrs
		ScriptExprValue m_exprValue;
	};

	struct ScriptSwitch
	{
		Label m_switchID;
		rkit::ConstSpan<ScriptSwitchCommand> m_commands;
		const ScriptPackage *m_package = nullptr;
	};

	struct ScriptPackage
	{
		rkit::Vector<ScriptWindow> m_windows;
		rkit::Vector<ScriptSwitch> m_switches;
		rkit::Vector<ScriptExpression> m_expressions;
		rkit::Vector<rkit::ByteString> m_strings;
		rkit::Vector<rkit::Span<const uint32_t>> m_operandLists;

		rkit::Vector<uint32_t> m_allOperands;
		rkit::Vector<uint8_t> m_allWindowCommands;
		rkit::Vector<ScriptSwitchCommand> m_allSwitchCommands;

		rkit::Vector<rkit::data::ContentID> m_materialContentIDs;
		rkit::Vector<uint32_t> m_materialNameLookups;
		rkit::HashMap<rkit::ByteString, uint32_t> m_wildcardLookups;
	};

	class ScriptLayerInstance
	{
	public:
		rkit::Result AddPackage(rkit::UniquePtr<ScriptPackage> &&package);

		void UnloadAll();

		const ScriptWindow *FindWindow(const Label &label) const;
		const ScriptWindow *FindWindowPrehashed(const Label &label, rkit::HashValue_t hashValue) const;
		const ScriptSwitch *FindSwitch(const Label &label) const;
		const ScriptSwitch *FindSwitchPrehashed(const Label &label, rkit::HashValue_t hashValue) const;

	private:

		rkit::Vector<rkit::UniquePtr<ScriptPackage>> m_packages;
		rkit::HashMap<Label, const ScriptWindow *> m_windows;
		rkit::HashMap<Label, const ScriptSwitch *> m_switches;
	};

	class ScriptEnvironmentImpl final : public rkit::OpaqueImplementation<ScriptEnvironment>
	{
	public:
		explicit ScriptEnvironmentImpl(ScriptManagerImpl &scriptManager);

		rkit::ResultCoroutine StartSequence(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world);

		bool TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, uint32_t exprID, int depth) const;
		bool TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, const ScriptExprValue &exprValue, int depth) const;
		bool TryEvaluateFloatScriptOperand(float &outValue, const ScriptPackage &pkg, const ScriptOperandType opType, uint32_t value, int depth) const;

	private:
		class WindowCommandParserImpl final : public APEWindowCommandParser
		{
		public:
			WindowCommandParserImpl() = delete;
			explicit WindowCommandParserImpl(const ScriptPackage &package);

		private:
			rkit::Result ParseString(uint32_t index, rkit::ByteStringView &value) override;
			rkit::Result ParseOperandList(uint32_t index, ScriptOperandList &value) override;

			const ScriptPackage &m_package;
		};

		rkit::ResultCoroutine ExecuteWindowCommands(rkit::ICoroThread &thread, ScriptWindowInstance &windowInstance, const ScriptWindow &window);
		rkit::ResultCoroutine ExecuteSwitchCommands(rkit::ICoroThread &thread, const ScriptPackage *pkg, rkit::Span<const ScriptSwitchCommand> cmds, int &loopCounter, int depth);

		bool TryResolveOptionalString(rkit::ByteString &outBStr, const ScriptPackage &pkg, uint32_t strID) const;

		rkit::Result ClearFloatVariable(const rkit::ByteStringView &bstr);
		rkit::Result SetFloatVariable(rkit::ByteString &&bstr, float value);

		bool ParseLabel(Label &outLabel, const rkit::ByteStringView bstr);

		static constexpr int kLoopCounterLimit = 1000;

		ScriptManagerImpl &m_scriptManager;

		rkit::HashMap<rkit::ByteString, float> m_floatVariables;
		rkit::HashMap<rkit::ByteString, rkit::Vector<float>> m_floatArrays;

		rkit::Vector<ScriptWindowInstance*> m_activeWindows;
	};

	class ScriptContextImpl final : public rkit::OpaqueImplementation<ScriptContext>
	{
	};

	class ScriptManagerImpl final : public rkit::OpaqueImplementation<ScriptManager>
	{
	public:
		rkit::Result LoadScriptPackage(ScriptManager::ScriptLayer layer, const rkit::Span<const uint8_t> &contents);

		ScriptLayerInstance &GetLayer(ScriptManager::ScriptLayer layer);
		const ScriptLayerInstance &GetLayer(ScriptManager::ScriptLayer layer) const;

		void FindScript(const anox::Label &label, const ScriptWindow *&outWindow, const ScriptSwitch *&outSwitch) const;
		void FindScriptPrehashed(const anox::Label &label, rkit::HashValue_t hashValue, const ScriptWindow *&outWindow, const ScriptSwitch *&outSwitch) const;

		void FindWindow(const anox::Label &label, const ScriptWindow *&outWindow) const;
		void FindWindowPrehashed(const anox::Label &label, rkit::HashValue_t hashValue, const ScriptWindow *&outWindow) const;

		void FindSwitch(const anox::Label &label, const ScriptSwitch *&outSwitch) const;
		void FindSwitchPrehashed(const anox::Label &label, rkit::HashValue_t hashValue, const ScriptSwitch *&outSwitch) const;

		rkit::Result CreateScriptEnvironment(rkit::UniquePtr<ScriptEnvironment> &outScriptEnvironment);

	private:
		void FindScriptInternal(const anox::Label &label, const ScriptWindow **outWindow, const ScriptSwitch **outSwitch) const;
		void FindScriptPrehashedInternal(const anox::Label &label, rkit::HashValue_t hashValue, const ScriptWindow **outWindow, const ScriptSwitch **outSwitch) const;

		static rkit::Result LoadScriptCommand(ScriptSwitchCommand &outCommand, const data::ape::SwitchCommand &inCommand);
		static rkit::Result LoadScriptExpression(ScriptExpression &outExpr, const data::ape::Expression &inExpr);

		static constexpr size_t kNumScriptLayers = static_cast<size_t>(ScriptManager::ScriptLayer::kCount);

		rkit::StaticArray<ScriptLayerInstance, kNumScriptLayers> m_layers;
	};

	ScriptEnvironmentImpl::WindowCommandParserImpl::WindowCommandParserImpl(const ScriptPackage &package)
		: m_package(package)
	{
	}

	rkit::Result ScriptEnvironmentImpl::WindowCommandParserImpl::ParseString(uint32_t index, rkit::ByteStringView &value)
	{
		if (index >= m_package.m_strings.Count())
			RKIT_THROW(rkit::ResultCode::kDataError);

		value = m_package.m_strings[index];
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptEnvironmentImpl::WindowCommandParserImpl::ParseOperandList(uint32_t index, ScriptOperandList &value)
	{
		if (index >= m_package.m_operandLists.Count())
			RKIT_THROW(rkit::ResultCode::kDataError);

		value = m_package.m_operandLists[index];
		RKIT_RETURN_OK;
	}

	ScriptEnvironmentImpl::ScriptEnvironmentImpl(ScriptManagerImpl &scriptManager)
		: m_scriptManager(scriptManager)
	{
	}

	rkit::ResultCoroutine ScriptEnvironmentImpl::StartSequence(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world)
	{
		const ScriptWindow *window = nullptr;
		const ScriptSwitch *sw = nullptr;

		m_scriptManager.FindScript(label, window, sw);

		if (window)
		{
			// Do nothing if the window is already active
			for (ScriptWindowInstance* windowPtr : m_activeWindows)
			{
				if (windowPtr->GetWindowID() == label)
					CORO_RETURN_OK;
			}

			ScriptWindowInstance *instance = nullptr;
			CORO_CHECK(WorldObjectFactory::CreateDynamic<ScriptWindowInstance>(world, instance));

			CORO_CHECK(m_activeWindows.Append(instance));
			instance->Initialize(label);

			CORO_CHECK(co_await ExecuteWindowCommands(thread, *instance, *window));

			const Label startSwitch = instance->GetStartSwitch();

			sw = nullptr;
			m_scriptManager.FindSwitch(startSwitch, sw);
			// Fall through and run the start switch
		}

		if (sw)
		{
			rkit::ConstSpan<ScriptSwitchCommand> cmds = sw->m_commands;

			const ScriptPackage &pkg = *sw->m_package;

			int loopCounter = 0;
			CORO_CHECK(co_await ExecuteSwitchCommands(thread, &pkg, cmds, loopCounter, 0));
		}

		CORO_RETURN_OK;
	}

	bool ScriptEnvironmentImpl::TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, uint32_t exprID, int depth) const
	{
		if (depth >= 64)
		{
			rkit::log::Error(u8"Expression stack overflow");
			return false;
		}

		if (exprID >= pkg.m_expressions.Count())
		{
			rkit::log::Error(u8"Invalid expression ID");
			return false;
		}

		const ScriptExpression &expr = pkg.m_expressions[exprID];

		float leftValue = 0.f;
		if (!TryEvaluateFloatScriptOperand(leftValue, pkg, expr.m_leftOpType, expr.m_leftValue, depth + 1))
			return false;

		float rightValue = 0.f;
		if (!TryEvaluateFloatScriptOperand(rightValue, pkg, expr.m_rightOpType, expr.m_rightValue, depth + 1))
			return false;

		switch (expr.m_op)
		{
		case ScriptOperator::Or:
			outValue = ((leftValue != 0.f) || (rightValue != 0.f)) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::And:
			outValue = ((leftValue != 0.f) && (rightValue != 0.f)) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Xor:
			outValue = ((leftValue != 0.f) != (rightValue != 0.f)) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Gt:
			outValue = (leftValue > rightValue) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Lt:
			outValue = (leftValue < rightValue) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Ge:
			outValue = (leftValue >= rightValue) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Le:
			outValue = (leftValue <= rightValue) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Eq:
			outValue = (leftValue == rightValue) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Neq:
			outValue = (leftValue != rightValue) ? 1.0f : 0.0f;
			return true;
		case ScriptOperator::Add:
			outValue = leftValue + rightValue;
			return true;
		case ScriptOperator::Sub:
			outValue = leftValue - rightValue;
			return true;
		case ScriptOperator::Mul:
			outValue = leftValue * rightValue;
			return true;
		case ScriptOperator::Div:
			if (rightValue == 0.f)
			{
				rkit::log::Error(u8"Division by zero");
				return false;
			}
			outValue = leftValue / rightValue;
			return true;
		default:
			rkit::log::Error(u8"Invalid expression op");
			return false;
		}
	}


	bool ScriptEnvironmentImpl::TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr, int depth) const
	{
		switch (expr.m_exprType)
		{
		case ScriptExprType::Empty:
			return false;
		case ScriptExprType::FloatLiteral:
			RKIT_STATIC_ASSERT(sizeof(outValue) == 4);
			RKIT_STATIC_ASSERT(sizeof(expr.m_index) == 4);
			memcpy(&outValue, &expr.m_index, 4);
			return true;
		case ScriptExprType::FloatExpression:
			return TryEvaluateFloatScriptExpr(outValue, pkg, expr.m_index, depth);
		default:
			rkit::log::Error(u8"Invalid expression type where float expression was expected");
			return false;
		}
	}

	bool ScriptEnvironmentImpl::TryEvaluateFloatScriptOperand(float &outValue, const ScriptPackage &pkg, const ScriptOperandType opType, uint32_t value, int depth) const
	{
		switch (opType)
		{
		case ScriptOperandType::Literal:
			memcpy(&outValue, &value, 4);
			return true;
		case ScriptOperandType::Expression:
			return TryEvaluateFloatScriptExpr(outValue, pkg, value, depth);
		case ScriptOperandType::Variable:
			rkit::log::Error(u8"Not yet implemented");
			return false;
		default:
			rkit::log::Error(u8"Invalid operand type");
			return false;
		}
	}

	rkit::ResultCoroutine ScriptEnvironmentImpl::ExecuteWindowCommands(rkit::ICoroThread &thread, ScriptWindowInstance &windowInstance, const ScriptWindow &window)
	{
		const ScriptPackage &pkg = *window.m_package;
		rkit::ConstSpan<uint8_t> cmdStream = window.m_commandStream;

		WindowCommandParserImpl cmdParser(pkg);

		while (cmdStream.Count() > 0)
		{
			size_t consumed = 0;
			CORO_CHECK(ape::HandleCommand(cmdStream.Ptr(), cmdStream.Count(), consumed, cmdParser, windowInstance, this->Base(), pkg));

			cmdStream = cmdStream.SubSpan(consumed);
		}

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine ScriptEnvironmentImpl::ExecuteSwitchCommands(rkit::ICoroThread &thread, const ScriptPackage *pkg, rkit::Span<const ScriptSwitchCommand> cmds, int &loopCounter, int depth)
	{
		size_t ip = 0;
		while (ip < cmds.Count())
		{
			const ScriptSwitchCommand &cmd = cmds[ip++];
			switch (cmd.m_opcode)
			{
			case 0:	// noop
				break;
			//case 1:	// if
			case 2:	// setfloat
				{
					rkit::ByteString varName;
					if (TryResolveOptionalString(varName, *pkg, cmd.m_strValue))
					{
						if (cmd.m_exprValue.m_exprType == ScriptExprType::Empty)
						{
							ClearFloatVariable(varName);
						}
						else
						{
							float v = 0.f;
							if (TryEvaluateFloatScriptExpr(v, *pkg, cmd.m_exprValue, 0))
							{
								CORO_CHECK(SetFloatVariable(std::move(varName), v));
							}
						}
					}
				}
				break;
			//case 2:	// setfloat
			//case 3:	// setstring
			case 4:	// goto
				{
					rkit::ByteString labelStr;
					Label destLabel;
					if (!TryResolveOptionalString(labelStr, *pkg, cmd.m_strValue) || !ParseLabel(destLabel, labelStr))
					{
						rkit::log::Error(u8"Couldn't resolve goto destination");
						CORO_RETURN_OK;
					}
					else
					{
						// return
						if (destLabel == Label(0, 0))
							CORO_RETURN_OK;

						const ScriptSwitch *sw = nullptr;
						m_scriptManager.FindSwitch(destLabel, sw);

						if (!sw)
						{
							rkit::log::Error(u8"Couldn't resolve goto switch");
							CORO_RETURN_OK;
						}
						else
						{
							if (loopCounter == kLoopCounterLimit)
							{
								rkit::log::Error(u8"Loop counter limit exceeded");
								CORO_RETURN_OK;
							}

							loopCounter++;
							pkg = sw->m_package;
							cmds = sw->m_commands;
							ip = 0;
						}
					}
				}
				break;
			//case 5:	// gosub
			//case 6:	// console
			//case 7:	// echo
			//case 8:	// target
			//case 9:	// pathtarget
			//case 10:	// extern
			//case 11:	// while
			//case 12:	// playambient
			//case 13:	// loopambient
			//case 14:	// stopambient
			//case 15:	// playscene
			//case 16:	// loopscene
			//case 17:	// stopscene
			//case 18:	// chainscripts
			//case 19:	// closewindow
			//case 20:	// loadape
			//case 21:	// setfocus
			//case 22:	// jump
			//case 23:	// rjump
			default:
				rkit::log::Error(u8"Unimplemented APE opcode");
				break;
			};
		}

		CORO_RETURN_OK;
	}

	bool ScriptEnvironmentImpl::TryResolveOptionalString(rkit::ByteString &outBStr, const ScriptPackage &pkg, uint32_t strID) const
	{
		if (strID == 0)
			return false;

		strID--;
		if (strID >= pkg.m_strings.Count())
			return false;

		outBStr = pkg.m_strings[strID];
		return true;
	}

	rkit::Result ScriptEnvironmentImpl::ClearFloatVariable(const rkit::ByteStringView &bstr)
	{
		m_floatVariables.Remove(bstr);
		RKIT_RETURN_OK;
	}

	rkit::Result ScriptEnvironmentImpl::SetFloatVariable(rkit::ByteString &&bstr, float value)
	{
		for (uint8_t ch : bstr.ToSpan())
		{
			if (ch == '[')
				RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
		}

		if (value == 0.0f)
		{
			m_floatVariables.Remove(bstr);
			RKIT_RETURN_OK;
		}
		else
			return m_floatVariables.Set(std::move(bstr), value);
	}

	bool ScriptEnvironmentImpl::ParseLabel(Label &outLabel, const rkit::ByteStringView bstr)
	{
		rkit::Span<const uint8_t> bytes = bstr.ToSpan();
		size_t splitIndex = 0;
		while (splitIndex < bytes.Count() && bstr[splitIndex] != ':')
			splitIndex++;

		if (splitIndex == bytes.Count())
			return false;

		const rkit::Span<const uint8_t> halfBytes[2] = { bytes.SubSpan(0, splitIndex), bytes.SubSpan(splitIndex + 1) };
		uint32_t halfValues[2] = { };

		for (int half = 0; half < 2; half++)
		{
			rkit::Span<const uint8_t> bspan = halfBytes[half];
			uint32_t value = 0;

			const uint32_t upperLimit = std::numeric_limits<uint32_t>::max() / 10u;

			if (bspan.Count() == 0)
				return false;

			for (uint8_t ch : bspan)
			{
				if (ch < '0' || ch > '9')
					return false;

				if (value > upperLimit)
					return false;

				value *= 10u;
				value += static_cast<uint32_t>(ch - '0');
			}

			halfValues[half] = value;
		}

		if (!Label::IsValid(halfValues[0], halfValues[1]))
			return false;

		outLabel = Label(halfValues[0], halfValues[1]);
		return true;
	}

	rkit::Result ScriptManagerImpl::LoadScriptPackage(ScriptManager::ScriptLayer layer, const rkit::Span<const uint8_t> &contents)
	{
		rkit::ReadOnlyMemoryStream stream(contents);

		anox::data::ape::APEScriptCatalog catalog;
		RKIT_CHECK(stream.ReadOneBinary(catalog));

		const uint32_t numStrings = catalog.m_numStrings.Get();
		const uint32_t numExprs = catalog.m_numExprs.Get();
		const uint32_t numOperandLists = catalog.m_numOperandLists.Get();
		const uint32_t numWindows = catalog.m_numWindows.Get();
		const uint32_t numSwitches = catalog.m_numSwitches.Get();

		const uint32_t numMaterialWildcards = catalog.m_numMaterialWildcards.Get();
		const uint32_t numMaterialNames = catalog.m_numMaterialNames.Get();

		const uint32_t numMaterialContentIDs = catalog.m_numMaterialContentIDs.Get();
		const uint32_t numMaterialNameLookups = catalog.m_numMaterialNameLookups.Get();
		const uint32_t numMaterialWildcardLookups = catalog.m_numMaterialWildcardLookups.Get();

		if (numMaterialWildcards != 0 || numMaterialNames != 0)
		{
			// These are intermediates and should not be in final data
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::Vector<rkit::ByteStringConstructionBuffer> strCBufs;
		rkit::Vector<rkit::ByteString> strings;

		RKIT_CHECK(strCBufs.Resize(numStrings));
		RKIT_CHECK(strings.Resize(numStrings));

		// Read lengths
		for (size_t i = 0; i < numStrings; i++)
		{
			rkit::endian::LittleUInt32_t strLength;
			RKIT_CHECK(stream.ReadOneBinary(strLength));
			RKIT_CHECK(strCBufs[i].Allocate(strLength.Get()));
		}

		// Read strings
		{
			auto processOne = [&stream](rkit::ByteString &outString, rkit::ByteStringConstructionBuffer &cbuf) -> rkit::Result
				{
					RKIT_CHECK(stream.ReadAllSpan(cbuf.GetSpan()));
					outString = rkit::ByteString(std::move(cbuf));
					RKIT_RETURN_OK;
				};

			RKIT_CHECK(rkit::CheckedProcessParallelSpans(strings.ToSpan(), strCBufs.ToSpan(), processOne));
		}

		rkit::Vector<ScriptExpression> scriptExprs;
		rkit::Vector<data::ape::Expression> exprData;

		RKIT_CHECK(scriptExprs.Resize(numExprs));
		RKIT_CHECK(exprData.Resize(numExprs));

		RKIT_CHECK(stream.ReadAllSpan(exprData.ToSpan()));

		RKIT_CHECK(rkit::CheckedProcessParallelSpans(scriptExprs.ToSpan(), exprData.ToSpan(), LoadScriptExpression));

		rkit::Vector<uint32_t> operandListCounts;
		RKIT_CHECK(operandListCounts.Resize(numOperandLists));

		RKIT_CHECK(stream.ReadAllSpan(operandListCounts.ToSpan()));

		size_t totalOperands = 0;
		for (uint32_t &operandListCount : operandListCounts)
		{
			rkit::endian::LittleUInt32_t::StaticConvertToHostOrderInPlace(operandListCount);
			RKIT_CHECK(rkit::SafeAdd<size_t>(totalOperands, totalOperands, operandListCount));
		}

		rkit::Vector<uint32_t> operands;
		RKIT_CHECK(operands.Resize(totalOperands));

		RKIT_CHECK(stream.ReadAllSpan(operands.ToSpan()));

		for (uint32_t &operand : operands)
			rkit::endian::LittleUInt32_t::StaticConvertToHostOrderInPlace(operand);

		rkit::Vector<rkit::Span<const uint32_t>> operandLists;
		RKIT_CHECK(operandLists.Reserve(numOperandLists));

		{
			size_t startOffset = 0;
			for (uint32_t operandListCount : operandListCounts)
			{
				RKIT_CHECK(operandLists.Append(operands.ToSpan().SubSpan(startOffset, operandListCount)));
				startOffset += operandListCount;
			}
		}

		rkit::Vector<anox::data::ape::Window> windowData;
		RKIT_CHECK(windowData.Resize(numWindows));

		rkit::Vector<ScriptWindow> scriptWindows;
		RKIT_CHECK(scriptWindows.Resize(numWindows));

		RKIT_CHECK(stream.ReadAllSpan(windowData.ToSpan()));

		size_t totalWindowStreamBytes = 0;
		RKIT_CHECK(rkit::CheckedProcessParallelSpans(scriptWindows.ToSpan(), windowData.ToSpan(), [&totalWindowStreamBytes](ScriptWindow &outWindow, const anox::data::ape::Window &inWindow) -> rkit::Result
			{
				outWindow.m_windowID = Label::FromRawValue(inWindow.m_windowID.Get());
				return rkit::SafeAdd<size_t>(totalWindowStreamBytes, totalWindowStreamBytes, inWindow.m_commandStreamLength.Get());
			})
		);

		rkit::Vector<uint8_t> windowCommandStreamBytes;
		RKIT_CHECK(windowCommandStreamBytes.Resize(totalWindowStreamBytes));

		RKIT_CHECK(stream.ReadAllSpan(windowCommandStreamBytes.ToSpan()));

		{
			size_t startOffset = 0;
			rkit::ProcessParallelSpans(scriptWindows.ToSpan(), windowData.ToSpan(), [&startOffset, &windowCommandStreamBytes](ScriptWindow &outWindow, const anox::data::ape::Window &inWindow)
				{
					const uint32_t commandStreamLength = inWindow.m_commandStreamLength.Get();
					outWindow.m_commandStream = windowCommandStreamBytes.ToSpan().SubSpan(startOffset, commandStreamLength);
					startOffset += commandStreamLength;
				});
		}

		// Switches
		rkit::Vector<data::ape::Switch> switchData;
		RKIT_CHECK(switchData.Resize(numSwitches));

		rkit::Vector<ScriptSwitch> scriptSwitches;
		RKIT_CHECK(scriptSwitches.Resize(numSwitches));

		RKIT_CHECK(stream.ReadAllSpan(switchData.ToSpan()));

		size_t totalSwitchCommands = 0;
		RKIT_CHECK(rkit::CheckedProcessParallelSpans(scriptSwitches.ToSpan(), switchData.ToSpan(), [&totalSwitchCommands](ScriptSwitch &outSwitch, const data::ape::Switch &inSwitch) -> rkit::Result
			{
				outSwitch.m_switchID = Label::FromRawValue(inSwitch.m_switchID.Get());
				return rkit::SafeAdd<size_t>(totalSwitchCommands, totalSwitchCommands, inSwitch.m_numCommands.Get());
			})
		);

		rkit::Vector<ScriptSwitchCommand> switchCommands;
		rkit::Vector<data::ape::SwitchCommand> switchCommandData;
		RKIT_CHECK(switchCommandData.Resize(totalSwitchCommands));
		RKIT_CHECK(switchCommands.Resize(totalSwitchCommands));

		RKIT_CHECK(stream.ReadAllSpan(switchCommandData.ToSpan()));

		RKIT_CHECK(rkit::CheckedProcessParallelSpans(switchCommands.ToSpan(), switchCommandData.ToSpan(), LoadScriptCommand));

		{
			size_t startOffset = 0;
			rkit::ProcessParallelSpans(scriptSwitches.ToSpan(), switchData.ToSpan(), [&startOffset, &switchCommands](ScriptSwitch &outSwitch, const data::ape::Switch &inSwitch)
				{
					const uint32_t numCommands = inSwitch.m_numCommands.Get();
					outSwitch.m_commands = switchCommands.ToSpan().SubSpan(startOffset, numCommands);
					startOffset += numCommands;
				});
		}

		// Material data
		rkit::Vector<rkit::data::ContentID> materialContentIDs;
		RKIT_CHECK(materialContentIDs.Resize(numMaterialContentIDs));

		RKIT_CHECK(stream.ReadAllSpan(materialContentIDs.ToSpan()));

		rkit::Vector<uint32_t> materialNameLookups;
		RKIT_CHECK(materialNameLookups.Resize(numMaterialNameLookups));
		RKIT_CHECK(stream.ReadAllSpan(materialNameLookups.ToSpan()));

		for (uint32_t &nameLookup : materialNameLookups)
		{
			rkit::endian::LittleUInt32_t::StaticConvertToHostOrderInPlace(nameLookup);
			if (nameLookup >= numMaterialContentIDs)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::HashMap<rkit::ByteString, uint32_t> wildcardLookups;
		for (size_t i = 0; i < numMaterialWildcardLookups; i++)
		{
			data::ape::MaterialWildcardLookup lookup;
			RKIT_CHECK(stream.ReadOneBinary(lookup));

			const uint32_t cidIndex = lookup.m_materialContentIndex.Get();
			const uint32_t strIndex = lookup.m_stringIndex.Get();

			if (cidIndex >= numMaterialContentIDs || strIndex >= numStrings)
				RKIT_THROW(rkit::ResultCode::kDataError);

			RKIT_CHECK(wildcardLookups.Set(strings[strIndex], cidIndex));
		}

		rkit::UniquePtr<ScriptPackage> package;
		RKIT_CHECK(rkit::New<ScriptPackage>(package));

		for (ScriptWindow &window : scriptWindows)
			window.m_package = package.Get();

		for (ScriptSwitch &sw : scriptSwitches)
			sw.m_package = package.Get();

		package->m_windows = std::move(scriptWindows);
		package->m_switches = std::move(scriptSwitches);
		package->m_expressions = std::move(scriptExprs);
		package->m_strings = std::move(strings);
		package->m_operandLists = std::move(operandLists);

		package->m_allOperands = std::move(operands);
		package->m_allWindowCommands = std::move(windowCommandStreamBytes);
		package->m_allSwitchCommands = std::move(switchCommands);

		package->m_materialContentIDs = std::move(materialContentIDs);
		package->m_materialNameLookups = std::move(materialNameLookups);
		package->m_wildcardLookups = std::move(wildcardLookups);

		RKIT_CHECK(GetLayer(layer).AddPackage(std::move(package)));

		RKIT_RETURN_OK;
	}

	rkit::Result ScriptManagerImpl::LoadScriptCommand(ScriptSwitchCommand &outCommand, const data::ape::SwitchCommand &inCommand)
	{
		if (outCommand.m_exprValue.m_exprType >= ScriptExprType::Count)
			RKIT_THROW(rkit::ResultCode::kDataError);

		outCommand.m_exprValue.m_exprType = inCommand.m_exprValue.m_exprType;
		outCommand.m_exprValue.m_index = inCommand.m_exprValue.m_index.Get();
		outCommand.m_fmtValue = inCommand.m_fmtValue.Get();
		outCommand.m_opcode = inCommand.m_opcode;
		outCommand.m_strValue = inCommand.m_strValue.Get();

		RKIT_RETURN_OK;
	}

	rkit::Result ScriptManagerImpl::LoadScriptExpression(ScriptExpression &outExpr, const data::ape::Expression &inExpr)
	{
		data::ape::Expression::UnpackOperandInfo(inExpr.m_packedOperandInfo, outExpr.m_op, outExpr.m_leftOpType, outExpr.m_rightOpType);

		if (outExpr.m_op == ScriptOperator::Invalid || outExpr.m_leftOpType == ScriptOperandType::Invalid || outExpr.m_rightOpType == ScriptOperandType::Invalid)
			RKIT_CHECK(rkit::ResultCode::kDataError);

		outExpr.m_leftValue = inExpr.m_leftValue.Get();
		outExpr.m_rightValue = inExpr.m_rightValue.Get();

		RKIT_RETURN_OK;
	}

	ScriptLayerInstance &ScriptManagerImpl::GetLayer(ScriptManager::ScriptLayer layer)
	{
		return m_layers[static_cast<size_t>(layer)];
	}

	const ScriptLayerInstance &ScriptManagerImpl::GetLayer(ScriptManager::ScriptLayer layer) const
	{
		return m_layers[static_cast<size_t>(layer)];
	}

	void ScriptManagerImpl::FindScript(const Label &label, const ScriptWindow *&outWindow, const ScriptSwitch *&outSwitch) const
	{
		return FindScriptInternal(label, &outWindow, &outSwitch);
	}

	void ScriptManagerImpl::FindScriptPrehashed(const Label &label, rkit::HashValue_t hashValue, const ScriptWindow *&outWindow, const ScriptSwitch *&outSwitch) const
	{
		return FindScriptPrehashedInternal(label, hashValue, &outWindow, &outSwitch);
	}

	void ScriptManagerImpl::FindWindow(const anox::Label &label, const ScriptWindow *&outWindow) const
	{
		return FindScriptInternal(label, &outWindow, nullptr);
	}

	void ScriptManagerImpl::FindWindowPrehashed(const anox::Label &label, rkit::HashValue_t hashValue, const ScriptWindow *&outWindow) const
	{
		return FindScriptPrehashedInternal(label, hashValue, &outWindow, nullptr);
	}

	void ScriptManagerImpl::FindSwitch(const anox::Label &label, const ScriptSwitch *&outSwitch) const
	{
		return FindScriptInternal(label, nullptr, &outSwitch);
	}

	void ScriptManagerImpl::FindSwitchPrehashed(const anox::Label &label, rkit::HashValue_t hashValue, const ScriptSwitch *&outSwitch) const
	{
		return FindScriptPrehashedInternal(label, hashValue, nullptr, &outSwitch);
	}

	void ScriptManagerImpl::FindScriptInternal(const Label &label, const ScriptWindow **outWindow, const ScriptSwitch **outSwitch) const
	{
		return FindScriptPrehashedInternal(label, rkit::Hasher<Label>::ComputeHash(0, label), outWindow, outSwitch);
	}

	void ScriptManagerImpl::FindScriptPrehashedInternal(const Label &label, rkit::HashValue_t hashValue, const ScriptWindow **outWindow, const ScriptSwitch **outSwitch) const
	{
		for (size_t layerPlusOne = static_cast<size_t>(ScriptManager::ScriptLayer::kCount); layerPlusOne > 0; layerPlusOne--)
		{
			const ScriptLayerInstance &layer = m_layers[layerPlusOne - 1];

			if (outWindow)
			{
				if (const ScriptWindow *window = layer.FindWindowPrehashed(label, hashValue))
				{
					*outWindow = window;
					if (outSwitch)
						*outSwitch = nullptr;
					return;
				}
			}

			if (outSwitch)
			{
				if (const ScriptSwitch *sw = layer.FindSwitchPrehashed(label, hashValue))
				{
					if (outWindow)
						*outWindow = nullptr;
					*outSwitch = sw;
					return;
				}
			}
		}

		if (outWindow)
			*outWindow = nullptr;
		if (outSwitch)
			*outSwitch = nullptr;
	}

	rkit::Result ScriptManagerImpl::CreateScriptEnvironment(rkit::UniquePtr<ScriptEnvironment> &outScriptEnvironment)
	{
		return rkit::New<ScriptEnvironment>(outScriptEnvironment, *this);
	}

	rkit::Result ScriptLayerInstance::AddPackage(rkit::UniquePtr<ScriptPackage> &&packageMoved)
	{
		const ScriptPackage &package = *packageMoved;

		RKIT_CHECK(m_packages.Append(std::move(packageMoved)));

		for (const ScriptWindow &window : package.m_windows)
		{
			RKIT_CHECK(m_windows.Set(window.m_windowID, &window));
		}

		for (const ScriptSwitch &sw : package.m_switches)
		{
			RKIT_CHECK(m_switches.Set(sw.m_switchID, &sw));
		}

		RKIT_RETURN_OK;
	}

	void ScriptLayerInstance::UnloadAll()
	{
		m_windows.Clear();
		m_switches.Clear();
	}

	const ScriptWindow *ScriptLayerInstance::FindWindow(const Label &label) const
	{
		return FindWindowPrehashed(label, rkit::Hasher<Label>::ComputeHash(0, label));
	}

	const ScriptWindow *ScriptLayerInstance::FindWindowPrehashed(const Label &label, rkit::HashValue_t hashValue) const
	{
		const rkit::HashMap<Label, const ScriptWindow *>::ConstIterator_t it = m_windows.FindPrehashed(hashValue, label);
		if (it == m_windows.end())
			return nullptr;

		return it.Value();
	}

	const ScriptSwitch *ScriptLayerInstance::FindSwitch(const Label &label) const
	{
		return FindSwitchPrehashed(label, rkit::Hasher<Label>::ComputeHash(0, label));
	}

	const ScriptSwitch *ScriptLayerInstance::FindSwitchPrehashed(const Label &label, rkit::HashValue_t hashValue) const
	{
		const rkit::HashMap<Label, const ScriptSwitch *>::ConstIterator_t it = m_switches.FindPrehashed(hashValue, label);
		if (it == m_switches.end())
			return nullptr;

		return it.Value();
	}

	rkit::Result ScriptManager::LoadScriptPackage(ScriptLayer layer, const rkit::Span<const uint8_t> &contents)
	{
		return Impl().LoadScriptPackage(layer, contents);
	}

	void ScriptManager::UnloadLayer(ScriptLayer layer)
	{
		Impl().GetLayer(layer).UnloadAll();
	}

	rkit::Result ScriptManager::CreateScriptEnvironment(rkit::UniquePtr<ScriptEnvironment> &outScriptEnvironment)
	{
		rkit::UniquePtr<ScriptEnvironment> scriptEnvironment;

		RKIT_CHECK(Impl().CreateScriptEnvironment(scriptEnvironment));

		outScriptEnvironment = std::move(scriptEnvironment);

		RKIT_RETURN_OK;
	}

	rkit::Result ScriptManager::Create(rkit::UniquePtr<ScriptManager> &outScriptManager)
	{
		return rkit::New<ScriptManager>(outScriptManager);
	}

	rkit::Result ScriptEnvironment::CreateScriptContext(rkit::UniquePtr<ScriptContext> &outScriptCtx)
	{
		return rkit::New<ScriptContext>(outScriptCtx);
	}

	bool ScriptEnvironment::TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr) const
	{
		return Impl().TryEvaluateFloatScriptExpr(outValue, pkg, expr, 0);
	}

	ScriptEnvironment::ScriptEnvironment(ScriptManagerImpl &scriptManager)
		: Opaque<ScriptEnvironmentImpl>(scriptManager)
	{
	}

	rkit::ResultCoroutine ScriptEnvironment::StartSequence(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world)
	{
		return Impl().StartSequence(thread, scriptContext, label, world);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::ScriptManagerImpl)
RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::ScriptContextImpl)
RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::ScriptEnvironmentImpl)
