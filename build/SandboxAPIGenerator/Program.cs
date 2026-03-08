using System.Drawing;
using System.Net;
using System.Reflection.Metadata;

namespace SandboxAPIGenerator
{
    internal enum ParamType
    {
        kAddress,
        kBool,
        kInt8,
        kInt16,
        kInt32,
        kInt64,
        kIntPtr,
        kUInt8,
        kUInt16,
        kUInt32,
        kUInt64,
        kUIntPtr,
        kFloat32,
        kFloat64,
    }

    internal struct ParameterDef
    {
        public ParamType ParamType;
        public string Name;

        public ParameterDef(ParamType paramType, string name)
        {
            ParamType = paramType;
            Name = name;
        }
    }

    internal class FunctionDef
    {
        public ParameterDef[] Parameters { get; private set; }
        public ParameterDef[] ReturnValues { get; private set; }
        public string Name { get; private set; }

        public FunctionDef(string name, ParameterDef[] parameters, ParameterDef[] returnValues)
        {
            Parameters = parameters;
            ReturnValues = returnValues;
            Name = name;
        }
    }

    internal class Program
    {
        static string[] TokenizeLine(string line, int lineNum)
        {
            List<string> result = new List<string>();
            string[] wsSplit = SplitWithWhitespace(line);

            foreach (string wsItem in wsSplit)
            {
                string[] puncSplit = SplitWithPunctuation(wsItem, lineNum);
                result.AddRange(puncSplit);
            }

            return result.ToArray();
        }

        private static string[] SplitWithPunctuation(string wsItem, int lineNum)
        {
            int startPos = 0;

            List<string> result = new List<string>();

            while (startPos < wsItem.Length)
            {
                char c = wsItem[startPos];

                int len = 0;
                if (c == '_' || IsAlphaChar(c))
                {
                    len = ParseIdentifier(wsItem, startPos);
                }
                else if (c == '(' || c == ')' || c == ',')
                {
                    len = 1;
                }
                else if (c == '-' && (startPos + 1) < wsItem.Length && wsItem[startPos + 1] == '>')
                {
                    len = 2;
                }
                else if (c == '\"')
                {
                    len = ParseQuotedString(wsItem, startPos, lineNum);
                }
                else
                    throw new Exception($"Invalid character '{c}' on line {lineNum}");

                result.Add(wsItem.Substring(startPos, len));

                startPos += len;
            }

            return result.ToArray();
        }

        private static int ParseQuotedString(string wsItem, int startPos, int lineNum)
        {
            int endPos = startPos + 1;
            while (endPos < wsItem.Length)
            {
                char ch = wsItem[endPos];
                if (wsItem[endPos] == '\"')
                {
                    endPos++;
                    return endPos - startPos;
                }

                if (ch == '\\')
                {
                    endPos++;
                    if (endPos == wsItem.Length)
                        break;
                }

                endPos++;
            }

            throw new Exception($"Unterminated string constant on line {lineNum}");
        }

        private static int ParseIdentifier(string wsItem, int startPos)
        {
            int endPos = startPos + 1;
            while (endPos < wsItem.Length)
            {
                if (!IsIdentifierChar(wsItem[endPos], true))
                    break;

                endPos++;
            }

            return endPos - startPos;
        }

        private static bool IsIdentifierChar(char c, bool permitSplits)
        {
            if (c == '_' || IsAlphaChar(c) || IsDigitChar(c))
                return true;

            if (permitSplits && (c == '.' || c == '/'))
                return true;

            return false;
        }

        private static bool IsDigitChar(char c)
        {
            return (c >= '0' && c <= '9');
        }

