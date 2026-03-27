using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;

namespace GameObjectReflector
{
    internal class ParserState
    {
        public Dictionary<string, DateTime> OldFileTimestamps { get; } = new Dictionary<string, DateTime>();
        public Dictionary<string, DateTime> NewFileTimestamps { get; } = new Dictionary<string, DateTime>();
    }

    internal class PropertyDef
    {
        public string Type { get; set; }
        public string Name { get; set; }
    }

    internal class ClassDef
    {
        public string Name { get; set; }
        public List<PropertyDef> Properties { get; } = new List<PropertyDef>();
    }

    internal class ReflectorState
    {
        public Dictionary<string, ClassDef> ClassDefs = new Dictionary<string, ClassDef>();
    }

    internal class ReflectorException : Exception
    {
        public ReflectorException(string message)
            : base(message)
        {
        }
    }

    internal class Program
    {
        private static string PunctuationChars { get => "():.;/<>,[]{}"; }

        static void Main(string[] args)
        {
            if (args.Length != 2)
                throw new ArgumentException("Usage: GameObjectReflector <file.ec> <output dir>");

            string inputPath = args[0].Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
            string outputBase = args[1].Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);

            if (outputBase.EndsWith(Path.DirectorySeparatorChar))
                outputBase = outputBase.Substring(0, outputBase.Length - 1);

            Directory.CreateDirectory(outputBase);

            CompileFile(inputPath, outputBase);
        }

        private static void CompileFile(string inFilePath, string outFileBase)
        {
            ReflectorState rfState = new ReflectorState();

            List<string> lines = new List<string>();
            using (FileStream inStream = new FileStream(inFilePath, FileMode.Open))
            {
                using (StreamReader reader = new StreamReader(inStream))
                {
                    string? line = reader.ReadLine();
                    while (line != null)
                    {
                        lines.Add(line);
                        line = reader.ReadLine();
                    }
                }
            }

            EntityClassCollection ec = new EntityClassCollection();

            CompileECFile(ec, lines);
        }

        private static void CompileECFile(EntityClassCollection ec, List<string> lines)
        {
            int lineNum = 0;

            while (lineNum < lines.Count)
            {
                List<TypeDefAttribute> typeDefAttribs = new List<TypeDefAttribute>();
                string line = PullLine(lines, ref lineNum);

                int col = 0;
                TokenType tokenType = TokenType.Invalid;
                string? token = TryPullToken(line, ref col, out tokenType);

                if (token == null)
                    continue;

                if (token == "[")
                {
                    typeDefAttribs.Add(ParseTypeDefAttribute(line, ref col));
                    ExpectEndOfLine(line, col);
                }
                else if (token == "class")
                {
                    ec.AddClass(ParseClass(ClassType.Class, typeDefAttribs.ToArray(), lines, ref lineNum, line, col));
                    typeDefAttribs.Clear();
                }
                else if (token == "component")
                {
                    ec.AddComponent(ParseClass(ClassType.Component, typeDefAttribs.ToArray(), lines, ref lineNum, line, col));
                    typeDefAttribs.Clear();
                }
            }
        }

        private static TypeDefAttribute ParseTypeDefAttribute(string line, ref int col)
        {
            string attribType = PullTokenOfType(line, ref col, TokenType.Identifier);

            if (attribType == "level")
                return ParseLevelTypeAttribute(line, ref col);
            else if (attribType == "userentity")
                return ParseUserEntityTypeAttribute(line, ref col);
            else
                throw new ReflectorException("Unknown type attribute");
        }

        private static TypeDefAttribute ParseLevelTypeAttribute(string line, ref int col)
        {
            ExpectToken(line, ref col, "(");
            string levelName = PullTokenOfType(line, ref col, TokenType.Identifier);
            ExpectToken(line, ref col, ")");
            ExpectToken(line, ref col, "]");

            return new LevelTypeDefAttribute(levelName);
        }

        private static TypeDefAttribute ParseUserEntityTypeAttribute(string line, ref int col)
        {
            ExpectToken(line, ref col, "(");
            string edefName = PullTokenOfType(line, ref col, TokenType.Identifier);
            ExpectToken(line, ref col, ")");
            ExpectToken(line, ref col, "]");

            return new UserEntityTypeDefAttribute(edefName);
        }

