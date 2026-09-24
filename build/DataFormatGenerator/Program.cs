using BuildToolsCommon;
using System.Data.SqlTypes;
using System.Security.AccessControl;

namespace DataFormatGenerator
{
    public class ByteBlobLexer
    {
        private class ByteCharConverter : ICharConverter<byte>
        {
            static int ICharConverter<byte>.ToCharCode(byte ch)
            {
                return ch;
            }
        }
        private Lexer<byte, ByteCharConverter, byte[]> _lexer;

        public ByteBlobLexer(byte[] blob)
        {
            _lexer = new Lexer<byte, ByteCharConverter, byte[]>(blob);
        }

        public Token? GetToken()
        {
            return _lexer.GetToken();
        }

        public string TokenToString(Token token)
        {
            return _lexer.TokenToString(token);
        }

        public bool TokenIsString(Token token, string str)
        {
            return _lexer.TokenIsString(token, str);
        }

        public Token? PeekToken()
        {
            LexerState backupState = _lexer.State;

            Token? token = _lexer.GetToken();

            _lexer.State = backupState;

            return token;
        }

        internal void Expect(string str)
        {
            Token token = ExpectToken();

            if (!_lexer.TokenIsString(token, str))
                throw MakeTokenException(token, "Expected '" + str + "'");
        }

        internal Token ExpectToken()
        {
            Token? token = _lexer.GetToken();

            if (token == null)
                throw new Exception("Unexpected end of file");

            return token.Value;
        }

        internal Token ExpectTokenOfType(TokenType tokenType)
        {
            Token token = ExpectToken();

            if (token.TokenType != tokenType)
                throw MakeTokenException(token, "Expected token of type " + tokenType.ToString());

            return token;
        }

        internal static Exception MakeTokenException(Token token, string msg)
        {
            return new Exception(token.ToString() + ": " + msg);
        }

        internal string? UnquoteString(Token token)
        {
            return _lexer.UnquoteString(token);
        }
    }

    internal class FormatBuilder
    {
        IList<string>? _namespace = null;
        DeduplicatedDict<string, StructDef> _structs = new DeduplicatedDict<string, StructDef>();
        DeduplicatedList<TypeDef> _dynArrayTypes = new DeduplicatedList<TypeDef>();

        public IList<string>? Namespace
        {
            get { return _namespace; }

            set
            {
                if (_namespace != null)
                    throw new Exception("Namespace was specified multiple times");

                _namespace = value;
            }
        }

        public string? LoaderPath { get; private set; }
        public string? FormatCode { get; private set; }
        public string? BuilderPath { get; private set; }
        public StructDef? FormatType { get; private set; }

        internal void ParseNamespace(ByteBlobLexer lexer)
        {
            List<string> namespaceChunks = new List<string>();

            for (; ; )
            {
                Token nextToken = lexer.ExpectTokenOfType(TokenType.Identifier);

                namespaceChunks.Add(lexer.TokenToString(nextToken));

                Token? sep = lexer.PeekToken();
                if (!sep.HasValue || !lexer.TokenIsString(sep.Value, "."))
                    break;

                lexer.GetToken();
            }

            Namespace = namespaceChunks;
        }

        internal void ParseLoader(ByteBlobLexer lexer)
        {
            Token pathToken = lexer.ExpectTokenOfType(TokenType.QuotedString);

            LoaderPath = lexer.UnquoteString(pathToken);
        }

        internal void ParseBuilder(ByteBlobLexer lexer)
        {
            Token pathToken = lexer.ExpectTokenOfType(TokenType.QuotedString);

            BuilderPath = lexer.UnquoteString(pathToken);
        }

        internal void ParseStruct(ByteBlobLexer lexer)
        {
            string name = lexer.TokenToString(lexer.ExpectTokenOfType(TokenType.Identifier));
            bool isDeduplicated = false;
            bool isInstanced = false;

            for (; ; )
            {
                Token attribToken = lexer.ExpectToken();

                if (lexer.TokenIsString(attribToken, "{"))
                    break;

                if (lexer.TokenIsString(attribToken, "deduplicated"))
                    isDeduplicated = true;
                else if (lexer.TokenIsString(attribToken, "instanced"))
                    isInstanced = true;
                else
                    throw ByteBlobLexer.MakeTokenException(attribToken, "Unexpected token");
            }

            List<StructMember> members = new List<StructMember>();

            for (; ; )
            {
                Token typeToken = lexer.ExpectToken();

                if (lexer.TokenIsString(typeToken, "}"))
                    break;

                TypeDef typeDef = ParseTypeDef(lexer, typeToken, !isDeduplicated);

                Token nameToken = lexer.ExpectTokenOfType(TokenType.Identifier);

                members.Add(new StructMember(typeDef, lexer.TokenToString(nameToken)));
            }

            StructDef structDef = new StructDef(name, members, isDeduplicated, isInstanced);

            _structs.Add(name, structDef);
        }