        private static bool IsAlphaChar(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        private static string[] SplitWithWhitespace(string line)
        {
            int startPos = 0;

            List<string> result = new List<string>();

            for (int i = 0; i <= line.Length; i++)
            {
                if (i == line.Length || IsWhitespace(line[i]))
                {
                    int endPos = i;
                    if (endPos - startPos != 0)
                    {
                        result.Add(line.Substring(startPos, endPos - startPos));
                    }
                    startPos = i + 1;
                }
            }

            return result.ToArray();
        }

        private static bool IsWhitespace(char v)
        {
            return v <= ' ';
        }

        static void ThrowOnLine(int lineNum, string text)
        {
            throw new Exception($"Line {lineNum}: {text}");
        }

        static string[] SplitIdentifierChunks(string str, char splitter, string errName)
        {
            string[] chunks = str.Split(splitter);

            foreach (string chunk in chunks)
            {
                if (chunk.Length == 0)
                    throw new Exception($"{errName} was invalid");

                foreach (char ch in chunk)
                {
                    if (!IsIdentifierChar(ch, false))
                        throw new Exception($"{errName} was invalid");
                }
            }

            return chunks;
        }

        static string CppTypeForParamType(ParamType paramType, bool addrIsAddress)
        {
            switch (paramType)
            {
                case ParamType.kBool:
                    return "bool";
                case ParamType.kAddress:
                    if (addrIsAddress)
                        return "::rkit::sandbox::Address_t";
                    else
                        return "void*";
                case ParamType.kInt8:
                    return "int8_t";
                case ParamType.kInt16:
                    return "int16_t";
                case ParamType.kInt32:
                    return "int32_t";
                case ParamType.kInt64:
                    return "int64_t";
                case ParamType.kIntPtr:
                    if (addrIsAddress)
                        return "::rkit::sandbox::AddressDiff_t";
                    else
                        return "intptr_t";
                case ParamType.kUInt8:
                    return "uint8_t";
                case ParamType.kUInt16:
                    return "uint16_t";
                case ParamType.kUInt32:
                    return "uint32_t";
                case ParamType.kUInt64:
                    return "uint64_t";
                case ParamType.kUIntPtr:
                    if (addrIsAddress)
                        return "::rkit::sandbox::Address_t";
                    else
                        return "uintptr_t";
                case ParamType.kFloat32:
                    return "float";
                case ParamType.kFloat64:
                    return "double";
                default:
                    throw new Exception("Internal error: Unknown type");
            }
        }

        static void WriteCanonicalParamList(TextWriter sw, string prefix, IEnumerable<ParameterDef> returnValues, IEnumerable<ParameterDef> parameters, bool addrIsAddress)
        {
            sw.Write("(");
            sw.Write(prefix);

            bool anyParams = false;
            foreach (ParameterDef rv in returnValues)
            {
                if (!anyParams)
                    anyParams = true;
                else
                    sw.Write(", ");

                sw.Write(CppTypeForParamType(rv.ParamType, addrIsAddress));
                sw.Write("& ");
                sw.Write(rv.Name);
            }
            foreach (ParameterDef p in parameters)
            {
                if (!anyParams)
                    anyParams = true;
                else
                    sw.Write(", ");

                sw.Write(CppTypeForParamType(p.ParamType, addrIsAddress));
                sw.Write(" ");
                sw.Write(p.Name);
            }

            sw.Write(")");
        }

        static void WriteNamespace(TextWriter sw, string[] classNameChunks)
        {

            sw.Write("namespace ");
            for (int i = 0; i < classNameChunks.Length; i++)
            {
                if (i != 0)
                    sw.Write("::");

                sw.Write(classNameChunks[i]);
            }
            sw.WriteLine();
        }

        static void Main(string[] args)
        {
            if (args.Length != 1)
                throw new Exception("Usage: SandboxAPIGenerator <path>");

            string[] lines = File.ReadAllLines(args[0]);

            string targetPath = null;
            string className = null;

            List<FunctionDef> imports = new List<FunctionDef>();
            List<FunctionDef> exports = new List<FunctionDef>();

            int lineNum = 1;
            foreach (string line in lines)
            {
                string[] tokens = TokenizeLine(line, lineNum++);

                if (tokens.Length == 0)
                    continue;

                string directive = tokens[0];

                if (directive == "target")
                {
                    if (tokens.Length != 2)
                        ThrowOnLine(lineNum, "Target directive usage: target <target path>");

                    targetPath = tokens[1];
                }
                else if (directive == "class")
                {
                    if (tokens.Length != 2)
                        ThrowOnLine(lineNum, "Class directive usage: class <class name>");

                    className = tokens[1];
                }
                else if (directive == "export" || directive == "import")
                {
                    ParseFunctionDef(lineNum, tokens, (directive == "export") ? exports : imports);
                }
                else
                    ThrowOnLine(lineNum, "Unknown directive");
            }

            {
                string[] targetPathChunks = SplitIdentifierChunks(targetPath, '/', "target");
                string[] classNameChunks = SplitIdentifierChunks(className, '.', "class name");

                string dirPath = "";
                for (int i = 0; i < targetPathChunks.Length - 1; i++)
                {
                    dirPath = Path.Combine(dirPath, targetPathChunks[i]);

                    if (!Directory.Exists(dirPath))
                        Directory.CreateDirectory(dirPath);
                }

                string lastTargetChunk = targetPathChunks[targetPathChunks.Length - 1];
                string targetPrefix = Path.Combine(dirPath, lastTargetChunk);

                using (StreamWriter sw = new StreamWriter(targetPrefix + ".sb.generated.h"))
                {
                    sw.WriteLine("#include \"rkit/Core/Result.h\"");
                    sw.WriteLine("#include \"rkit/Sandbox/SandboxEntryDescriptor.h\"");
                    sw.WriteLine();

                    WriteNamespace(sw, classNameChunks);
                    sw.WriteLine("{");

                    string indent = "\t";

                    sw.WriteLine(indent + "class SandboxImports");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "public:");

                    foreach (FunctionDef fdef in imports)
                    {
                        sw.Write(indent + "\tstatic ::rkit::Result " + fdef.Name);

                        WriteCanonicalParamList(sw, "", fdef, false);
                        sw.WriteLine(";");
                    }

                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "class SandboxExports");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "public:");

                    foreach (FunctionDef fdef in exports)
                    {
                        sw.Write(indent + "\tstatic ::rkit::Result " + fdef.Name);

                        WriteCanonicalParamList(sw, "", fdef, false);
                        sw.WriteLine(";");
                    }

                    sw.WriteLine(indent + "};");

                    sw.WriteLine("}");
                }