        private static ClassDef2 ParseClass(ClassType classType, TypeDefAttribute[] typeDefAttributes, IReadOnlyList<string> lines, ref int lineNum, string line, int col)
        {
            string componentName = PullTokenOfType(line, ref col, TokenType.Identifier);

            List<string> parentClasses = new List<string>();
            List<FieldDef> fieldDefs = new List<FieldDef>();

            TokenType tokenType;
            {
                string? extenderToken = TryPullToken(line, ref col, out tokenType);
                if (extenderToken == ":")
                {
                    for (; ; )
                    {
                        parentClasses.Add(PullTokenOfType(line, ref col, TokenType.Identifier));
                        extenderToken = TryPullToken(line, ref col, out tokenType);
                        if (extenderToken != ",")
                            break;
                    }
                }
            }

            ExpectEndOfLine(line, col);

            line = PullNonEmptyLine(lines, ref lineNum, out col);
            ExpectToken(line, ref col, "{");
            ExpectEndOfLine(line, col);

            for (; ; )
            {
                line = PullNonEmptyLine(lines, ref lineNum, out col);

                List<FieldAttribute> fieldAttribs = new List<FieldAttribute>();

                string token = PullToken(line, ref col, out tokenType);

                if (token == "}")
                    break;

                while (token == "[")
                {
                    fieldAttribs.Add(ParseFieldAttribute(line, ref col));
                    token = PullToken(line, ref col, out tokenType);
                }

                if (tokenType != TokenType.Identifier)
                    throw new ReflectorException("Expected identifier");

                FieldType fieldType = DetermineFieldType(token);
                string fieldName = PullToken(line, ref col, out tokenType);

                fieldDefs.Add(new FieldDef(fieldAttribs.ToArray(), fieldName, fieldType));
            }

            return new ClassDef2(classType, typeDefAttributes, parentClasses, fieldDefs.ToArray());
        }

        private static FieldType DetermineFieldType(string token)
        {
            if (token == "vec2")
                return FieldType.Vec2;
            else if (token == "vec3")
                return FieldType.Vec3;
            else if (token == "vec4")
                return FieldType.Vec4;
            else if (token == "bool")
                return FieldType.Bool;
            else if (token == "bytestring")
                return FieldType.ByteString;
            else if (token == "float")
                return FieldType.Float;
            else if (token == "uint")
                return FieldType.UInt;
            else if (token == "label")
                return FieldType.Label;
            else if (token == "bspmodel")
                return FieldType.BspModel;
            else if (token == "broken")
                return FieldType.Broken;
            else if (token == "edef")
                return FieldType.EDef;
            else
                throw new ReflectorException("Unknown field type " + token);
        }

        private static FieldAttribute ParseFieldAttribute(string line, ref int col)
        {
            string attribType = PullTokenOfType(line, ref col, TokenType.Identifier);

            if (attribType == "level")
                return ParseLevelFieldAttribute(line, ref col);
            else if (attribType == "alias")
                return ParseAliasFieldAttribute(line, ref col);
            else
                throw new ReflectorException("Unknown field attribute");
        }

        private static FieldAttribute ParseAliasFieldAttribute(string line, ref int col)
        {
            ExpectToken(line, ref col, "(");
            string aliasName = PullTokenOfType(line, ref col, TokenType.Identifier);
            ExpectToken(line, ref col, ")");
            ExpectToken(line, ref col, "]");

            return new AliasFieldAttribute(aliasName);
        }

        private static FieldAttribute ParseLevelFieldAttribute(string line, ref int col)
        {
            LevelFieldAttribute attrib = new LevelFieldAttribute();
            for (; ; )
            {
                TokenType tokenType;
                string nextToken = PullToken(line, ref col, out tokenType);
                if (nextToken == "]")
                    break;

                if (nextToken == "scalarized")
                {
                    ExpectToken(line, ref col, "(");

                    List<string> subFields = new List<string>();

                    for (; ; )
                    {
                        nextToken = PullTokenOfType(line, ref col, TokenType.Identifier);

                        subFields.Add(nextToken);
                        nextToken = PullToken(line, ref col, out tokenType);
                        if (nextToken == ")")
                            break;
                        else if (nextToken != ",")
                            throw new ReflectorException("Expected ',' or ')' after scalarized subfield");
                    }

                    if (attrib.ScalarizedFields != null)
                        throw new ReflectorException("Scalarized subfields already specified");

                    attrib.ScalarizedFields = subFields.ToArray();
                }
                else if (nextToken == "name")
                {
                    ExpectToken(line, ref col, "(");
                    string overrideName = PullTokenOfType(line, ref col, TokenType.Identifier);
                    ExpectToken(line, ref col, ")");

                    attrib.OverrideName = overrideName;
                }
                else if (nextToken == "onoff")
                {
                    attrib.IsOnOff = true;
                }
                else
                    throw new ReflectorException("Unknown sub-attribute for level attribute");
            }

            return attrib;
        }

