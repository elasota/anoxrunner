namespace APEWindowCommandReflector
{
    internal class FieldDef
    {
        public enum FieldType
        {
            UInt16,
            UInt32,
            Str,
            OptStr,
            Format,
            OptExpr,
            Bits32,
            Padding,
        }

        public FieldType Type { get; }
        public string Name { get; }
        public IReadOnlyList<KeyValuePair<string, int>>? Bits { get; }

        public FieldDef(FieldType type, string name)
        {
            Type = type;
            Name = name;
        }

        public FieldDef(IReadOnlyList<KeyValuePair<string, int>> bits)
        {
            Type = FieldType.Bits32;
            Bits = bits;
            Name = "bits";
        }
    }

    internal class WindowCommandDef
    {
        public string Name { get; }
        public byte Opcode { get; }
        public IReadOnlyList<FieldDef> Fields { get; }


        public WindowCommandDef(string name, byte opcode, IReadOnlyList<FieldDef> fields)
        {
            Name = name;
            Opcode = opcode;
            Fields = fields;
        }
    }

    internal class Program
    {
        static void Main(string[] args)
        {
            if (args.Length != 2)
                throw new Exception("Usage: APEWindowCommandReflector <defs file> <output directory>");

            string[] defLines = File.ReadAllLines(args[0]);

            List<WindowCommandDef> windowCommandDefs = ParseWindowCommandDefs(defLines);

            string outBuildDir = Path.Combine(args[1], "Anox_Build").Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
            string outDataIncludeDir = Path.Combine(args[1], "include", "anox", "Data").Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);

            Directory.CreateDirectory(outBuildDir);
            Directory.CreateDirectory(outDataIncludeDir);

            using (StreamWriter sw = new StreamWriter(Path.Combine(outBuildDir, "AnoxAPEWindowCommandData.generated.h")))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#pragma once");
                sw.WriteLine();
                sw.WriteLine("#include \"AnoxAPEParser.h\"");
                sw.WriteLine("#include \"rkit/Core/Optional.h\"");
                sw.WriteLine("#include \"rkit/Core/String.h\"");
                sw.WriteLine();
                sw.WriteLine("namespace anox::buildsystem::ape_parse");
                sw.WriteLine("{");

                foreach (WindowCommandDef wcDef in windowCommandDefs)
                {
                    sw.WriteLine("\tstruct " + wcDef.Name.ToString() + "Command : public WindowCommand");
                    sw.WriteLine("\t{");

                    foreach (FieldDef fieldDef in wcDef.Fields)
                    {
                        string? fieldCppType = null;
                        string? initValue = null;
                        switch (fieldDef.Type)
                        {
                            case FieldDef.FieldType.Padding:
                                break;
                            case FieldDef.FieldType.Bits32:
                                foreach (KeyValuePair<string, int> bit in fieldDef.Bits)
                                    sw.WriteLine("\t\tbool m_" + bit.Key + " = false;");
                                break;
                            case FieldDef.FieldType.UInt16:
                                fieldCppType = "uint16_t";
                                initValue = "0";
                                break;
                            case FieldDef.FieldType.UInt32:
                                fieldCppType = "uint32_t";
                                initValue = "0";
                                break;
                            case FieldDef.FieldType.Str:
                                fieldCppType = "::rkit::ByteString";
                                break;
                            case FieldDef.FieldType.OptStr:
                                fieldCppType = "::rkit::Optional<::rkit::ByteString>";
                                break;
                            case FieldDef.FieldType.Format:
                                fieldCppType = "FormattingValue";
                                break;
                            case FieldDef.FieldType.OptExpr:
                                fieldCppType = "::rkit::Optional<ExpressionValue>";
                                break;
                            default:
                                throw new Exception("Unhandled fielddef type");
                        }

                        if (fieldCppType != null)
                        {
                            sw.Write("\t\t" + fieldCppType + " m_" + fieldDef.Name);
                            if (initValue != null)
                                sw.Write(" = " + initValue);
                            sw.WriteLine(";");
                        }
                    }

                    sw.WriteLine();
                    sw.WriteLine("\t\t::anox::data::WindowCommandType GetCommandType() const override");
                    sw.WriteLine("\t\t{");
                    sw.WriteLine("\t\t\treturn ::anox::data::WindowCommandType::" + wcDef.Name + ";");
                    sw.WriteLine("\t\t}");
                    sw.WriteLine();
                    sw.WriteLine("\t\t::rkit::Result Parse(APEReader &reader) override");
                    sw.WriteLine("\t\t{");

                    foreach (FieldDef fieldDef in wcDef.Fields)
                    {
                        string indent = "\t\t\t";
                        switch (fieldDef.Type)
                        {
                            case FieldDef.FieldType.Padding:
                                sw.WriteLine(indent + "RKIT_CHECK(reader.SkipPadding(" + fieldDef.Name + "));");
                                break;
                            case FieldDef.FieldType.Bits32:
                                {
                                    sw.WriteLine(indent + "{");
                                    sw.WriteLine(indent + "\tuint32_t tempBits = 0;");
                                    uint bitMask = 0;
                                    foreach (KeyValuePair<string, int> bits in fieldDef.Bits!)
                                        bitMask |= (uint)(1 << bits.Value);

                                    sw.WriteLine(indent + "\tRKIT_CHECK(reader.ReadBits(tempBits, " + bitMask.ToString() + "u));");
                                    foreach (KeyValuePair<string, int> bits in fieldDef.Bits!)
                                    {
                                        uint singleBitMask = (uint)(1 << bits.Value);

                                        sw.WriteLine(indent + "\tthis->m_" + bits.Key + " = ((tempBits & " + singleBitMask.ToString() + ") != 0);");
                                    }
                                    sw.WriteLine(indent + "}");
                                }
                                break;
                            default:
                                sw.WriteLine(indent + "RKIT_CHECK(reader.Read(this->m_" + fieldDef.Name + "));");
                                break;
                        }
                    }


                    sw.WriteLine("\t\t\tRKIT_RETURN_OK;");
                    sw.WriteLine("\t\t}");
                    sw.WriteLine();
                    sw.WriteLine("\t\t::rkit::Result Write(IAPEWriter &writer) const override");
                    sw.WriteLine("\t\t{");

                    foreach (FieldDef fieldDef in wcDef.Fields)
                    {
                        string indent = "\t\t\t";
                        switch (fieldDef.Type)
                        {
                            case FieldDef.FieldType.Padding:
                                break;
                            case FieldDef.FieldType.Bits32:
                                {
                                    sw.WriteLine(indent + "{");
                                    sw.WriteLine(indent + "\tuint32_t tempBits = 0;");

                                    foreach (KeyValuePair<string, int> bits in fieldDef.Bits!)
                                    {
                                        uint singleBitMask = (uint)(1 << bits.Value);

                                        sw.WriteLine(indent + "\tif (this->m_" + bits.Key + ")");
                                        sw.WriteLine(indent + "\t\ttempBits |= " + singleBitMask.ToString() + "u;");
                                    }

                                    sw.WriteLine(indent + "\tRKIT_CHECK(writer.Write(tempBits));");
                                    sw.WriteLine(indent + "}");
                                }
                                break;
                            default:
                                sw.WriteLine(indent + "RKIT_CHECK(writer.Write(this->m_" + fieldDef.Name + "));");
                                break;
                        }
                    }


                    sw.WriteLine("\t\t\tRKIT_RETURN_OK;");
                    sw.WriteLine("\t\t}");
                    sw.WriteLine("\t};");
                    sw.WriteLine();
                }

                sw.WriteLine("\trkit::Result CreateWindowCommand(rkit::UniquePtr<WindowCommand> &outCommand, uint8_t opcode)");
                sw.WriteLine("\t{");
                sw.WriteLine("\t\tswitch (opcode)");
                sw.WriteLine("\t\t{");
                foreach (WindowCommandDef wcDef in windowCommandDefs)
                {
                    sw.WriteLine("\t\tcase " + wcDef.Opcode.ToString() + ":");
                    sw.WriteLine("\t\t\treturn rkit::New<" + wcDef.Name + "Command>(outCommand);");
                }
                sw.WriteLine("\t\tdefault:");
                sw.WriteLine("\t\t\tRKIT_THROW(rkit::ResultCode::kDataError);");
                sw.WriteLine("\t\t};");
                sw.WriteLine("\t}");

                sw.WriteLine("}");
            }

            using (StreamWriter sw = new StreamWriter(Path.Combine(outDataIncludeDir, "APEWindowCommandOpcodes.generated.h")))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#pragma once");
                sw.WriteLine();
                sw.WriteLine("#include <stdint.h>");
                sw.WriteLine();
                sw.WriteLine("namespace anox::data");
                sw.WriteLine("{");
                sw.WriteLine("\tenum class WindowCommandType : uint8_t");
                sw.WriteLine("\t{");

                foreach (WindowCommandDef wcDef in windowCommandDefs)
                {
                    sw.WriteLine("\t\t" + wcDef.Name + ",");
                }
                sw.WriteLine();
                sw.WriteLine("\t\tCount,");

                sw.WriteLine("\t};");
                sw.WriteLine("}");
            }
        }

        private static List<WindowCommandDef> ParseWindowCommandDefs(string[] defLines)
        {
            List<WindowCommandDef> wcDefs = new List<WindowCommandDef>();

            foreach (string line in defLines)
            {
                List<string> tokens = new List<string>();
                tokens.AddRange(line.Split(' '));

                {
                    string splitChars = "()=";

                    foreach (char splitChar in splitChars)
                    {
                        List<string> reformedTokens = new List<string>();

                        foreach (string baseToken in tokens)
                        {
                            string token = baseToken;

                            int splitPos = token.IndexOf(splitChar);
                            while (splitPos > 0)
                            {
                                reformedTokens.Add(token.Substring(0, splitPos));
                                reformedTokens.Add(splitChar.ToString());
                                token = token.Substring(splitPos + 1);

                                splitPos = token.IndexOf(splitChar);
                            }

                            reformedTokens.Add(token);
                        }

                        tokens = reformedTokens;
                    }

                    List<string> filteredTokens = new List<string>();
                    foreach (string token in tokens)
                    {
                        if (token.Length != 0)
                            filteredTokens.Add(token);
                    }

                    tokens = filteredTokens;
                }

                if (tokens.Count == 0)
                    continue;

                if (tokens.Count < 2)
                    throw new Exception("Unhandled field def: " + line);

                List<FieldDef> fieldDefs = new List<FieldDef>();

                string name = tokens[0];
                byte opcode = byte.Parse(tokens[1]);

                int tokenIndex = 2;

                while (tokenIndex < tokens.Count)
                {
                    string fieldTypeName = tokens[tokenIndex++];

                    if (tokens[tokenIndex++] != "(")
                        throw new Exception("Expected (");

                    if (fieldTypeName == "bits32")
                    {
                        List<KeyValuePair<string, int>> bits = new List<KeyValuePair<string, int>>();

                        while (tokens[tokenIndex] != ")")
                        {
                            string bitName = tokens[tokenIndex++];
                            if (tokens[tokenIndex++] != "=")
                                throw new Exception("Expected =");
                            int bit = int.Parse(tokens[tokenIndex++]);

                            bits.Add(new KeyValuePair<string, int>(bitName, bit));
                        }

                        tokenIndex++;

                        fieldDefs.Add(new FieldDef(bits));
                    }
                    else
                    {
                        string fieldName = tokens[tokenIndex++];
                        if (tokens[tokenIndex++] != ")")
                            throw new Exception("Expected )");

                        FieldDef.FieldType fieldType;
                        if (fieldTypeName == "uint16")
                            fieldType = FieldDef.FieldType.UInt16;
                        else if (fieldTypeName == "uint32")
                            fieldType = FieldDef.FieldType.UInt32;
                        else if (fieldTypeName == "str")
                            fieldType = FieldDef.FieldType.Str;
                        else if (fieldTypeName == "optstr")
                            fieldType = FieldDef.FieldType.OptStr;
                        else if (fieldTypeName == "fmt")
                            fieldType = FieldDef.FieldType.Format;
                        else if (fieldTypeName == "optexpr")
                            fieldType = FieldDef.FieldType.OptExpr;
                        else if (fieldTypeName == "pad")
                            fieldType = FieldDef.FieldType.Padding;
                        else
                            throw new Exception("Unhandled field type " + fieldTypeName);

                        fieldDefs.Add(new FieldDef(fieldType, fieldName));
                    }
                }

                wcDefs.Add(new WindowCommandDef(name, opcode, fieldDefs));
            }

            return wcDefs;
        }
    }
}