        private TypeDef ParseTypeDef(ByteBlobLexer lexer, Token typeToken, bool dynArraysAllowed)
        {
            if (lexer.TokenIsString(typeToken, "uint8"))
                return new TypeDef(TypeDefKind.UInt8);
            else if (lexer.TokenIsString(typeToken, "uint16"))
                return new TypeDef(TypeDefKind.UInt16);
            else if (lexer.TokenIsString(typeToken, "uint32"))
                return new TypeDef(TypeDefKind.UInt32);
            else if (lexer.TokenIsString(typeToken, "int8"))
                return new TypeDef(TypeDefKind.SInt8);
            else if (lexer.TokenIsString(typeToken, "int16"))
                return new TypeDef(TypeDefKind.SInt16);
            else if (lexer.TokenIsString(typeToken, "int32"))
                return new TypeDef(TypeDefKind.SInt32);
            else if (lexer.TokenIsString(typeToken, "float"))
                return new TypeDef(TypeDefKind.Float32);
            else if (lexer.TokenIsString(typeToken, "double"))
                return new TypeDef(TypeDefKind.Float64);
            else if (lexer.TokenIsString(typeToken, "contentid"))
                return new TypeDef(TypeDefKind.ContentID);
            else if (lexer.TokenIsString(typeToken, "bool"))
                return new TypeDef(TypeDefKind.Bool);
            else if (lexer.TokenIsString(typeToken, "array"))
            {
                lexer.Expect("(");

                Token firstSubTypeToken = lexer.ExpectTokenOfType(TokenType.Identifier);

                TypeDef subType = ParseTypeDef(lexer, firstSubTypeToken, dynArraysAllowed);

                lexer.Expect(",");

                Token sizeToken = lexer.ExpectTokenOfType(TokenType.Number);

                int count = 0;
                if (!int.TryParse(lexer.TokenToString(sizeToken), out count))
                    throw ByteBlobLexer.MakeTokenException(sizeToken, "Invalid size");

                lexer.Expect(")");

                return new TypeDef(TypeDefKind.FixedArray, count, subType);
            }
            else if (lexer.TokenIsString(typeToken, "dynarray"))
            {
                if (!dynArraysAllowed)
                    throw ByteBlobLexer.MakeTokenException(typeToken, "Dynamic arrays are not allowed in deduplicated types");

                lexer.Expect("(");

                Token firstSubTypeToken = lexer.ExpectTokenOfType(TokenType.Identifier);

                TypeDef subType = ParseTypeDef(lexer, firstSubTypeToken, dynArraysAllowed);

                lexer.Expect(")");

                AddDynArrayType(subType);

                return new TypeDef(TypeDefKind.DynArray, subType);
            }
            else if (lexer.TokenIsString(typeToken, "clusterarray"))
            {
                if (!dynArraysAllowed)
                    throw ByteBlobLexer.MakeTokenException(typeToken, "Dynamic arrays are not allowed in deduplicated types");

                lexer.Expect("(");

                Token firstSubTypeToken = lexer.ExpectTokenOfType(TokenType.Identifier);

                TypeDef subType = ParseTypeDef(lexer, firstSubTypeToken, dynArraysAllowed);

                lexer.Expect(")");

                if (subType.Kind != TypeDefKind.Struct || (!subType.StructDef!.IsInstanced && !subType.StructDef!.IsDeduplicated))
                    throw ByteBlobLexer.MakeTokenException(firstSubTypeToken, "Cluster arrays are only valid for instanced or deduplicated structs");

                AddDynArrayType(new TypeDef(TypeDefKind.ClusterRef, subType));

                return new TypeDef(TypeDefKind.ClusterArray, subType);
            }
            else if (lexer.TokenIsString(typeToken, "invertible"))
            {
                lexer.Expect("(");

                Token firstSubTypeToken = lexer.ExpectTokenOfType(TokenType.Identifier);

                TypeDef subType = ParseTypeDef(lexer, firstSubTypeToken, dynArraysAllowed);

                lexer.Expect(")");

                return new TypeDef(TypeDefKind.Invertible, subType);
            }
            else
            {
                string structName = lexer.TokenToString(typeToken);

                StructDef? structDef;
                if (_structs.TryGetValue(structName, out structDef))
                    return new TypeDef(TypeDefKind.Struct, structDef);

                throw ByteBlobLexer.MakeTokenException(typeToken, "Unrecognized type token '" + structName + "'");
            }
        }

        private void AddDynArrayType(TypeDef typeDef)
        {
            switch (typeDef.Kind)
            {
                case TypeDefKind.Bool:
                case TypeDefKind.UInt8:
                case TypeDefKind.UInt16:
                case TypeDefKind.UInt32:
                case TypeDefKind.SInt8:
                case TypeDefKind.SInt16:
                case TypeDefKind.SInt32:
                case TypeDefKind.Float32:
                case TypeDefKind.Float64:
                case TypeDefKind.Struct:
                case TypeDefKind.ContentID:
                    _dynArrayTypes.Add(typeDef);
                    return;

                case TypeDefKind.DynArray:
                case TypeDefKind.ClusterArray:
                case TypeDefKind.ClusterRef:
                case TypeDefKind.FixedArray:
                case TypeDefKind.Invertible:
                    AddDynArrayType(typeDef.SubType!);
                    _dynArrayTypes.Add(typeDef);
                    return;

                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        internal void ParseFormatType(ByteBlobLexer lexer)
        {
            lexer.Expect("(");
            Token formatCodeToken = lexer.ExpectTokenOfType(TokenType.Identifier);

            lexer.Expect(")");

            FormatCode = lexer.TokenToString(formatCodeToken);

            Token typeToken = lexer.ExpectTokenOfType(TokenType.Identifier);

            StructDef structDef;
            if (!_structs.TryGetValue(lexer.TokenToString(typeToken), out structDef))
                throw ByteBlobLexer.MakeTokenException(typeToken, "'" + lexer.TokenToString(typeToken) + "' isn't a struct name");

            FormatType = structDef;
        }

        internal void Export()
        {
            if (Namespace == null)
                throw new Exception("No namespace was specified");

            if (LoaderPath == null)
                throw new Exception("No loader path was specified");

            if (FormatCode == null)
                throw new Exception("No format code was specified");

            if (BuilderPath == null)
                throw new Exception("No builder path was specified");

            if (FormatType == null)
                throw new Exception("No format type was specified");

            string fullNamespace = "";
            foreach (string part in Namespace)
                fullNamespace = fullNamespace + "::" + part;

            List<byte> hashInput = new List<byte>();

            hashInput.AddRange(IntToBytes(Namespace.Count));
            foreach (string part in Namespace)
                AddStringToHash(hashInput, part);

            KeyValuePair<string, StructDef>[] unrolledStructs = _structs.Unroll();

            Dictionary<StructDef, int> structDefToIndex = new Dictionary<StructDef, int>(); ;

            {
                int index = 0;

                foreach (KeyValuePair<string, StructDef> structDef in unrolledStructs)
                {
                    structDefToIndex.Add(structDef.Value, index);
                    index++;
                }
            }

            hashInput.AddRange(IntToBytes(unrolledStructs.Length));
            foreach (KeyValuePair<string, StructDef> structDef in unrolledStructs)
                AddStructDefToHash(hashInput, structDef.Value, structDefToIndex);

            hashInput.AddRange(IntToBytes(structDefToIndex[FormatType]));

            uint versionCode = 0;

            {
                byte[] versionHash = System.Security.Cryptography.SHA256.HashData(hashInput.ToArray());
                for (int i = 0; i < 4; i++)
                    versionCode = (versionCode << 8) + versionHash[i];
            }

            using (StreamWriter sw = new BuildToolsCommon.WriteIfChangedStreamWriter(this.BuilderPath + ".generated.h"))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#pragma once");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Data/ContentID.h\"");
                sw.WriteLine("#include \"rkit/Data/Invertible.h\"");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Core/RefCounted.h\"");
                sw.WriteLine();
                sw.Write("namespace ");

                foreach (string part in Namespace)
                {
                    sw.Write(part);
                    sw.Write("::");
                }
                sw.WriteLine("builder");

                sw.WriteLine("{");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;
                    sw.WriteLine("\tstruct " + structDef.Name);
                    sw.WriteLine("\t{");
                    sw.Write("\t\tstatic constexpr size_t kContentsSize = ");

                    {
                        bool isFirst = true;
                        foreach (StructMember member in structDef.Members)
                        {
                            if (isFirst)
                                isFirst = false;
                            else
                                sw.Write(" + ");

                            sw.Write(MemberSizeString(member));
                        }
                    }
                    sw.WriteLine(";");


                    sw.Write("\t\tstatic constexpr size_t kInlineSize = ");
                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                        sw.Write("sizeof(uint32_t)");
                    else
                        sw.Write("kContentsSize");

                    sw.WriteLine(";");
                    sw.WriteLine();

                    foreach (StructMember member in structDef.Members)
                    {
                        TypeDef typeDef = member.Type;

                        sw.Write("\t\t");
                        sw.Write(BuilderName(typeDef));
                        sw.WriteLine(" m_" + member.Name + ";");
                    }

                    sw.WriteLine("\t};");
                }

                sw.WriteLine("}");
            }