        private static void WriteInlineFile(ReflectorState rfState, string fileName, TextWriter writer)
        {
            List<string> sortedKeys = new List<string>(rfState.ClassDefs.Keys);
            sortedKeys.Sort();

            foreach (string typeName in sortedKeys)
            {
                ClassDef classDef = rfState.ClassDefs[typeName];

                writer.WriteLine("#include \"Serializer.h\"");
                writer.WriteLine();

                writer.Write("::rkit::Result ");
                writer.Write(typeName);
                writer.WriteLine("::Serialize(::anox::game::ISerializer &serializer)");
                writer.WriteLine("{");

                foreach (PropertyDef prop in classDef.Properties)
                {
                    writer.Write("\tRKIT_CHECK(serializer.Serialize(this->");
                    writer.Write(prop.Name);
                    writer.WriteLine("));");
                }

                writer.WriteLine("\tRKIT_RETURN_OK;");

                writer.WriteLine("}");
            }
        }

        private static void CompileFileContents(IReadOnlyList<string> lines, ReflectorState rfState)
        {
            for (int lineIndex = 0; lineIndex < lines.Count;)
            {
                string line = lines[lineIndex];

                if (IsReflectorStartLine(line))
                {
                    ParseReflectorDirective(rfState, lines, ref lineIndex);
                }
                else
                    lineIndex++;
            }
        }

        private static string PullReflectorDirective(string line, out int continuePos)
        {
            continuePos = 0;
            SkipWhitespace(line, ref continuePos);

            continuePos += "/*".Length;   // Skip /*
            SkipWhitespace(line, ref continuePos);

            continuePos += "reflector".Length;

            ExpectToken(line, ref continuePos, ":");

            TokenType tokenType;
            return PullToken(line, ref continuePos, out tokenType);
        }

        private static void ParseReflectorDirective(ReflectorState rfState, IReadOnlyList<string> lines, ref int lineIndex)
        {
            string line = PullLine(lines, ref lineIndex);

            int pos = 0;
            string directiveType = PullReflectorDirective(line, out pos);

            if (directiveType == "properties")
            {
                ParsePropertiesDirective(rfState, lines, line, pos, ref lineIndex);
            }
            else
            {
                throw new ReflectorException("Unknown directive: " + directiveType);
            }
        }

        private static void ParsePropertiesDirective(ReflectorState rfState, IReadOnlyList<string> lines, string directiveLine, int pos, ref int lineIndex)
        {
            ExpectToken(directiveLine, ref pos, "(");

            string typeName = PullTypeName(directiveLine, ref pos);

            ExpectToken(directiveLine, ref pos, ")");

            ClassDef classDef;
            if (!rfState.ClassDefs.TryGetValue(typeName, out classDef))
            {
                classDef = new ClassDef() { Name = typeName };
                rfState.ClassDefs[typeName] = classDef;
            }

            for (; ; )
            {
                if (lineIndex == lines.Count)
                    throw new ReflectorException("Unexpected end of file while searching for 'endproperties' directive");

                string line = lines[lineIndex++];
                {
                    if (IsReflectorStartLine(line))
                    {
                        int reflectorPos = 0;
                        string directive = PullReflectorDirective(line, out reflectorPos);

                        if (directive == "endproperties")
                            return;
                        else
                            throw new ReflectorException("Directive " + directive + " is not supported in a properties context");
                    }
                }

                ParsePropertyLine(classDef, line);
            }

            throw new NotImplementedException();
        }

        private static void ParsePropertyLine(ClassDef classDef, string line)
        {
            int pos = 0;
            SkipWhitespace(line, ref pos);

            if (pos == line.Length)
                return;

            {
                int tokenCheckPos = pos;
                TokenType tokenCheckType;
                string token = PullToken(line, ref tokenCheckPos, out tokenCheckType);

                if (token == "//")
                    return;
            }

            string propertyType = PullTypeName(line, ref pos);

            string propertyIdentifier = PullTokenOfType(line, ref pos, TokenType.Identifier);

            classDef.Properties.Add(new PropertyDef() { Name = propertyIdentifier, Type = propertyType });
        }

        private static string PullTypeName(string line, ref int pos)
        {
            string firstIdentifier = PullTokenOfType(line, ref pos, TokenType.Identifier);

            StringBuilder sb = new StringBuilder(firstIdentifier);

            int bracketDepth = 0;

            for (; ; )
            {
                int tempPos = pos;

                TokenType tokenType;
                string? nextToken = TryPullToken(line, ref tempPos, out tokenType);

                if (nextToken == "::")
                {
                    pos = tempPos;
                    string nextIdentifier = PullTokenOfType(line, ref pos, TokenType.Identifier);
                    sb.Append("::");
                    sb.Append(nextIdentifier);
                }
                else if (nextToken == ",")
                {
                    if (bracketDepth == 0)
                        break;

                    sb.Append(", ");
                    pos = tempPos;
                }
                else if (nextToken == "<")
                {
                    bracketDepth++;
                    pos = tempPos;

                    string nextIdentifier = PullTokenOfType(line, ref pos, TokenType.Identifier);
                    sb.Append("<");
                    sb.Append(nextIdentifier);
                }
                else if (nextToken == ">")
                {
                    if (bracketDepth == 0)
                        break;

                    bracketDepth--;
                    sb.Append(">");
                    pos = tempPos;
                }
                else
                    break;
            }

            return sb.ToString();
        }