                using (StreamWriter sw = new StreamWriter(targetPrefix + ".sb.generated.inl"))
                {
                    sw.WriteLine("#include \"" + lastTargetChunk + ".sb.generated.h\"");
                    sw.WriteLine("#include \"rkit/Sandbox/SandboxIO.h\"");
                    sw.WriteLine();

                    WriteNamespace(sw, classNameChunks);
                    sw.WriteLine("{");

                    string indent = "\t";


                    sw.WriteLine(indent + "class SandboxAPI");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "public:");
                    sw.WriteLine(indent + "\tstatic ::rkit::sandbox::SysCallStub ms_sysCallStubs[" + imports.Count.ToString() + "];");
                    sw.WriteLine(indent + "\tstatic const ::rkit::sandbox::SysCallDescriptor ms_sysCallDescriptors[" + imports.Count.ToString() + "];");
                    sw.WriteLine(indent + "\tstatic const ::rkit::sandbox::EntryDescriptor ms_entryDescriptor;");
                    sw.WriteLine(indent + "\tstatic ::rkit::ISandbox *ms_sandboxPtr;");
                    sw.WriteLine(indent + "\tstatic ::rkit::sandbox::Environment *ms_sandboxEnvPtr;");
                    sw.WriteLine(indent + "\tstatic const ::rkit::sandbox::io::SysCallDispatchFunc_t *ms_sysCalls;");
                    sw.WriteLine(indent + "};");

                    sw.WriteLine();