            using (StreamWriter sw = new BuildToolsCommon.WriteIfChangedStreamWriter(this.BuilderPath + ".generated.inl"))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#include \"" + Path.GetFileName(this.BuilderPath) + ".generated.h\"");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Core/BufferStream.h\"");
                sw.WriteLine("#include \"rkit/Core/HashTable.h\"");
                sw.WriteLine("#include \"rkit/Data/ByteBlob.h\"");
                sw.WriteLine("#include \"rkit/Data/DataFormatWriter.h\"");
                sw.WriteLine();
                sw.Write("namespace ");

                foreach (string part in Namespace)
                {
                    sw.Write(part);
                    sw.Write("::");
                }
                sw.WriteLine("builder");

                sw.WriteLine("{");
                sw.WriteLine("\tclass " + FormatType!.Name + "_Builder");
                sw.WriteLine("\t{");
                sw.WriteLine("\tpublic:");
                sw.WriteLine("\t\tstatic void WriteToStream(::rkit::IWriteStream &outStream, const " + FormatType!.Name + " &obj);");

                sw.WriteLine();
                sw.WriteLine("\tprivate:");
                sw.WriteLine("\t\tvoid InternalWriteToStream(::rkit::IWriteStream &outStream, const " + FormatType!.Name + " &obj);");
                sw.WriteLine();

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsInstanced)
                        sw.WriteLine("\t\t::rkit::HashMap<const " + structDef.Name + "*, uint32_t> m_instancesOf_" + structDef.Name + ";");
                    else if (structDef.IsDeduplicated)
                        sw.WriteLine("\t\t::rkit::HashMap<::rkit::data::ByteBlob<" + structDef.Name + "::kContentsSize>, uint32_t> m_instancesOf_" + structDef.Name + ";");
                }

                sw.WriteLine();

                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                {
                    sw.WriteLine("\t\t::rkit::Vector<::rkit::data::ByteBlob<" + TypeInlineSizeString(typeDef) + ">> m_dynArraysOf_" + DynArrayNameString(typeDef) + ";");
                }

                sw.WriteLine();

                string[] baseTypes = { "uint8_t", "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t", "float", "double", "::rkit::data::ContentID" };

                foreach (string baseType in baseTypes)
                {
                    sw.WriteLine();
                    sw.WriteLine("\t\tvoid WriteInlineStruct(::rkit::Span<uint8_t> &outSpan, const " + baseType + " &obj)");
                    sw.WriteLine("\t\t{");
                    sw.WriteLine("\t\t\t::rkit::data::DataFormatWriter::WriteOne(outSpan, obj);");
                    sw.WriteLine("\t\t}");
                }

                string[] staticLines =
                {
                    "template<class TClass>",
                    "void WriteInstances(::rkit::IWriteStream &outStream, const ::rkit::HashMap<const TClass *, uint32_t> &hashMap)",
                    "{",
                    "\t::rkit::Vector<::rkit::data::ByteBlob<TClass::kContentsSize>> contentBlobs;",
                    "\tcontentBlobs.Resize(hashMap.Count());",
                    "\tfor (const ::rkit::HashMapKeyValueView<const TClass *, const uint32_t> &kvp : hashMap)",
                    "\t{",
                    "\t\t::rkit::Span<uint8_t> contentsSpan = contentBlobs[kvp.Value()].ModifyStaticArray().ToSpan();",
                    "\t\tWriteStructContents(contentsSpan, *kvp.Key());",
                    "\t}",
                    "\t::rkit::data::DataFormatWriter::WriteBlobs(outStream, contentBlobs.ToSpan());",
                    "}",
                    "",
                    "template<size_t TInlineSize, class TInlineObject>",
                    "void AddArrayItems(::rkit::Vector<::rkit::data::ByteBlob<TInlineSize>> &outItems,",
                    "\t::rkit::Span<const TInlineObject> inItems,",
                    "\tvoid (" + FormatType!.Name + "_Builder:: *writeStructContents)(::rkit::Span<uint8_t> &, const TInlineObject &))",
                    "{",
                    "\tfor (const TInlineObject &inlineObject : inItems)",
                    "\t{",
                    "\t\t::rkit::data::ByteBlob<TInlineSize> outItem;",
                    "\t\t::rkit::Span<uint8_t> outItemContents = outItem.ModifyStaticArray().ToSpan();",
                    "\t\t(this->*writeStructContents)(outItemContents, inlineObject);",
                    "\t\toutItems.Append(outItem);",
                    "\t}",
                    "}",
                    "",
                    "template<class TInlineObject>",
                    "uint32_t AddClusterItems(::rkit::Vector<::rkit::data::ByteBlob<8>> &outItems,",
                    "\t::rkit::Span<const TInlineObject> inItems,",
                    "\tuint32_t(BSPFile_Builder:: *resolveStructReference)(const TInlineObject &))",
                    "{",
                    "\t::rkit::Vector<uint32_t> clusterArray;",
                    "\tfor (const TInlineObject &inItem : inItems)",
                    "\t{",
                    "\t\tconst uint32_t itemIndex = (this->*resolveStructReference)(inItem);",
                    "\t\tclusterArray.Append(itemIndex);",
                    "\t}",
                    "\treturn ::rkit::data::DataFormatWriter::Clusterize(outItems, clusterArray);",
                    "}",
                    "",
                    "void WriteInlineStruct(::rkit::Span<uint8_t> &outSpan, const bool &obj)",
                    "{",
                    "\t::rkit::data::DataFormatWriter::WriteOne(outSpan, static_cast<uint8_t>(obj ? 1 : 0));",
                    "}",
                };

                sw.WriteLine();

                foreach (string aaiLine in staticLines)
                {
                    if (aaiLine == "")
                        sw.WriteLine();
                    else
                        sw.WriteLine("\t\t" + aaiLine);
                }

                sw.WriteLine();

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsDeduplicated || TypeContainsInstances(new TypeDef(TypeDefKind.Struct, structDef), TypeDefRefBehavior.ScanMembers))
                        sw.WriteLine("\t\tvoid CollectInstances(const " + structDef.Name + " &obj);");

                    if (structDef.IsDeduplicated)
                        sw.WriteLine("\t\tvoid CollectInstances(const ::rkit::data::builder::InvertibleBuilder<" + structDef.Name + "> &obj);");

                    if (structDef.IsInstanced)
                        sw.WriteLine("\t\tvoid CollectInstances(const ::rkit::RCPtr<" + structDef.Name + "> &ref);");
                }

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    sw.WriteLine();

                    sw.WriteLine("\t\tvoid WriteStructContents(::rkit::Span<uint8_t> &outSpan, const " + structDef.Name + " &obj)");
                    sw.WriteLine("\t\t{");

                    foreach (StructMember member in structDef.Members)
                        EmitWriteLines(sw, member.Type, "\t\t\t", "outSpan", "obj.m_" + member.Name);

                    sw.WriteLine("\t\t}");

                    string instanceSignature = "";

                    if (structDef.IsInstanced)
                    {
                        instanceSignature = "::rkit::RCPtr<" + structDef.Name + ">";
                        sw.WriteLine();
                        sw.WriteLine("\t\tuint32_t ResolveStructReference(const " + instanceSignature + " &ref)");
                        sw.WriteLine("\t\t{");
                        sw.WriteLine("\t\t\tconst " + structDef.Name + "* ptr = ref.Get();");
                        sw.WriteLine("\t\t\treturn ::rkit::data::DataFormatWriter::Deduplicate(m_instancesOf_" + structDef.Name + ", ptr);");
                        sw.WriteLine("\t\t}");

                    }
                    else if (structDef.IsDeduplicated)
                    {
                        instanceSignature = structDef.Name;

                        sw.WriteLine();
                        sw.WriteLine("\t\tuint32_t ResolveStructReference(const " + instanceSignature + " &ref)");
                        sw.WriteLine("\t\t{");
                        sw.WriteLine("\t\t\t::rkit::data::ByteBlob<" + structDef.Name + "::kContentsSize> contents;");
                        sw.WriteLine("\t\t\t::rkit::Span<uint8_t> contentsSpan = contents.ModifyStaticArray().ToSpan();");
                        sw.WriteLine("\t\t\tWriteStructContents(contentsSpan, ref);");
                        sw.WriteLine("\t\t\treturn ::rkit::data::DataFormatWriter::Deduplicate(m_instancesOf_" + structDef.Name + ", contents);");
                        sw.WriteLine("\t\t}");
                    }

                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                    {
                        sw.WriteLine();
                        sw.WriteLine("\t\tvoid WriteInlineStruct(::rkit::Span<uint8_t> &outSpan, const " + instanceSignature + " &ref)");
                        sw.WriteLine("\t\t{");
                        sw.WriteLine("\t\t\tconst uint32_t instanceID = ResolveStructReference(ref);");
                        sw.WriteLine("\t\t\t::rkit::data::DataFormatWriter::WriteOne(outSpan, instanceID);");
                        sw.WriteLine("\t\t}");
                        sw.WriteLine();
                        sw.WriteLine("\t\tvoid WriteInlineStruct(::rkit::Span<uint8_t> &outSpan, const ::rkit::data::builder::InvertibleBuilder<" + instanceSignature + "> &ref)");
                        sw.WriteLine("\t\t{");
                        sw.WriteLine("\t\t\tuint32_t instanceID = ResolveStructReference(ref.GetValue());");
                        sw.WriteLine("\t\t\tif (instanceID > 0x7fffffffu)");
                        sw.WriteLine("\t\t\t\tRKIT_THROW(::rkit::ResultCode::kIntegerOverflow);");
                        sw.WriteLine("\t\t\tinstanceID <<= 1;");
                        sw.WriteLine("\t\t\tif (ref.IsInverted())");
                        sw.WriteLine("\t\t\t\tinstanceID |= 1;");
                        sw.WriteLine("\t\t\t::rkit::data::DataFormatWriter::WriteOne(outSpan, instanceID);");
                        sw.WriteLine("\t\t}");
                    }
                    else
                    {
                        sw.WriteLine("\t\tvoid WriteInlineStruct(::rkit::Span<uint8_t> &outSpan, const " + structDef.Name + " &obj)");
                        sw.WriteLine("\t\t{");
                        sw.WriteLine("\t\t\tWriteStructContents(outSpan, obj);");
                        sw.WriteLine("\t\t}");
                    }
                }

                sw.WriteLine("\t};");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsDeduplicated || TypeContainsInstances(new TypeDef(TypeDefKind.Struct, structDef), TypeDefRefBehavior.ScanMembers))
                    {
                        sw.WriteLine();

                        sw.WriteLine("\tvoid " + FormatType!.Name + "_Builder::CollectInstances(const " + structDef.Name + " &obj)");
                        sw.WriteLine("\t{");

                        foreach (StructMember member in structDef.Members)
                            EmitCollectInstancesLinesForStructMember(sw, member.Type, "\t\t", "obj.m_" + member.Name);

                        if (structDef.IsDeduplicated)
                            sw.WriteLine("\t\tResolveStructReference(obj);");

                        sw.WriteLine("\t}");
                    }

                    if (structDef.IsDeduplicated)
                    {
                        sw.WriteLine();
                        sw.WriteLine("\tvoid " + FormatType!.Name + "_Builder::CollectInstances(const ::rkit::data::builder::InvertibleBuilder<" + structDef.Name + "> &ref)");
                        sw.WriteLine("\t{");
                        sw.WriteLine("\t\tCollectInstances(ref.GetValue());");
                        sw.WriteLine("\t}");
                    }

                    if (structDef.IsInstanced)
                    {
                        sw.WriteLine();

                        string collectExpr = "::rkit::data::DataFormatWriter::CollectInstance(m_instancesOf_" + structDef.Name + ", ref.Get())";

                        sw.WriteLine("\tvoid " + FormatType!.Name + "_Builder::CollectInstances(const ::rkit::RCPtr<" + structDef.Name + "> &ref)");
                        sw.WriteLine("\t{");

                        if (TypeContainsInstances(new TypeDef(TypeDefKind.Struct, structDef), TypeDefRefBehavior.ScanMembers))
                        {
                            sw.WriteLine("\t\tif (" + collectExpr + ")");
                            sw.WriteLine("\t\t\tCollectInstances(*ref);");
                        }
                        else
                            sw.WriteLine("\t\t" + collectExpr + ";");

                        sw.WriteLine("\t}");
                    }
                }

                int headerSize = FormatCode.Length + 4;
                int numInstanceBlobs = 0;

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                        headerSize += 4;

                    if (structDef.IsInstanced)
                        numInstanceBlobs++;
                }

                headerSize += _dynArrayTypes.Count * 4;

                sw.WriteLine("\tvoid " + FormatType!.Name + "_Builder::WriteToStream(::rkit::IWriteStream &outStream, const " + FormatType!.Name + " &obj)");
                sw.WriteLine("\t{");
                sw.WriteLine("\t\t" + FormatType!.Name + "_Builder builder;");
                sw.WriteLine("\t\tbuilder.InternalWriteToStream(outStream, obj);");
                sw.WriteLine("\t}");
                sw.WriteLine();
                sw.WriteLine("\tvoid " + FormatType!.Name + "_Builder::InternalWriteToStream(::rkit::IWriteStream &outStream, const " + FormatType!.Name + " &obj)");
                sw.WriteLine("\t{");
                sw.WriteLine("\t\tCollectInstances(obj);");

                if (numInstanceBlobs > 0)
                {
                    sw.WriteLine();
                    sw.WriteLine("\t\t::rkit::StaticArray<::rkit::BufferStream, " + numInstanceBlobs.ToString() + "> instanceStreams;");
                }

                sw.WriteLine();
                sw.WriteLine("\t\t::rkit::data::ByteBlob<" + headerSize.ToString() + "> headerBlob;");
                sw.WriteLine("\t\t::rkit::data::ByteBlob<" + FormatType!.Name + "::kContentsSize> mainObjectBlob;");
                sw.WriteLine("\t\t::rkit::Span<uint8_t> headerSpan = headerBlob.ModifyStaticArray().ToSpan();");
                sw.WriteLine("\t\t::rkit::Span<uint8_t> mainObjectSpan = mainObjectBlob.ModifyStaticArray().ToSpan();");

                for (int i = 0; i < FormatCode.Length; i++)
                    sw.WriteLine("\t\theaderSpan[" + i + "] = " + ((int)FormatCode[i]).ToString() + ";");

                sw.WriteLine("\t\theaderSpan = headerSpan.SubSpan(" + FormatCode.Length.ToString() + ");");
                sw.WriteLine("\t\tWriteInlineStruct(headerSpan, static_cast<uint32_t>(" + versionCode.ToString() + "u));");

                // Main struct and instances are written first so that other fields are populated
                sw.WriteLine("\t\tWriteInlineStruct(mainObjectSpan, obj);");

                {
                    int instanceBlobIndex = 0;
                    foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                    {
                        StructDef structDef = structDefKVP.Value;

                        if (structDef.IsInstanced)
                        {
                            sw.WriteLine("\t\tWriteInstances(instanceStreams[" + instanceBlobIndex.ToString() + "], m_instancesOf_" + structDef.Name + ");");
                            instanceBlobIndex++;
                        }
                    }
                }


                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsInstanced)
                        sw.WriteLine("\t\tWriteInlineStruct(headerSpan, static_cast<uint32_t>(m_instancesOf_" + structDef.Name + ".Count()));");
                }

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsDeduplicated || structDef.IsDeduplicated)
                        sw.WriteLine("\t\tWriteInlineStruct(headerSpan, static_cast<uint32_t>(m_instancesOf_" + structDef.Name + ".Count()));");
                }

                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                    sw.WriteLine("\t\tWriteInlineStruct(headerSpan, static_cast<uint32_t>(m_dynArraysOf_" + DynArrayNameString(typeDef) + ".Count()));");

                sw.WriteLine();
                sw.WriteLine("\t\toutStream.WriteAllSpan(headerBlob.GetStaticArray().ToSpan());");


                if (numInstanceBlobs > 0)
                {
                    sw.WriteLine("\t\tfor (const ::rkit::BufferStream &instanceStream : instanceStreams)");
                    sw.WriteLine("\t\t\toutStream.WriteAllSpan(instanceStream.GetBuffer().ToSpan());");
                }

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsDeduplicated)
                        sw.WriteLine("\t\t::rkit::data::DataFormatWriter::WriteDeduplicated(outStream, m_instancesOf_" + structDef.Name + ");");
                }

                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                    sw.WriteLine("\t\t::rkit::data::DataFormatWriter::WriteBlobs(outStream, m_dynArraysOf_" + DynArrayNameString(typeDef) + ".ToSpan());");

                sw.WriteLine("\t\toutStream.WriteAllSpan(mainObjectBlob.GetStaticArray().ToSpan());");
                sw.WriteLine("\t}");
                sw.WriteLine("}");
            }

            using (StreamWriter sw = new BuildToolsCommon.WriteIfChangedStreamWriter(this.LoaderPath + ".generated.h"))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#pragma once");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Data/ClusterRefVector.h\"");
                sw.WriteLine("#include \"rkit/Data/ContentID.h\"");
                sw.WriteLine("#include \"rkit/Data/Invertible.h\"");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Core/Vector.h\"");
                sw.WriteLine("#include \"rkit/Core/Span.h\"");
                sw.WriteLine();
                sw.WriteLine("#include <stdint.h>");
                sw.WriteLine();
                sw.Write("namespace ");

                {
                    bool isFirst = true;
                    foreach (string part in Namespace)
                    {
                        if (isFirst)
                            isFirst = false;
                        else
                            sw.Write("::");
                        sw.Write(part);
                    }
                }
                sw.WriteLine();
                sw.WriteLine("{");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    sw.WriteLine("\tstruct " + structDef.Name);
                    sw.WriteLine("\t{");

                    foreach (StructMember member in structDef.Members)
                    {
                        string memberDataName = DataName(member.Type);
                        sw.WriteLine("\t\t" + memberDataName + " m_" + member.Name + ";");
                    }

                    sw.WriteLine("\t};");
                }

                sw.WriteLine();
                sw.WriteLine("\tstruct " + FormatType.Name + "_Instance");
                sw.WriteLine("\t{");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                        sw.WriteLine("\t\t::rkit::Vector<" + structDef.Name + "> m_instancesOf_" + structDef.Name + ";");
                }

                foreach (TypeDef type in _dynArrayTypes.Unroll())
                {
                    if (type.Kind == TypeDefKind.ClusterRef)
                        sw.WriteLine("\t\t::rkit::data::ClusterRefVector<" + type.SubType!.StructDef!.Name + "> m_dynArraysOf_ClusterOf_" + DynArrayNameString(type.SubType!) + ";");
                    else
                        sw.WriteLine("\t\t::rkit::Vector<" + DataDynArrayContentsTypeString(type) + "> m_dynArraysOf_" + DynArrayNameString(type) + ";");
                }

                sw.WriteLine("\t};");
                sw.WriteLine("}");
            }


            using (StreamWriter sw = new BuildToolsCommon.WriteIfChangedStreamWriter(this.LoaderPath + ".loader.generated.h"))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#pragma once");
                sw.WriteLine();
                sw.WriteLine("#include \"" + Path.GetFileName(this.LoaderPath) + ".generated.h\"");
                sw.WriteLine();
                sw.Write("namespace ");

                foreach (string part in Namespace)
                {
                    sw.Write(part);
                    sw.Write("::");
                }
                sw.WriteLine("loader");
                sw.WriteLine("{");
                sw.WriteLine("\tstruct " + FormatType.Name + "_Loader");
                sw.WriteLine("\t{");
                sw.WriteLine("\t};");
                sw.WriteLine("}");
            }
        }

        private string DataDynArrayContentsTypeString(TypeDef type)
        {
            if (IsPrimitiveType(type.Kind))
                return PrimitiveName(type.Kind);

            switch (type.Kind)
            {
                case TypeDefKind.DynArray:
                    return "::rkit::Span<" + DataDynArrayContentsTypeString(type.SubType!) + " const>";
                case TypeDefKind.ClusterArray:
                    return "::rkit::data::ClusterLocator<" + DataDynArrayContentsTypeString(type.SubType!) + ">";

                case TypeDefKind.Struct:
                    {
                        StructDef structDef = type.StructDef!;
                        if (structDef.IsInstanced || structDef.IsDeduplicated)
                            return structDef.Name + " const *";
                        else
                            return structDef.Name;
                    }

                case TypeDefKind.FixedArray:
                    return "::rkit::StaticArray<" + DataDynArrayContentsTypeString(type.SubType!) + ", " + type.Count.ToString() + ">";
                case TypeDefKind.Invertible:
                    return "::rkit::data::InvertibleRef<" + DataDynArrayContentsTypeString(type.SubType!) + ">";
                case TypeDefKind.ContentID:
                    return "::rkit::data::ContentID";
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private string DynArrayNameString(TypeDef typeDef)
        {
            switch (typeDef.Kind)
            {
                case TypeDefKind.Bool:
                    return "Bool";
                case TypeDefKind.UInt8:
                    return "UInt8";
                case TypeDefKind.UInt16:
                    return "UInt16";
                case TypeDefKind.UInt32:
                    return "UInt32";
                case TypeDefKind.DynArray:
                    return "DynArrayOf_" + DynArrayNameString(typeDef.SubType!);
                case TypeDefKind.ClusterRef:
                    return "ClusterOf_" + DynArrayNameString(typeDef.SubType!);
                case TypeDefKind.ClusterArray:
                    return "ClusterArrayOf_" + DynArrayNameString(typeDef.SubType!);
                case TypeDefKind.SInt8:
                    return "Int8";
                case TypeDefKind.SInt16:
                    return "Int16";
                case TypeDefKind.SInt32:
                    return "Int32";
                case TypeDefKind.Float32:
                    return "Float";
                case TypeDefKind.Float64:
                    return "Double";

                case TypeDefKind.Struct:
                    return typeDef.StructDef!.Name;

                case TypeDefKind.FixedArray:
                    return "Fixed" + typeDef.Count.ToString() + "Of_" + DynArrayNameString(typeDef.SubType!);
                case TypeDefKind.Invertible:
                    return "Invertible_" + DynArrayNameString(typeDef.SubType!);
                case TypeDefKind.ContentID:
                    return "ContentID";
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private string MemberSizeString(StructMember member)
        {
            return TypeInlineSizeString(member.Type);
        }

        private void EmitCollectInstancesLinesForStructMember(StreamWriter writer, TypeDef type, string indent, string valueExpr)
        {
            if (!TypeContainsInstances(type, TypeDefRefBehavior.True))
                return;

            switch (type.Kind)
            {
                case TypeDefKind.DynArray:
                case TypeDefKind.ClusterArray:
                case TypeDefKind.FixedArray:
                    writer.WriteLine(indent + "for (const auto &item : " + valueExpr + ")");
                    writer.WriteLine(indent + "\tCollectInstances(item);");
                    return;

                case TypeDefKind.Struct:
                case TypeDefKind.Invertible:
                    writer.WriteLine(indent + "CollectInstances(" + valueExpr + ");");
                    return;

                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private static bool TypeContainsInstances(TypeDef type, TypeDefRefBehavior typeDefRefBehavior)
        {
            switch (type.Kind)
            {
                case TypeDefKind.Bool:
                case TypeDefKind.UInt8:
                case TypeDefKind.UInt16:
                case TypeDefKind.SInt8:
                case TypeDefKind.SInt16:
                case TypeDefKind.SInt32:
                case TypeDefKind.Float32:
                case TypeDefKind.Float64:
                case TypeDefKind.UInt32:
                case TypeDefKind.ContentID:
                    return false;
                case TypeDefKind.DynArray:
                case TypeDefKind.ClusterArray:
                case TypeDefKind.Invertible:
                case TypeDefKind.FixedArray:
                    return TypeContainsInstances(type.SubType!, TypeDefRefBehavior.True);

                case TypeDefKind.Struct:
                    if ((type.StructDef!.IsInstanced || type.StructDef!.IsDeduplicated) && typeDefRefBehavior != TypeDefRefBehavior.ScanMembers)
                        return typeDefRefBehavior == TypeDefRefBehavior.True;
                    else
                    {
                        foreach (StructMember member in type.StructDef!.Members)
                        {
                            if (TypeContainsInstances(member.Type, TypeDefRefBehavior.True))
                                return true;
                        }
                        return false;
                    }
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private void EmitWriteLines(StreamWriter writer, TypeDef type, string indent, string spanExpr, string valueExpr)
        {
            switch (type.Kind)
            {
                case TypeDefKind.Bool:
                case TypeDefKind.UInt8:
                case TypeDefKind.UInt16:
                case TypeDefKind.SInt8:
                case TypeDefKind.SInt16:
                case TypeDefKind.SInt32:
                case TypeDefKind.Float32:
                case TypeDefKind.Float64:
                case TypeDefKind.UInt32:
                case TypeDefKind.ContentID:
                case TypeDefKind.Struct:
                case TypeDefKind.Invertible:
                    writer.WriteLine(indent + "WriteInlineStruct(" + spanExpr + ", " + valueExpr + ");");
                    return;
                case TypeDefKind.DynArray:
                    EmitWriteLines(writer, new TypeDef(TypeDefKind.UInt32), indent, spanExpr, "static_cast<uint32_t>(" + valueExpr + ".Count())");
                    writer.WriteLine(indent + "AddArrayItems(m_dynArraysOf_" + DynArrayNameString(type.SubType!) + ", " + valueExpr + ".ToSpan(), &" + FormatType!.Name + "_Builder::WriteInlineStruct);");
                    return;
                case TypeDefKind.ClusterArray:
                    writer.WriteLine(indent + "{");
                    writer.WriteLine(indent + "\tconst uint32_t clusterCount = AddClusterItems(m_dynArraysOf_" + DynArrayNameString(new TypeDef(TypeDefKind.ClusterRef, type.SubType!)) + ", " + valueExpr + ".ToSpan(), &" + FormatType!.Name + "_Builder::ResolveStructReference);");
                    EmitWriteLines(writer, new TypeDef(TypeDefKind.UInt32), indent + "\t", spanExpr, "clusterCount");
                    writer.WriteLine(indent + "}");
                    return;

                case TypeDefKind.FixedArray:
                    writer.WriteLine(indent + "for (const auto &item : " + valueExpr + ")");
                    EmitWriteLines(writer, type.SubType!, indent + "\t", spanExpr, "item");
                    return;
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private string TypeInlineSizeString(TypeDef type)
        {
            switch (type.Kind)
            {
                case TypeDefKind.UInt8:
                case TypeDefKind.Bool:
                    return "sizeof(uint8_t)";
                case TypeDefKind.UInt16:
                    return "sizeof(uint16_t)";
                case TypeDefKind.UInt32:
                case TypeDefKind.DynArray:
                case TypeDefKind.ClusterArray:
                    return "sizeof(uint32_t)";
                case TypeDefKind.ClusterRef:
                    return "(sizeof(uint32_t) * 2)";
                case TypeDefKind.SInt8:
                    return "sizeof(int8_t)";
                case TypeDefKind.SInt16:
                    return "sizeof(int16_t)";
                case TypeDefKind.SInt32:
                    return "sizeof(int32_t)";
                case TypeDefKind.Float32:
                    return "sizeof(float)";
                case TypeDefKind.Float64:
                    return "sizeof(double)";

                case TypeDefKind.Struct:
                    return type.StructDef!.Name + "::kInlineSize";

                case TypeDefKind.FixedArray:
                    return "(" + type.Count.ToString() + " * " + TypeInlineSizeString(type.SubType!) + ")";
                case TypeDefKind.Invertible:
                    return TypeInlineSizeString(type.SubType!);
                case TypeDefKind.ContentID:
                    return "sizeof(::rkit::data::ContentID)";
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private string DataName(TypeDef typeDef)
        {
            if (IsPrimitiveType(typeDef.Kind))
                return PrimitiveName(typeDef.Kind);
            else
            {
                switch (typeDef.Kind)
                {
                    case TypeDefKind.Bool:
                        return "bool";
                    case TypeDefKind.Struct:
                        {
                            StructDef structDef = typeDef.StructDef!;

                            if (structDef.IsInstanced || structDef.IsDeduplicated)
                                return structDef.Name + " const*";
                            else
                                return structDef.Name;
                        }
                    case TypeDefKind.FixedArray:
                        return "::rkit::StaticArray<" + DataName(typeDef.SubType!) + ", " + typeDef.Count.ToString() + ">";
                    case TypeDefKind.Invertible:
                        return "::rkit::data::Invertible<" + DataName(typeDef.SubType!) + ">";
                    case TypeDefKind.ClusterArray:
                    case TypeDefKind.DynArray:
                        return "::rkit::Span<" + DataName(typeDef.SubType!) + " const>";
                    default:
                        throw new Exception("Internal error");
                }
            }
        }

        private string BuilderName(TypeDef typeDef)
        {
            if (IsPrimitiveType(typeDef.Kind))
                return PrimitiveName(typeDef.Kind);
            else
            {
                switch (typeDef.Kind)
                {
                    case TypeDefKind.Struct:
                        {
                            StructDef structDef = typeDef.StructDef!;

                            if (structDef.IsInstanced)
                                return "::rkit::RCPtr<" + structDef.Name + ">";
                            else
                                return structDef.Name;
                        }
                    case TypeDefKind.FixedArray:
                        return "::rkit::StaticArray<" + BuilderName(typeDef.SubType!) + ", " + typeDef.Count.ToString() + ">";
                    case TypeDefKind.Invertible:
                        return "::rkit::data::builder::InvertibleBuilder<" + BuilderName(typeDef.SubType!) + ">";
                    case TypeDefKind.ClusterArray:
                    case TypeDefKind.DynArray:
                        return "::rkit::Vector<" + BuilderName(typeDef.SubType!) + ">";
                    default:
                        throw new Exception("Internal error");
                }
            }
        }

        private static string PrimitiveName(TypeDefKind kind)
        {
            string? primName = UnsafePrimitiveName(kind);
            if (primName != null)
                return primName;

            throw new Exception("Internal error");
        }

        private static string? UnsafePrimitiveName(TypeDefKind kind)
        {
            switch (kind)
            {
            case TypeDefKind.UInt8:
                return "uint8_t";
            case TypeDefKind.UInt16:
                return "uint16_t";
            case TypeDefKind.UInt32:
                return "uint32_t";
            case TypeDefKind.SInt8:
                return "int8_t";
            case TypeDefKind.SInt16:
                return "int16_t";
            case TypeDefKind.SInt32:
                return "int32_t";
            case TypeDefKind.Float32:
                return "float";
            case TypeDefKind.Float64:
                return "double";
            case TypeDefKind.Bool:
                return "bool";
            case TypeDefKind.ContentID:
                return "::rkit::data::ContentID";

            case TypeDefKind.FixedArray:
            case TypeDefKind.Invertible:
            case TypeDefKind.DynArray:
            case TypeDefKind.Struct:
            case TypeDefKind.ClusterArray:
            case TypeDefKind.ClusterRef:
                    return null;

            default:
                throw new Exception("Internal error");
            }
        }

        private static bool IsPrimitiveType(TypeDefKind kind)
        {
            return UnsafePrimitiveName(kind) != null;
        }

        private static void AddStringToHash(List<byte> hashInput, string str)
        {
            hashInput.AddRange(IntToBytes(str.Length));
            hashInput.AddRange(System.Text.Encoding.UTF8.GetBytes(str));
        }

        private static void AddStructDefToHash(List<byte> hashInput, StructDef structDef, Dictionary<StructDef, int> structDefToIndex)
        {
            AddStringToHash(hashInput, structDef.Name);
            hashInput.AddRange(IntToBytes(structDef.Members.Count));

            foreach (StructMember member in structDef.Members)
                AddStructMemberToHash(hashInput, member, structDefToIndex);

            hashInput.Add(structDef.IsDeduplicated ? (byte)0 : (byte)1);
        }

        private static void AddStructMemberToHash(List<byte> hashInput, StructMember member, Dictionary<StructDef, int> structDefToIndex)
        {
            AddTypeDefToHash(hashInput, member.Type, structDefToIndex);
            AddStringToHash(hashInput, member.Name);
        }

        private static void AddTypeDefToHash(List<byte> hashInput, TypeDef type, Dictionary<StructDef, int> structDefToIndex)
        {
            hashInput.Add((byte)type.Kind);

            switch (type.Kind)
            {
                case TypeDefKind.UInt8:
                case TypeDefKind.UInt16:
                case TypeDefKind.UInt32:
                case TypeDefKind.SInt8:
                case TypeDefKind.SInt16:
                case TypeDefKind.SInt32:
                case TypeDefKind.Float32:
                case TypeDefKind.Float64:
                case TypeDefKind.Bool:
                case TypeDefKind.ContentID:
                    break;

                case TypeDefKind.Invertible:
                case TypeDefKind.DynArray:
                case TypeDefKind.ClusterArray:
                    AddTypeDefToHash(hashInput, type.SubType!, structDefToIndex);
                    break;

                case TypeDefKind.FixedArray:
                    AddTypeDefToHash(hashInput, type.SubType!, structDefToIndex);
                    hashInput.AddRange(IntToBytes(type.Count));
                    break;

                case TypeDefKind.Struct:
                    hashInput.AddRange(IntToBytes(structDefToIndex[type.StructDef!]));
                    break;

                default:
                    throw new Exception("Internal error");
            }
        }

        private static IEnumerable<byte> UIntToBytes(uint v)
        {
            byte[] bytes = new byte[4];
            for (int i = 0; i < 4; i++)
            {
                bytes[i] = (byte)(v & 0xff);
                v >>= 8;
            }

            return bytes;
        }

        private static IEnumerable<byte> IntToBytes(int v)
        {
            byte[] bytes = new byte[4];
            for (int i = 0; i < 4; i++)
            {
                bytes[i] = (byte)(v & 0xff);
                v >>= 8;
            }

            return bytes;
        }
    }

    internal class StructDef
    {
        public string Name { get; private set; }
        public IReadOnlyList<StructMember> Members { get; private set; }
        public bool IsDeduplicated { get; private set; }
        public bool IsInstanced { get; private set; }

        public StructDef(string name, List<StructMember> members, bool isDeduplicated, bool isInstanced)
        {
            this.Name = name;
            this.Members = members;
            this.IsDeduplicated = isDeduplicated;
            this.IsInstanced = isInstanced;
        }
    }

    internal enum TypeDefKind
    {
        UInt8,
        UInt16,
        UInt32,
        SInt8,
        SInt16,
        SInt32,
        Float32,
        Float64,
        Bool,

        FixedArray,
        Invertible,
        ContentID,
        DynArray,
        Struct,
        ClusterArray,
        ClusterRef,
    }

    internal class TypeDef : IEquatable<TypeDef>
    {
        public TypeDefKind Kind { get; private set; }
        public int Count { get; private set; }
        public TypeDef? SubType { get; private set; }
        public StructDef? StructDef { get; private set; }

        public TypeDef(TypeDefKind kind)
        {
            Kind = kind;
        }

        public TypeDef(TypeDefKind kind, TypeDef subType)
        {
            Kind = kind;
            SubType = subType;
        }

        public TypeDef(TypeDefKind kind, int count, TypeDef subType)
        {
            Kind = kind;
            Count = count;
            SubType = subType;
        }

        public TypeDef(TypeDefKind kind, StructDef structDef)
        {
            Kind = kind;
            StructDef = structDef;
        }

        public override int GetHashCode()
        {
            int hashCode = Kind.GetHashCode() + Count.GetHashCode();

            if (SubType != null)
                hashCode += SubType.GetHashCode();

            return hashCode;
        }

        public bool Equals(TypeDef? other)
        {
            if (other == null)
                return false;

            if (Kind != other.Kind)
                return false;

            if (Count != other.Count)
                return false;

            if (StructDef != other.StructDef)
                return false;

            if (SubType == null)
            {
                if (other.SubType != null)
                    return false;
            }
            else
            {
                if (other.SubType == null)
                    return false;

                if (!SubType.Equals(other.SubType))
                    return false;
            }

            return true;
        }
    }

    internal enum TypeDefRefBehavior
    {
        True,
        False,
        ScanMembers
    }

    internal struct StructMember
    {
        public TypeDef Type { get; private set; }
        public string Name { get; private set; }

        public StructMember(TypeDef type, string name)
        {
            Type = type;
            Name = name;
        }
    }

    internal class Program
    {
        static void Main(string[] args)
        {
            if (args.Length != 1)
                throw new Exception("Usage: DataFormatGenerator file.format");

            byte[] formatFileContents = File.ReadAllBytes(args[0]);

            ByteBlobLexer lexer = new ByteBlobLexer(formatFileContents);

            FormatBuilder builder = new FormatBuilder();

            for (; ; )
            {
                Token? tokenNullable = lexer.GetToken();
                if (tokenNullable == null)
                    break;

                Token token = tokenNullable.Value;

                if (lexer.TokenIsString(token, "namespace"))
                    builder.ParseNamespace(lexer);
                else if (lexer.TokenIsString(token, "struct"))
                    builder.ParseStruct(lexer);
                else if (lexer.TokenIsString(token, "loader"))
                    builder.ParseLoader(lexer);
                else if (lexer.TokenIsString(token, "builder"))
                    builder.ParseBuilder(lexer);
                else if (lexer.TokenIsString(token, "formattype"))
                    builder.ParseFormatType(lexer);
                else
                    throw ByteBlobLexer.MakeTokenException(token, "Unexpected def type '" + lexer.TokenToString(token) + "'");
            }

            builder.Export();
        }
    }
}