        private static string? TryPullToken(string line, ref int pos, out TokenType tokenType)
        {
            SkipWhitespace(line, ref pos);

            if (pos == line.Length)
            {
                tokenType = TokenType.Invalid;
                return null;
            }

            char firstCh = line[pos];
            if (firstCh == '_' || IsAlpha(firstCh))
            {
                tokenType = TokenType.Identifier;
                return PullIdentifier(line, ref pos);
            }
            else if (PunctuationChars.Contains(firstCh))
            {
                tokenType = TokenType.Punctuation;
                return PullPunctuation(line, ref pos);
            }
            else
                throw new ReflectorException("Unrecognized token");
        }

        private static void ExpectToken(string line, ref int pos, string expected)
        {
            TokenType tokenType;
            string token = PullToken(line, ref pos, out tokenType);

            if (token != expected)
                throw new ReflectorException("Expected " + expected);
        }

        private static void ExpectEndOfLine(string line, int pos)
        {
            TokenType tokenType;
            string? token = TryPullToken(line, ref pos, out tokenType);

            if (token == null || token == "//")
                return;

            throw new ReflectorException("Expected end of line");
        }

        private static string PullTokenOfType(string line, ref int pos, TokenType expectedType)
        {
            TokenType actualType;
            string token = PullToken(line, ref pos, out actualType);

            if (actualType != expectedType)
                throw new ReflectorException("Expected token of type " + expectedType.ToString() + " but found " + actualType.ToString());

            return token;
        }

        private static string PullToken(string line, ref int pos, out TokenType tokenType)
        {
            string? token = TryPullToken(line, ref pos, out tokenType);

            if (token == null)
                throw new ReflectorException("Expected token");

            return token;
        }

        private static string PullPunctuation(string line, ref int pos)
        {
            int availableChars = line.Length - pos;

            int startPos = pos;
            char ch = line[pos];
            if (ch == ':' && availableChars >= 2 && line[pos + 1] == ':')
                pos += 2;
            else if (ch == '/' && availableChars >= 2 && line[pos + 1] == '/')
                pos += 2;
            else
                pos++;

            return line.Substring(startPos, pos - startPos);
        }

        private static string PullIdentifier(string line, ref int pos)
        {
            int startPos = pos;

            while (pos < line.Length)
            {
                char ch = line[pos];

                if (ch == '_' || IsAlpha(ch) || IsNumeric(ch))
                    pos++;
                else
                    break;
            }

            return line.Substring(startPos, pos - startPos);
        }

        private static bool IsAlpha(char ch)
        {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        }

        private static bool IsNumeric(char ch)
        {
            return (ch >= '0' && ch <= '9');
        }

        private static string PullLine(IReadOnlyList<string> lines, ref int lineIndex)
        {
            if (lineIndex < lines.Count)
                return lines[lineIndex++];

            throw new IndexOutOfRangeException("File ended when more lines were expected");
        }

        private static string PullNonEmptyLine(IReadOnlyList<string> lines, ref int lineIndex, out int col)
        {
            for (; ; )
            {
                string line = PullLine(lines, ref lineIndex);
                col = 0;
                TokenType tokenType;
                if (TryPullToken(line, ref col, out tokenType) != null)
                {
                    col = 0;
                    SkipWhitespace(line, ref col);
                    return line;
                }
            }
        }

        private static bool IsWhitespace(char ch)
        {
            return ch >= 0 && ch <= ' ';
        }

        private static void SkipWhitespace(string str, ref int index)
        {
            while (index < str.Length && IsWhitespace(str[index]))
                index++;
        }

        private static bool IsReflectorStartLine(string line)
        {
            int reflectorDirectiveStart = 0;

            int pos = 0;
            SkipWhitespace(line, ref pos);

            if (!LineContinuesWith(line, pos, "/*"))
                return false;

            pos += 2;
            SkipWhitespace(line, ref pos);

            if (LineContinuesWith(line, pos, "reflector"))
                return true;

            return false;
        }

        private static bool LineContinuesWith(string line, int pos, string v)
        {
            int candidateLength = v.Length;

            if (line.Length - pos < candidateLength)
                return false;

            return line.AsSpan().Slice(pos, candidateLength).SequenceEqual(v.AsSpan());
        }
    }

    internal enum TokenType
    {
        Identifier,
        Punctuation,
        Number,
        Invalid,
    }
}