                    for (int sysCallID = 0; sysCallID < imports.Count; sysCallID++)
                    {
                        FunctionDef fdef = imports[sysCallID];

                        sw.Write(indent + "::rkit::Result SandboxImports::" + fdef.Name);

                        WriteCanonicalParamList(sw, "", fdef, false);
                        sw.WriteLine();
                        sw.WriteLine(indent + "{");

                        ParameterDef[] returnValues = fdef.ReturnValues;
                        ParameterDef[] parameters = fdef.Parameters;
                        sw.WriteLine(indent + "\t::rkit::sandbox::io::Value_t loc_ioContext[" + (1 + returnValues.Length + parameters.Length) + "];");

                        for (int i = 0; i < parameters.Length; i++)
                        {
                            sw.WriteLine(indent + "\tloc_ioContext[" + (returnValues.Length + 1 + i).ToString() + "] = ::rkit::sandbox::io::LoadValue(" + parameters[i].Name + ");");
                        }

                        sw.WriteLine(indent + "\t::rkit::sandbox::io::SysCall(SandboxAPI::ms_sandboxPtr, SandboxAPI::ms_sandboxEnvPtr, SandboxAPI::ms_sysCalls, SandboxAPI::ms_sysCallStubs[" + sysCallID.ToString() + "].m_sysCallID, loc_ioContext);");

                        sw.WriteLine(indent + "\tRKIT_CHECK(::rkit::ThrowIfError(static_cast<::rkit::PackedResultAndExtCode>(loc_ioContext[" + returnValues.Length.ToString() + "])));");

                        for (int i = 0; i < returnValues.Length; i++)
                        {
                            sw.WriteLine(indent + "\t::rkit::sandbox::io::StoreValue(" + returnValues[i].Name + ", loc_ioContext[" + (returnValues.Length - 1 + i).ToString() + "]);");
                        }

                        sw.WriteLine(indent + "\tRKIT_RETURN_OK;");
                        sw.WriteLine(indent + "}");
                        sw.WriteLine();
                    }

                    sw.WriteLine(indent + "::rkit::sandbox::SysCallStub SandboxAPI::ms_sysCallStubs[" + imports.Count.ToString() + "] = {};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "const ::rkit::sandbox::SysCallDescriptor SandboxAPI::ms_sysCallDescriptors[" + imports.Count.ToString() + "] =");
                    sw.WriteLine(indent + "{");
                    foreach (FunctionDef fdef in imports)
                    {
                        sw.WriteLine(indent + "\t{ u8\"" + fdef.Name + "\", " + fdef.Name.Length.ToString() + " },");
                    }
                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "class SandboxExportThunks");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "public:");
                    foreach (FunctionDef fdef in exports)
                    {
                        sw.WriteLine(indent + "\tstatic void " + fdef.Name + "(void *ioContext) noexcept");
                        sw.WriteLine(indent + "\t{");

                        ParameterDef[] returnValues = fdef.ReturnValues;
                        ParameterDef[] parameters = fdef.Parameters;

                        List<ParameterDef> prv = new List<ParameterDef>();
                        prv.AddRange(returnValues);
                        prv.AddRange(parameters);

                        foreach (ParameterDef pdef in parameters)
                        {
                            sw.WriteLine(indent + "\t\t" + CppTypeForParamType(pdef.ParamType, false) + " " + pdef.Name + ";");
                        }

                        foreach (ParameterDef rvDef in returnValues)
                        {
                            sw.Write(indent + "\t\t" + CppTypeForParamType(rvDef.ParamType, false) + " " + rvDef.Name + " = ");
                            if (rvDef.ParamType == ParamType.kAddress)
                                sw.Write("nullptr");
                            else
                                sw.Write("0");
                            sw.WriteLine(";");
                        }


                        for (int i = 0; i < parameters.Length; i++)
                        {
                            ParameterDef param = parameters[i];
                            sw.WriteLine(indent + "\t\t::rkit::sandbox::io::StoreValue(" + param.Name + ", ::rkit::sandbox::io::LoadParam(ioContext, " + i.ToString() + "));");
                        }

                        sw.Write(indent + "\t\t::rkit::PackedResultAndExtCode loc_resultCode = RKIT_TRY_EVAL(SandboxExports::" + fdef.Name + "(");

                        {
                            bool anyParam = false;

                            foreach (ParameterDef prvDef in prv)
                            {
                                if (anyParam)
                                    sw.Write(", ");
                                else
                                    anyParam = true;

                                sw.Write(prvDef.Name);
                            }
                        }

                        sw.WriteLine("));");

                        sw.WriteLine(indent + "\t\t::rkit::sandbox::io::StoreResult(ioContext, loc_resultCode);");

                        if (returnValues.Length > 0)
                        {
                            sw.WriteLine(indent + "\t\tif (::rkit::utils::ResultIsOK(loc_resultCode))");
                            sw.WriteLine(indent + "\t\t{");
                            for (int i = 0; i < returnValues.Length; i++)
                            {
                                sw.WriteLine(indent + "\t\t\t::rkit::sandbox::io::StoreReturnValue(ioContext, " + i.ToString() + ", ::rkit::sandbox::io::LoadValue(" + returnValues[i].Name + "));");
                            }
                            sw.WriteLine(indent + "\t\t}");
                        }

                        sw.WriteLine(indent + "\t}");
                        sw.WriteLine();
                    }

                    sw.WriteLine(indent + "\tstatic const ::rkit::sandbox::ExportDescriptor ms_exports[" + exports.Count + "];");
                    sw.WriteLine(indent + "};");

                    sw.WriteLine();
                    sw.WriteLine(indent + "const ::rkit::sandbox::ExportDescriptor SandboxExportThunks::ms_exports[" + exports.Count + "] =");
                    sw.WriteLine(indent + "{");
                    foreach (FunctionDef fdef in exports)
                    {
                        sw.WriteLine(indent + "\t{ u8\"" + fdef.Name + "\", " + fdef.Name.Length.ToString() + ", SandboxExportThunks::" + fdef.Name + " },");

                    }
                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "const ::rkit::sandbox::EntryDescriptor SandboxAPI::ms_entryDescriptor =");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "\t{");
                    sw.WriteLine(indent + "\t\tsizeof(void *),");
                    sw.WriteLine(indent + "\t\tsizeof(::rkit::sandbox::ExportDescriptor::ExportFunctionPtr_t),");
                    sw.WriteLine(indent + "\t\tsizeof(::rkit::sandbox::EntryDescriptor),");
                    sw.WriteLine(indent + "\t\t0,");
                    sw.WriteLine(indent + "\t\t" + imports.Count.ToString() + ",");
                    sw.WriteLine(indent + "\t\t" + exports.Count.ToString() + ",");
                    sw.WriteLine(indent + "\t},");
                    sw.WriteLine(indent + "\tSandboxAPI::ms_sysCallDescriptors,");
                    sw.WriteLine(indent + "\tSandboxAPI::ms_sysCallStubs,");
                    sw.WriteLine(indent + "\tSandboxExportThunks::ms_exports,");
                    sw.WriteLine(indent + "\t&SandboxExportThunks::ms_exports[0].m_functionAddress,");
                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "::rkit::ISandbox *SandboxAPI::ms_sandboxPtr = nullptr;");
                    sw.WriteLine(indent + "::rkit::sandbox::Environment *SandboxAPI::ms_sandboxEnvPtr = nullptr;");
                    sw.WriteLine(indent + "const ::rkit::sandbox::io::SysCallDispatchFunc_t *SandboxAPI::ms_sysCalls = nullptr;");

                    sw.WriteLine("}");
                }

                using (StreamWriter sw = new StreamWriter(targetPrefix + ".host.generated.h"))
                {
                    sw.WriteLine("#include \"rkit/Sandbox/SandboxIO.h\"");
                    sw.WriteLine("#include \"rkit/Sandbox/SandboxHostDescriptor.h\"");
                    sw.WriteLine();

                    sw.WriteLine("namespace rkit::sandbox");
                    sw.WriteLine("{");
                    sw.WriteLine("\tstruct Environment;");
                    sw.WriteLine("\tstruct IThreadContext;");
                    sw.WriteLine("}");

                    WriteNamespace(sw, classNameChunks);
                    sw.WriteLine("{");

                    string indent = "\t";

                    sw.WriteLine(indent + "class HostImports");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "public:");
                    sw.WriteLine(indent + "\tHostImports();");

                    foreach (FunctionDef fdef in exports)
                    {
                        sw.Write(indent + "\tstatic ::rkit::Result " + fdef.Name);

                        WriteCanonicalParamList(sw, "::rkit::sandbox::IThreadContext *thread, ", fdef, true);
                        sw.WriteLine(";");
                    }

                    sw.WriteLine();
                    sw.WriteLine(indent + "\t::rkit::sandbox::HostAPIDescriptor &GetHostAPIDescriptor();");

                    sw.WriteLine();
                    sw.WriteLine(indent + "private:");
                    sw.WriteLine(indent + "\tHostImports &operator=(const HostImports&) = delete;");
                    sw.WriteLine(indent + "\tHostImports(const HostImports&) = delete;");
                    sw.WriteLine();
                    sw.WriteLine(indent + "\t::rkit::sandbox::Address_t m_importAddresses[" + exports.Count.ToString() + "];");
                    sw.WriteLine(indent + "\t::rkit::sandbox::HostAPIDescriptor m_hostAPI;");

                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "class HostExports");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "public:");

                    foreach (FunctionDef fdef in imports)
                    {
                        sw.Write(indent + "\tstatic ::rkit::Result " + fdef.Name);

                        WriteCanonicalParamList(sw, "::rkit::sandbox::Environment &env, ::rkit::sandbox::IThreadContext *thread, ", fdef, true);
                        sw.WriteLine(";");
                    }
                    sw.WriteLine(indent + "};");
                    sw.WriteLine("}");
                    sw.WriteLine();


                    WriteNamespace(sw, classNameChunks);
                    sw.WriteLine("{");
                    sw.WriteLine(indent + "inline ::rkit::sandbox::HostAPIDescriptor &HostImports::GetHostAPIDescriptor()");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "\treturn this->m_hostAPI;");
                    sw.WriteLine(indent + "}");
                    sw.WriteLine("}");
                }

                using (StreamWriter sw = new StreamWriter(targetPrefix + ".host.generated.inl"))
                {
                    sw.WriteLine("#include \"" + lastTargetChunk + ".host.generated.h\"");
                    sw.WriteLine("#include \"rkit/Sandbox/Sandbox.h\"");
                    sw.WriteLine();

                    WriteNamespace(sw, classNameChunks);
                    sw.WriteLine("{");

                    string indent = "\t";

                    sw.WriteLine(indent + "class HostAPI");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "public:");
                    sw.WriteLine(indent + "\tstatic const ::rkit::sandbox::io::SysCallDispatchFunc_t ms_sysCalls[" + imports.Count.ToString() + "];");
                    sw.WriteLine(indent + "\tstatic const ::rkit::sandbox::HostAPIName ms_sysCallNames[" + imports.Count.ToString() + "];");
                    sw.WriteLine(indent + "\tstatic const ::rkit::sandbox::HostAPIName ms_importNames[" + exports.Count.ToString() + "];");
                    sw.WriteLine();
                    sw.WriteLine(indent + "private:");

                    foreach (FunctionDef fdef in imports)
                    {
                        sw.WriteLine(indent + "\tstatic ::rkit::PackedResultAndExtCode " + fdef.Name + "(::rkit::ISandbox *sandbox, ::rkit::sandbox::Environment *env, ::rkit::sandbox::IThreadContext *thread, ::rkit::sandbox::Address_t ioValuesAddr) noexcept;");
                    }

                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "const ::rkit::sandbox::io::SysCallDispatchFunc_t HostAPI::ms_sysCalls[" + imports.Count.ToString() + "] =");
                    sw.WriteLine(indent + "{");

                    foreach (FunctionDef fdef in imports)
                    {
                        sw.WriteLine(indent + "\tHostAPI::" + fdef.Name + ",");
                    }

                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "const ::rkit::sandbox::HostAPIName HostAPI::ms_sysCallNames[" + imports.Count.ToString() + "] =");
                    sw.WriteLine(indent + "{");
                    foreach (FunctionDef fdef in imports)
                    {
                        sw.WriteLine(indent + "\t{ u8\"" + fdef.Name + "\", " + fdef.Name.Length.ToString() + " },");
                    }
                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    sw.WriteLine(indent + "const ::rkit::sandbox::HostAPIName HostAPI::ms_importNames[" + exports.Count.ToString() + "] =");
                    sw.WriteLine(indent + "{");
                    foreach (FunctionDef fdef in exports)
                    {
                        sw.WriteLine(indent + "\t{ u8\"" + fdef.Name + "\", " + fdef.Name.Length.ToString() + " },");
                    }
                    sw.WriteLine(indent + "};");
                    sw.WriteLine();

                    foreach (FunctionDef fdef in imports)
                    {
                        ParameterDef[] parameters = fdef.Parameters;
                        ParameterDef[] returnValues = fdef.ReturnValues;

                        string ioConditionLine = "if (!sandbox->TryAccessMemoryArray(loc_ioValues, ioValuesAddr, " + (parameters.Length + returnValues.Length).ToString() + "))";

                        sw.WriteLine(indent + "::rkit::PackedResultAndExtCode HostAPI::" + fdef.Name + "(::rkit::ISandbox *sandbox, ::rkit::sandbox::Environment *env, ::rkit::sandbox::IThreadContext *thread, ::rkit::sandbox::Address_t ioValuesAddr) noexcept");
                        sw.WriteLine(indent + "{");
                        sw.WriteLine(indent + "\t::rkit::sandbox::io::Value_t *loc_ioValues = nullptr;");

                        foreach (ParameterDef parameter in parameters)
                        {
                            sw.WriteLine(indent + "\t" + CppTypeForParamType(parameter.ParamType, true) + " " + parameter.Name + ";");
                        }

                        foreach (ParameterDef rvDef in returnValues)
                        {
                            sw.WriteLine(indent + "\t" + CppTypeForParamType(rvDef.ParamType, true) + " " + rvDef.Name + " = 0;");
                        }

                        if (parameters.Length > 0)
                        {
                            sw.WriteLine(indent + "\t" + ioConditionLine);
                            sw.WriteLine(indent + "\t\treturn ::rkit::utils::PackResult(::rkit::ResultCode::kSandboxMemoryError);");
                            sw.WriteLine();
                            for (int paramIndex = 0; paramIndex < parameters.Length; paramIndex++)
                            {
                                sw.WriteLine(indent + "\t::rkit::sandbox::io::StoreValue(" + parameters[paramIndex].Name + ", loc_ioValues[" + (paramIndex + returnValues.Length).ToString() + "]);");
                            }
                        }

                        sw.Write(indent + "\t::rkit::PackedResultAndExtCode loc_result = RKIT_TRY_EVAL(HostExports::" + fdef.Name + "(*env, thread");
                        foreach (ParameterDef rv in returnValues)
                        {
                            sw.Write(", ");
                            sw.Write(rv.Name);
                        }
                        foreach (ParameterDef p in parameters)
                        {
                            sw.Write(", ");
                            sw.Write(p.Name);
                        }
                        sw.WriteLine("));");
                        sw.WriteLine();

                        if (returnValues.Length > 0)
                        {
                            sw.WriteLine(indent + "\tif (::rkit::utils::ResultIsOK(loc_result))");
                            sw.WriteLine(indent + "\t{");
                            sw.WriteLine(indent + "\t\t" + ioConditionLine);
                            sw.WriteLine(indent + "\t\t\tloc_result = ::rkit::utils::PackResult(::rkit::ResultCode::kSandboxMemoryError);");
                            sw.WriteLine(indent + "\t\telse");
                            sw.WriteLine(indent + "\t\t{");
                            for (int rvIndex = 0; rvIndex < returnValues.Length; rvIndex++)
                            {
                                sw.WriteLine(indent + "\t\t\tloc_ioValues[" + rvIndex + "] = ::rkit::sandbox::io::LoadValue(" + returnValues[rvIndex].Name + ");");
                            }
                            sw.WriteLine(indent + "\t\t}");
                            sw.WriteLine(indent + "\t}");
                        }
                        sw.WriteLine();
                        sw.WriteLine(indent + "\treturn loc_result;");

                        sw.WriteLine(indent + "}");
                        sw.WriteLine();
                    }

                    sw.WriteLine(indent + "HostImports::HostImports()");
                    sw.WriteLine(indent + "\t: m_importAddresses {}");
                    sw.WriteLine(indent + "\t, m_hostAPI{ nullptr, { HostAPI::ms_sysCalls, " + imports.Count.ToString() + " }, HostAPI::ms_sysCallNames, m_importAddresses, HostAPI::ms_importNames, " + exports.Count.ToString() + " }");
                    sw.WriteLine(indent + "{");
                    sw.WriteLine(indent + "}");

                    sw.WriteLine("}");
                }
            }
        }

        private static void WriteCanonicalParamList(StreamWriter sw, string prefix, FunctionDef fdef, bool addrIsAddress)
        {
            WriteCanonicalParamList(sw, prefix, fdef.ReturnValues, fdef.Parameters, addrIsAddress);
        }

        private static void ParseParameterList(int lineNum, string[] tokens, ref int tokenNum, IList<ParameterDef> parameters)
        {
            if (tokenNum >= tokens.Length || tokens[tokenNum] != "(")
                ThrowOnLine(lineNum, "Expected '('");

            tokenNum++;

            if (tokenNum < tokens.Length && tokens[tokenNum] == ")")
                tokenNum++;
            else
            {
                for (; ; )
                {
                    int tokensAvailable = tokens.Length - tokenNum;
                    if (tokensAvailable < 3)
                        ThrowOnLine(lineNum, "Expected parameter def");

                    string paramTypeStr = tokens[tokenNum];
                    string paramName = tokens[tokenNum + 1];

                    string terminator = tokens[tokenNum + 2];

                    tokenNum += 3;

                    ParamType paramType = ParamType.kInt32;
                    if (paramTypeStr == "address")
                        paramType = ParamType.kAddress;
                    else if (paramTypeStr == "bool")
                        paramType = ParamType.kBool;
                    else if (paramTypeStr == "int8")
                        paramType = ParamType.kInt8;
                    else if (paramTypeStr == "int32")
                        paramType = ParamType.kInt32;
                    else if (paramTypeStr == "int64")
                        paramType = ParamType.kInt64;
                    else if (paramTypeStr == "intptr")
                        paramType = ParamType.kIntPtr;
                    else if (paramTypeStr == "uint8")
                        paramType = ParamType.kUInt8;
                    else if (paramTypeStr == "uint32")
                        paramType = ParamType.kUInt32;
                    else if (paramTypeStr == "uint64")
                        paramType = ParamType.kUInt64;
                    else if (paramTypeStr == "uintptr")
                        paramType = ParamType.kUIntPtr;
                    else if (paramTypeStr == "float32")
                        paramType = ParamType.kFloat32;
                    else if (paramTypeStr == "float64")
                        paramType = ParamType.kFloat64;
                    else
                        ThrowOnLine(lineNum, $"Unknown type: {paramTypeStr}");

                    parameters.Add(new ParameterDef(paramType, paramName));

                    if (terminator == ",")
                        continue;
                    else if (terminator == ")")
                        break;
                    else
                        ThrowOnLine(lineNum, "Expected ',' or ')'");
                }
            }
        }

        private static void ParseFunctionDef(int lineNum, string[] tokens, IList<FunctionDef> functionDefs)
        {
            if (tokens.Length < 2)
                ThrowOnLine(lineNum, "Invalid function def");

            string functionName = tokens[1];

            List<ParameterDef> parameters = new List<ParameterDef>();
            List<ParameterDef> returnValues = new List<ParameterDef>();

            int tokenNum = 2;
            ParseParameterList(lineNum, tokens, ref tokenNum, parameters);

            if (tokenNum < tokens.Length)
            {
                if (tokens[tokenNum] != "->")
                    ThrowOnLine(lineNum, "Expected '->'");

                tokenNum++;
                ParseParameterList(lineNum, tokens, ref tokenNum, returnValues);
            }

            if (tokenNum != tokens.Length)
                ThrowOnLine(lineNum, "Unexpected tokens after return value list");

            functionDefs.Add(new FunctionDef(functionName, parameters.ToArray(), returnValues.ToArray()));
        }
    }
}
