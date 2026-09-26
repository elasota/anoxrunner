using BuildToolsCommon;
using System.Data.SqlTypes;
using System.Net.Mime;
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

    internal struct CompoundSize
    {
        public int ByteSize { get; private set; }
        public int ContentIDCount { get; private set; }

        public CompoundSize(int byteSize, int contentIDCount)
        {
            this.ByteSize = byteSize;
            this.ContentIDCount = contentIDCount;
        }

        public CompoundSize(int byteSize)
        {
            this.ByteSize = byteSize;
            this.ContentIDCount = 0;
        }

        public CompoundSize Add(CompoundSize other)
        {
            return new CompoundSize(this.ByteSize + other.ByteSize, this.ContentIDCount + other.ContentIDCount);
        }

        public override string ToString()
        {
            string? bytePart = null;
            string? contentIDPart = null;

            if (ByteSize > 0)
                bytePart = ByteSize.ToString();
            if (ContentIDCount > 0)
                contentIDPart = "(sizeof(::rkit::data::ContentID) * " + ContentIDCount.ToString() + "u)";

            if (bytePart != null)
            {
                if (contentIDPart != null)
                    return "(" + bytePart + " + " + contentIDPart + ")";
                else
                    return bytePart;
            }
            else
            {
                if (contentIDPart != null)
                    return contentIDPart;
                else
                    return "0";
            }
        }

        internal CompoundSize Mul(int count)
        {
            return new CompoundSize(this.ByteSize * count, this.ContentIDCount * count);
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

        public string? BuilderLoaderPath { get; private set; }
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

        internal void ParseBuilderLoader(ByteBlobLexer lexer)
        {
            Token pathToken = lexer.ExpectTokenOfType(TokenType.QuotedString);

            BuilderLoaderPath = lexer.UnquoteString(pathToken);
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

        internal void ExportBuilderHeader()
        {

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
        }

        internal void ExportBuilderInl(int headerSize, int numInstanceBlobs, uint versionCode)
        {
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
                        sw.WriteLine("\t\t::rkit::HashMap<::rkit::data::ByteBlob<" + StructContentsSize(structDef).ToString() + ">, uint32_t> m_instancesOf_" + structDef.Name + ";");
                }

                sw.WriteLine();

                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                {
                    sw.WriteLine("\t\t::rkit::Vector<::rkit::data::ByteBlob<" + TypeInlineCompoundSize(typeDef) + ">> m_dynArraysOf_" + DynArrayNameString(typeDef) + ";");
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
                    "template<class TClass, size_t TUnitSize>",
                    "void WriteInstances(::rkit::IWriteStream &outStream, const ::rkit::HashMap<const TClass *, uint32_t> &hashMap)",
                    "{",
                    "\t::rkit::Vector<::rkit::data::ByteBlob<TUnitSize>> contentBlobs;",
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
                        sw.WriteLine("\t\t\t::rkit::data::ByteBlob<" + StructContentsSize(structDef) + "> contents;");
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
                sw.WriteLine("\t\t::rkit::data::ByteBlob<" + StructContentsSize(FormatType) + "> mainObjectBlob;");
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
                            sw.WriteLine("\t\tWriteInstances<" + structDef.Name + ", " + StructContentsSize(structDef) + ">(instanceStreams[" + instanceBlobIndex.ToString() + "], m_instancesOf_" + structDef.Name + ");");
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

                    if (structDef.IsDeduplicated)
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
        }

        internal void ExportBuilderLoaderInl()
        {
            using (StreamWriter sw = new BuildToolsCommon.WriteIfChangedStreamWriter(this.BuilderPath + ".loader.generated.inl"))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#include \"" + Path.GetFileName(this.BuilderPath) + ".generated.h\"");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Data/DataFormatBuilderHelper.h\"");
                sw.WriteLine();
                sw.WriteLine("#include \"" + BuilderLoaderPath + ".loader.generated.h\"");
                sw.WriteLine();

                string namespaceBase = "";

                foreach (string part in Namespace)
                {
                    if (namespaceBase != "")
                        namespaceBase += "::";
                    namespaceBase += part;
                }

                string builderNS = namespaceBase + "::builder";
                string loaderNS = namespaceBase + "::loader";

                sw.WriteLine("namespace " + builderNS);
                sw.WriteLine("{");
                sw.WriteLine("\tclass " + FormatType.Name + "_BuilderLoader");
                sw.WriteLine("\t{");
                sw.WriteLine("\tpublic:");
                sw.WriteLine("\t\tvoid Convert(::" + builderNS + "::" + FormatType.Name + " &outObject, const ::" + namespaceBase + "::" + FormatType.Name + "_Instance &inInstance);");
                sw.WriteLine("\tprivate:");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    sw.WriteLine("\t\tvoid ConvertStructContents(" + structDef.Name + " &outStruct, const ::" + namespaceBase + "::" + structDef.Name + " &inStruct);");
                }

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    TypeDef typeDef = new TypeDef(TypeDefKind.Struct, structDef);

                    sw.WriteLine("\t\tvoid ConvertInlineStruct(" + BuilderName(typeDef) + " &outValue, ::" + namespaceBase + "::" + DataName(typeDef) + " const& inValue);");
                }

                sw.WriteLine();
                sw.WriteLine("\t\tconst ::" + namespaceBase + "::" + FormatType.Name + "_Instance *m_instance = nullptr;");
                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsInstanced)
                        sw.WriteLine("\t\t::rkit::Vector<::rkit::RCPtr<::" + builderNS + "::" + structDef.Name + ">> m_instancesOf_" + structDef.Name + ";");
                }

                sw.WriteLine("\t};");
                sw.WriteLine("}");
                sw.WriteLine();
                sw.WriteLine("namespace " + builderNS);
                sw.WriteLine("{");
                sw.WriteLine("\tinline void " + FormatType.Name + "_BuilderLoader::Convert(::" + builderNS + "::" + FormatType.Name + " &outObject, const ::" + namespaceBase + "::" + FormatType.Name + "_Instance &inInstance)");
                sw.WriteLine("\t{");
                sw.WriteLine("\t\tm_instance = &inInstance;");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsInstanced)
                        sw.WriteLine("\t\t::rkit::data::DataFormatBuilderHelper::InitRCPtrVector(m_instancesOf_" + structDef.Name + ", inInstance.m_instancesOf_" + structDef.Name + ".Count());");
                }
                sw.WriteLine("\t\tConvertStructContents(outObject, inInstance.m_rootObject);");
                sw.WriteLine("\t}");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    TypeDef typeDef = new TypeDef(TypeDefKind.Struct, structDef);

                    sw.WriteLine();
                    sw.WriteLine("\tinline void " + FormatType.Name + "_BuilderLoader::ConvertInlineStruct(" + BuilderName(typeDef) + " &outValue, ::" + namespaceBase + "::" + DataName(typeDef) + " const& inValue)");
                    sw.WriteLine("\t{");

                    if (structDef.IsInstanced)
                        sw.WriteLine("\t\toutValue = m_instancesOf_" + structDef.Name + "[inValue - m_instance->m_instancesOf_" + structDef.Name + ".GetBuffer()];");
                    else if (structDef.IsDeduplicated)
                        sw.WriteLine("\t\tConvertStructContents(outValue, *inValue);");
                    else
                        sw.WriteLine("\t\tConvertStructContents(outValue, inValue);");

                    sw.WriteLine("\t}");
                    sw.WriteLine();
                    sw.WriteLine("\tinline void " + FormatType.Name + "_BuilderLoader:: ConvertStructContents(" + structDef.Name + " &outStruct, const ::" + namespaceBase + "::" + structDef.Name + " &inStruct)");
                    sw.WriteLine("\t{");

                    foreach (StructMember member in structDef.Members)
                        EmitConvertLines(sw, member.Type, "\t\t", "outStruct.m_" + member.Name, "inStruct.m_" + member.Name, namespaceBase);

                    sw.WriteLine("\t}");
                }

                sw.WriteLine("}");
            }
        }

        internal void ExportInstanceHeader()
        {
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
                sw.WriteLine("namespace rkit");
                sw.WriteLine("{");
                sw.WriteLine("\tstruct IReadStream;");
                sw.WriteLine("}");
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
                sw.WriteLine("\t\t" + FormatType.Name + " m_rootObject;");

                sw.WriteLine("\t};");
                sw.WriteLine("}");
            }
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

            string fullNamespace = "";
            foreach (string part in Namespace)
                fullNamespace = fullNamespace + "::" + part;

            List<byte> hashInput = new List<byte>();

            hashInput.AddRange(IntToBytes(Namespace!.Count));
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

            ExportBuilderHeader();
            ExportBuilderInl(headerSize, numInstanceBlobs, versionCode);

            if (BuilderLoaderPath != null)
            {
                ExportBuilderLoaderInl();
            }

            ExportInstanceHeader();

            using (StreamWriter sw = new BuildToolsCommon.WriteIfChangedStreamWriter(this.LoaderPath + ".loader.generated.h"))
            {
                sw.NewLine = "\n";

                sw.WriteLine("#pragma once");
                sw.WriteLine();
                sw.WriteLine("#include \"" + Path.GetFileName(this.LoaderPath) + ".generated.h\"");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Data/DataFormatReader.h\"");
                sw.WriteLine();
                sw.WriteLine("#include \"rkit/Core/Stream.h\"");
                sw.WriteLine();

                {
                    bool isFirst = true;
                    sw.Write("namespace ");
                    foreach (string part in Namespace)
                    {
                        if (isFirst)
                            isFirst = false;
                        else
                            sw.Write("::");
                        sw.Write(part);
                    }
                    sw.WriteLine();
                    sw.WriteLine("{");
                    sw.WriteLine("\tstruct " + FormatType.Name + "_Instance;");
                    sw.WriteLine("}");
                }

                string instanceName = FormatType.Name + "_Instance";
                string loaderName = FormatType.Name + "_Loader";

                sw.Write("namespace ");

                foreach (string part in Namespace)
                {
                    sw.Write(part);
                    sw.Write("::");
                }
                sw.WriteLine("loader");
                sw.WriteLine("{");
                sw.WriteLine("\tclass " + loaderName);
                sw.WriteLine("\t{");
                sw.WriteLine("\tpublic:");
                sw.WriteLine("\t\tRKIT_NODISCARD bool Load(" + instanceName + " &instance, ::rkit::IReadStream &stream);");
                sw.WriteLine("\tprivate:");
                sw.WriteLine("\t\tinline static bool FirstChanceDataFailure()");
                sw.WriteLine("\t\t{");
                sw.WriteLine("\t\t\treturn false;");
                sw.WriteLine("\t\t}");
                sw.WriteLine("\t\tconst " + instanceName + " *m_instance = nullptr;");

                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                {
                    string contentsType;
                    if (typeDef.Kind == TypeDefKind.ClusterRef)
                        contentsType = "::rkit::data::ClusterRef";
                    else
                        contentsType = DataDynArrayContentsTypeString(typeDef);

                    sw.WriteLine("\t\t::rkit::Span<" + contentsType + "> m_currentOutSliceOf_" + DynArrayNameString(typeDef) + ";");
                    sw.WriteLine("\t\t::rkit::Span<uint8_t const> m_currentInSliceOf_" + DynArrayNameString(typeDef) + ";");
                }

                string[] baseTypes = { "uint8_t", "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t", "float", "double", "::rkit::data::ContentID", "bool", "::rkit::data::ClusterRef" };

                foreach (string baseType in baseTypes)
                {
                    sw.WriteLine("\t\tbool ReadInstances(::rkit::Span<" + baseType + "> objs, ::rkit::Span<const uint8_t> &inSpan) noexcept;");
                    sw.WriteLine("\t\tbool ReadInlineStruct(" + baseType + "& obj, ::rkit::Span<const uint8_t> &inSpan) noexcept;");
                }
                sw.WriteLine();

                sw.WriteLine("\t\ttemplate<class T>");
                sw.WriteLine("\t\tbool ReadArrayItems(::rkit::Span<T const> &inlineItems, ::rkit::Span<T> &blobItems, ::rkit::Span<const uint8_t> &itemsSpan, ::rkit::Span<const uint8_t> &counterSpan, size_t unitSize) noexcept;");
                sw.WriteLine("\t\ttemplate<class T>");
                sw.WriteLine("\t\tbool ReadClusterArrayItems(::rkit::data::ClusterSpan<T> &inlineItems, ::rkit::Span<::rkit::data::ClusterRef> &clusterRefs, ::rkit::Span<const uint8_t> &itemsSpan, ::rkit::Span<const uint8_t> &counterSpan, ::rkit::Span<const T> instances) noexcept;");

                sw.WriteLine();
                sw.WriteLine("\t\tinline static bool ReadByteSpan(::rkit::Span<uint8_t> bytes, ::rkit::IReadStream &stream)");
                sw.WriteLine("\t\t{");
                sw.WriteLine("\t\t\tsize_t countRead = 0;");
                sw.WriteLine("\t\t\tstream.ReadPartial(bytes.Ptr(), bytes.Count(), countRead);");
                sw.WriteLine("\t\t\treturn countRead == bytes.Count();");
                sw.WriteLine("\t\t}");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    sw.WriteLine("\t\tbool ReadInstances(::rkit::Span<" + structDef.Name + "> objs, ::rkit::Span<const uint8_t> &inSpan) noexcept;");

                    string structKey = structDef.Name;
                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                    {
                        structKey += " const*";
                        sw.WriteLine("\t\tbool ResolveStructInstance(" + structKey + "& obj, uint32_t index);");
                    }

                    sw.WriteLine("\t\tbool ReadInlineStruct(" + structKey + "& obj, ::rkit::Span<const uint8_t> &inSpan);");

                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                        sw.WriteLine("\t\tbool ReadInlineStruct(::rkit::data::Invertible<" + structKey + ">& obj, ::rkit::Span<const uint8_t> &inSpan);");
                }
                sw.WriteLine("\t\ttemplate<class T>");
                sw.WriteLine("\t\tbool ReadInstancesFromStream(::rkit::Span<T> objs, ::rkit::IReadStream &stream, size_t unitSize);");

                sw.WriteLine();
                sw.WriteLine("\t\ttemplate<class T>");
                sw.WriteLine("\t\tinline static bool PreloadInstances(::rkit::Span<const uint8_t> &headerSpan, ::rkit::Vector<T> &itemVector, ::rkit::Vector<uint8_t> &byteVector, size_t unitSize, ::rkit::IReadStream &stream)");
                sw.WriteLine("\t\t{");
                sw.WriteLine("\t\t\tuint32_t count = 0;");
                sw.WriteLine("\t\t\t::rkit::data::DataFormatReader::ReadSpan(::rkit::Span<uint32_t>(&count, 1), headerSpan);");
                sw.WriteLine("\t\t\tif (std::numeric_limits<size_t>::max() / unitSize < count) return FirstChanceDataFailure();");
                sw.WriteLine("\t\t\titemVector.Resize(count);");
                sw.WriteLine("\t\t\tbyteVector.Resize(count * unitSize);");
                sw.WriteLine("\t\t\treturn ReadByteSpan(byteVector.ToSpan(), stream);");
                sw.WriteLine("\t\t}");

                sw.WriteLine();
                sw.WriteLine("\t\ttemplate<class T>");
                sw.WriteLine("\t\tinline static bool PreloadInstances(::rkit::Span<const uint8_t> &headerSpan, ::rkit::data::ClusterRefVector<T> &vector, ::rkit::Vector<uint8_t> &byteVector, size_t unitSize, ::rkit::IReadStream &stream)");
                sw.WriteLine("\t\t{");
                sw.WriteLine("\t\t\treturn PreloadInstances(headerSpan, vector.ModifyClusterVector(), byteVector, unitSize, stream);");
                sw.WriteLine("\t\t}");

                sw.WriteLine();
                sw.WriteLine("\t\ttemplate<class T>");
                sw.WriteLine("\t\tinline bool ParseInstances(::rkit::Span<T> instanceSpan, ::rkit::Span<const uint8_t> &inSpan, size_t unitSize)");
                sw.WriteLine("\t\t{");
                sw.WriteLine("\t\t\tconst size_t instanceSize = instanceSpan.Count() * unitSize;");
                sw.WriteLine("\t\t\t::rkit::Span<const uint8_t> sliceSpan = inSpan.SubSpan(0, instanceSize);");
                sw.WriteLine("\t\t\tinSpan = sliceSpan.SubSpan(instanceSize);");
                sw.WriteLine("\t\t\treturn ReadInstances(instanceSpan, sliceSpan);");

                sw.WriteLine("\t\t}");

                sw.WriteLine("\t};");
                sw.WriteLine("}");
                sw.WriteLine();

                sw.Write("namespace ");
                foreach (string part in Namespace)
                {
                    sw.Write(part);
                    sw.Write("::");
                }
                sw.WriteLine("loader");
                sw.WriteLine("{");
                sw.WriteLine("\tinline bool " + FormatType.Name + "_Loader::Load(" + FormatType.Name + "_Instance &instance, ::rkit::IReadStream &stream)");
                sw.WriteLine("\t{");
                sw.WriteLine("\t\tm_instance = &instance;");
                sw.WriteLine("\t\t::rkit::StaticArray<uint8_t, " + headerSize + "> headerBlob;");
                sw.WriteLine("\t\tif (!ReadByteSpan(headerBlob.ToSpan(), stream)) return FirstChanceDataFailure();");
                sw.WriteLine("\t\t::rkit::Span<uint8_t const> headerSpan = headerBlob.ToSpan();");

                for (int i = 0; i < FormatCode.Length; i++)
                    sw.WriteLine("\t\tif (headerSpan[" + i + "] != " + ((int)FormatCode[i]).ToString() + ") return FirstChanceDataFailure();");

                sw.WriteLine("\t\theaderSpan = headerSpan.SubSpan(" + FormatCode.Length.ToString() + ");");
                sw.WriteLine("\t\t{");
                sw.WriteLine("\t\t\tuint32_t versionCode = 0;");
                sw.WriteLine("\t\t\t::rkit::data::DataFormatReader::ReadSpan(::rkit::Span<uint32_t>(&versionCode, 1), headerSpan);");
                sw.WriteLine("\t\t\tif (versionCode != static_cast<uint32_t>(" + versionCode.ToString() + "u)) return FirstChanceDataFailure();");
                sw.WriteLine("\t\t}");

                // Alloc instances
                {
                    List<StructDef> instanced = new List<StructDef>();
                    List<StructDef> deduped = new List<StructDef>();

                    foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                    {
                        StructDef structDef = structDefKVP.Value;
                        if (structDef.IsInstanced)
                            instanced.Add(structDef);
                        else if (structDef.IsDeduplicated)
                            deduped.Add(structDef);
                    }

                    List<StructDef> combined = new List<StructDef>(instanced);
                    combined.AddRange(deduped);

                    foreach (StructDef structDef in combined)
                    {
                        sw.WriteLine("\t\t::rkit::Vector<uint8_t> byteBlobOfInstancesOf_" + structDef.Name + ";");
                        sw.WriteLine("\t\tif (!PreloadInstances(headerSpan, instance.m_instancesOf_" + structDef.Name + ", byteBlobOfInstancesOf_" + structDef.Name + ", " + StructContentsSize(structDef) + ", stream)) return FirstChanceDataFailure();");

                        if (structDef.IsInstanced || structDef.IsDeduplicated)
                            sw.WriteLine("\t\t::rkit::Span<const uint8_t> byteBlobSpanOfInstancesOf_" + structDef.Name + " = byteBlobOfInstancesOf_" + structDef.Name + ".ToSpan();");
                    }
                }

                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                {
                    string arrName = DynArrayNameString(typeDef);

                    sw.WriteLine("\t\t::rkit::Vector<uint8_t> byteBlobOfArraysOf_" + arrName + ";");
                    sw.WriteLine("\t\tif (!PreloadInstances(headerSpan, instance.m_dynArraysOf_" + arrName + ", byteBlobOfArraysOf_" + arrName + ", " + TypeInlineCompoundSize(typeDef) + ", stream)) return FirstChanceDataFailure();");
                }


                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                {
                    string spanConversionSuffix = ".ToSpan()";
                    if (typeDef.Kind == TypeDefKind.ClusterRef)
                    {
                        sw.WriteLine("\t\tinstance.m_dynArraysOf_" + DynArrayNameString(typeDef) + ".SetInstances(instance.m_instancesOf_" + DynArrayNameString(typeDef.SubType!) + ".ToSpan());");
                        spanConversionSuffix = ".ModifyClusterVector()" + spanConversionSuffix;
                    }

                    sw.WriteLine("\t\tm_currentOutSliceOf_" + DynArrayNameString(typeDef) + " = instance.m_dynArraysOf_" + DynArrayNameString(typeDef) + spanConversionSuffix + ";");
                    sw.WriteLine("\t\tm_currentInSliceOf_" + DynArrayNameString(typeDef) + " = byteBlobOfArraysOf_" + DynArrayNameString(typeDef) + ".ToSpan();");
                }

                sw.WriteLine();

                // Read instances
                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                        sw.WriteLine("\t\tif (!ParseInstances(instance.m_instancesOf_" + structDef.Name + ".ToSpan(), byteBlobSpanOfInstancesOf_" + structDef.Name + ", " + StructContentsSize(structDef) + ")) return false;");
                }

                sw.WriteLine("\t\t::rkit::StaticArray<uint8_t, " + StructContentsSize(FormatType) + "> rootObjectBytes;");
                sw.WriteLine("\t\t::rkit::Span<const uint8_t> rootObjectSpan = rootObjectBytes.ToSpan();");
                sw.WriteLine("\t\tif (!ReadByteSpan(rootObjectBytes.ToSpan(), stream)) return FirstChanceDataFailure();");
                sw.WriteLine("\t\tif (!ParseInstances(::rkit::Span<" + FormatType.Name + ">(&instance.m_rootObject, 1), rootObjectSpan, rootObjectSpan.Count())) return FirstChanceDataFailure();");

                // Validate that all arrays were consumed
                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                    sw.WriteLine("\t\tif (m_currentOutSliceOf_" + DynArrayNameString(typeDef) + ".Count() != 0) return FirstChanceDataFailure();");

                sw.WriteLine();
                sw.WriteLine("\t\treturn true;");
                sw.WriteLine("\t}");

                foreach (KeyValuePair<string, StructDef> structDefKVP in _structs.Unroll())
                {
                    StructDef structDef = structDefKVP.Value;

                    sw.WriteLine();
                    sw.WriteLine("\tinline bool " + FormatType!.Name + "_Loader::ReadInstances(::rkit::Span<" + structDef.Name + "> objs, ::rkit::Span<const uint8_t> &inSpan) noexcept");
                    sw.WriteLine("\t{");
                    sw.WriteLine("\t\t::rkit::Span<const uint8_t> slice = inSpan.SubSpan(0, " + StructContentsSize(structDef) + " * objs.Count());");
                    sw.WriteLine("\t\tinSpan = inSpan.SubSpan(slice.Count());");
                    sw.WriteLine("\t\tfor (auto &baseItem : objs)");
                    sw.WriteLine("\t\t{");
                    foreach (StructMember member in structDef.Members)
                        EmitReadLines(sw, member.Type, "\t\t\t", "slice", "baseItem.m_" + member.Name);
                    sw.WriteLine("\t\t}");

                    sw.WriteLine("\t\treturn true;");
                    sw.WriteLine("\t}");

                    string structKey = structDef.Name;
                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                    {
                        structKey += " const*";

                        sw.WriteLine();
                        sw.WriteLine("\tbool " + FormatType!.Name + "_Loader::ResolveStructInstance(" + structKey + "& obj, uint32_t index)");
                        sw.WriteLine("\t{");
                        sw.WriteLine("\t\t::rkit::Span<const " + structDef.Name + "> instances = m_instance->m_instancesOf_" + structDef.Name + ".ToSpan();");
                        sw.WriteLine("\t\tif (index >= instances.Count()) return FirstChanceDataFailure();");
                        sw.WriteLine("\t\tobj = &instances[index];");
                        sw.WriteLine("\t\treturn true;");
                        sw.WriteLine("\t}");

                    }

                    if (structDef.IsInstanced || structDef.IsDeduplicated)
                    {
                        sw.WriteLine();
                        sw.WriteLine("\tbool " + FormatType!.Name + "_Loader::ReadInlineStruct(" + structKey + "& obj, ::rkit::Span<const uint8_t> &inSpan)");
                        sw.WriteLine("\t{");
                        sw.WriteLine("\t\tuint32_t index = 0;");
                        sw.WriteLine("\t\tif (!ReadInlineStruct(index, inSpan)) return FirstChanceDataFailure();");
                        sw.WriteLine("\t\treturn ResolveStructInstance(obj, index);");
                        sw.WriteLine("\t}");
                        sw.WriteLine();
                        sw.WriteLine("\tbool " + FormatType!.Name + "_Loader::ReadInlineStruct(::rkit::data::Invertible<" + structKey + ">& obj, ::rkit::Span<const uint8_t> &inSpan)");
                        sw.WriteLine("\t{");
                        sw.WriteLine("\t\tuint32_t index = 0;");
                        sw.WriteLine("\t\tif (!ReadInlineStruct(index, inSpan)) return FirstChanceDataFailure();");
                        sw.WriteLine("\t\t" + structKey + " ptr = nullptr;");
                        sw.WriteLine("\t\tif (!ResolveStructInstance(ptr, index >> 1)) return FirstChanceDataFailure();");
                        sw.WriteLine("\t\tobj.Set(ptr, (index & 1) != 0);");
                        sw.WriteLine("\t\treturn true;");
                        sw.WriteLine("\t}");
                    }
                    else
                    {
                        sw.WriteLine();
                        sw.WriteLine("\tbool " + FormatType!.Name + "_Loader::ReadInlineStruct(" + structKey + "& obj, ::rkit::Span<const uint8_t> &inSpan)");
                        sw.WriteLine("\t{");
                        sw.WriteLine("\t\treturn ReadInstances(::rkit::Span<" + structKey + ">(&obj, 1), inSpan);");
                        sw.WriteLine("\t}");
                    }
                }

                foreach (string baseType in baseTypes)
                {
                    sw.WriteLine();
                    sw.WriteLine("\tinline bool " + FormatType!.Name + "_Loader::ReadInstances(::rkit::Span<" + baseType + "> objs, ::rkit::Span<const uint8_t> &inSpan) noexcept");
                    sw.WriteLine("\t{");
                    sw.WriteLine("\t\t::rkit::data::DataFormatReader::ReadSpan(objs, inSpan);");
                    sw.WriteLine("\t\treturn true;");
                    sw.WriteLine("\t}");
                    sw.WriteLine();
                    sw.WriteLine("\tinline bool " + FormatType!.Name + "_Loader::ReadInlineStruct(" + baseType + "& obj, ::rkit::Span<const uint8_t> &inSpan) noexcept");
                    sw.WriteLine("\t{");
                    sw.WriteLine("\t\t::rkit::data::DataFormatReader::ReadSpan(::rkit::Span<" + baseType + ">(&obj, 1), inSpan);");
                    sw.WriteLine("\t\treturn true;");
                    sw.WriteLine("\t}");
                }
                sw.WriteLine();

                foreach (TypeDef typeDef in _dynArrayTypes.Unroll())
                {
                    if (IsPrimitiveType(typeDef.Kind) || typeDef.Kind == TypeDefKind.Struct)
                        continue;
                }

                sw.WriteLine();
                sw.WriteLine("\ttemplate<class T>");
                sw.WriteLine("\tinline bool " + FormatType!.Name + "_Loader::ReadArrayItems(::rkit::Span<T const> &inlineItems, ::rkit::Span<T> &blobItems, ::rkit::Span<const uint8_t> &itemsSpan, ::rkit::Span<const uint8_t> &counterSpan, size_t unitSize) noexcept");
                sw.WriteLine("\t{");
                sw.WriteLine("\t\tuint32_t count = 0;");
                sw.WriteLine("\t\tif (!ReadInlineStruct(count, counterSpan)) return FirstChanceDataFailure();");
                sw.WriteLine("\t\tif (count > blobItems.Count()) return FirstChanceDataFailure();");
                sw.WriteLine("\t\t::rkit::Span<T> itemsSlice = blobItems.SubSpan(0, count);");
                sw.WriteLine("\t\tblobItems = blobItems.SubSpan(count);");
                sw.WriteLine("\t\t::rkit::Span<const uint8_t> bytesSlice = itemsSpan.SubSpan(0, count * unitSize);");
                sw.WriteLine("\t\titemsSpan = itemsSpan.SubSpan(count * unitSize);");
                sw.WriteLine("\t\tinlineItems = itemsSlice;");
                sw.WriteLine("\t\tfor (T &item : itemsSlice)");
                sw.WriteLine("\t\t{");
                sw.WriteLine("\t\t\tif (!ReadInlineStruct(item, bytesSlice))");
                sw.WriteLine("\t\t\t\treturn FirstChanceDataFailure();");
                sw.WriteLine("\t\t}");
                sw.WriteLine("\t\treturn true;");
                sw.WriteLine("\t}");

                sw.WriteLine();
                sw.WriteLine("\ttemplate<class T>");
                sw.WriteLine("\tinline bool " + FormatType!.Name + "_Loader::ReadClusterArrayItems(::rkit::data::ClusterSpan<T> &inlineItems, ::rkit::Span<::rkit::data::ClusterRef> &clusterRefs, ::rkit::Span<const uint8_t> &itemsSpan, ::rkit::Span<const uint8_t> &counterSpan, ::rkit::Span<const T> instances) noexcept");
                sw.WriteLine("\t{");
                sw.WriteLine("\t\t::rkit::Span<const ::rkit::data::ClusterRef> inlineRefs;");
                sw.WriteLine("\t\tif (!ReadArrayItems(inlineRefs, clusterRefs, itemsSpan, counterSpan, 8)) return FirstChanceDataFailure();");
                sw.WriteLine("\t\tif (!::rkit::data::DataFormatReader::ValidateClusterRefs(inlineRefs, instances.Count())) return FirstChanceDataFailure();");
                sw.WriteLine("\t\tinlineItems = ::rkit::data::ClusterSpan<T>(instances.Ptr(), inlineRefs);");
                sw.WriteLine("\t\treturn true;");
                sw.WriteLine("\t}");
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
                case TypeDefKind.ClusterRef:
                    return "::rkit::data::ClusterRef";

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
                    {
                        string itemName = "item" + indent.Length.ToString();
                        writer.WriteLine(indent + "for (const auto &" + itemName + " : " + valueExpr + ")");
                        writer.WriteLine(indent + "{");
                        EmitWriteLines(writer, type.SubType!, indent + "\t", spanExpr, itemName);
                        writer.WriteLine(indent + "}");
                    }
                    return;
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private void EmitReadLines(StreamWriter writer, TypeDef type, string indent, string spanExpr, string valueExpr)
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
                    writer.WriteLine(indent + "if (!ReadInlineStruct(" + valueExpr + ", " + spanExpr + ")) return FirstChanceDataFailure();");
                    return;
                case TypeDefKind.DynArray:
                    writer.WriteLine(indent + "if (!ReadArrayItems(" + valueExpr + ", m_currentOutSliceOf_" + DynArrayNameString(type.SubType!) + ", m_currentInSliceOf_" + DynArrayNameString(type.SubType!) + ", " + spanExpr + ", " + TypeInlineCompoundSize(type.SubType!) + ")) return FirstChanceDataFailure();");
                    return;
                case TypeDefKind.ClusterArray:
                    writer.WriteLine(indent + "if (!ReadClusterArrayItems(" + valueExpr + ", m_currentOutSliceOf_ClusterOf_" + DynArrayNameString(type.SubType!) + ", m_currentInSliceOf_ClusterOf_" + DynArrayNameString(type.SubType!) + ", " + spanExpr + ", m_instance->m_instancesOf_" + type.SubType!.StructDef!.Name + ".ToSpan())) return FirstChanceDataFailure();");
                    return;

                case TypeDefKind.FixedArray:
                    {
                        string itemName = "item" + indent.Length.ToString();
                        writer.WriteLine(indent + "for (auto &" + itemName + " : " + valueExpr + ")");
                        writer.WriteLine(indent + "{");
                        EmitReadLines(writer, type.SubType!, indent + "\t", spanExpr, itemName);
                        writer.WriteLine(indent + "}");
                    }
                    return;
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private void EmitSubtypeLambdaConvertLines(StreamWriter writer, TypeDef type, string indent, string outExpr, string inExpr, string namespaceBase, string funcName)
        {
            string inName = "inValue" + indent.Length;
            string outName = "outValue" + indent.Length;
            writer.WriteLine(indent + "::rkit::data::DataFormatBuilderHelper::" + funcName + "(" + outExpr + ", " + inExpr + ",");
            writer.WriteLine(indent + "\t[&](" + BuilderName(type.SubType!) + " &" + outName + ", " + DataName(type.SubType!, "::" + namespaceBase + "::") + " const& " + inName + ")");
            writer.WriteLine(indent + "\t{");
            EmitConvertLines(writer, type.SubType, indent + "\t\t", outName, inName, namespaceBase);
            writer.WriteLine(indent + "\t});");
        }

        private void EmitConvertLines(StreamWriter writer, TypeDef type, string indent, string outExpr, string inExpr, string namespaceBase)
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
                    writer.WriteLine(indent + outExpr + " = " + inExpr + ";");
                    return;
                case TypeDefKind.Invertible:
                    EmitSubtypeLambdaConvertLines(writer, type, indent, outExpr, inExpr, namespaceBase, "ConvertInvertible");
                    return;
                case TypeDefKind.Struct:
                    writer.WriteLine(indent + "ConvertInlineStruct(" + outExpr + ", " + inExpr + ");");
                    return;
                case TypeDefKind.DynArray:
                    EmitSubtypeLambdaConvertLines(writer, type, indent, outExpr, inExpr, namespaceBase, "ConvertVector");
                    return;
                case TypeDefKind.ClusterArray:
                    EmitSubtypeLambdaConvertLines(writer, type, indent, outExpr, inExpr, namespaceBase, "ConvertClusterArray");
                    return;

                case TypeDefKind.FixedArray:
                    EmitSubtypeLambdaConvertLines(writer, type, indent, outExpr, inExpr, namespaceBase, "ConvertFixedArray");
                    return;
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private static CompoundSize StructContentsSize(StructDef structDef)
        {
            CompoundSize totalSize = new CompoundSize(0);

            foreach (StructMember member in structDef.Members)
                totalSize = totalSize.Add(TypeInlineCompoundSize(member.Type));

            return totalSize;
        }

        private static CompoundSize StructInlineSize(StructDef structDef)
        {
            if (structDef.IsInstanced || structDef.IsDeduplicated)
                return new CompoundSize(4);

            return StructContentsSize(structDef);
        }

        private static CompoundSize TypeInlineCompoundSize(TypeDef type)
        {
            switch (type.Kind)
            {
                case TypeDefKind.UInt8:
                case TypeDefKind.Bool:
                case TypeDefKind.SInt8:
                    return new CompoundSize(1);
                case TypeDefKind.UInt16:
                case TypeDefKind.SInt16:
                    return new CompoundSize(2);
                case TypeDefKind.UInt32:
                case TypeDefKind.SInt32:
                case TypeDefKind.Float32:
                case TypeDefKind.DynArray:
                case TypeDefKind.ClusterArray:
                    return new CompoundSize(4);
                case TypeDefKind.ClusterRef:
                case TypeDefKind.Float64:
                    return new CompoundSize(8);

                case TypeDefKind.Struct:
                    return StructInlineSize(type.StructDef!);

                case TypeDefKind.FixedArray:
                    return TypeInlineCompoundSize(type.SubType!).Mul(type.Count);
                case TypeDefKind.Invertible:
                    return TypeInlineCompoundSize(type.SubType!);
                case TypeDefKind.ContentID:
                    return new CompoundSize(0, 1);
                default:
                    throw new ArgumentException("Unknown type kind");
            }
        }

        private string DataName(TypeDef typeDef, string namespacePrefix)
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
                                return namespacePrefix + structDef.Name + " const*";
                            else
                                return namespacePrefix + structDef.Name;
                        }
                    case TypeDefKind.FixedArray:
                        return "::rkit::StaticArray<" + DataName(typeDef.SubType!, namespacePrefix) + ", " + typeDef.Count.ToString() + ">";
                    case TypeDefKind.Invertible:
                        return "::rkit::data::Invertible<" + DataName(typeDef.SubType!, namespacePrefix) + ">";
                    case TypeDefKind.ClusterArray:
                        return "::rkit::data::ClusterSpan<" + namespacePrefix + typeDef.SubType!.StructDef!.Name + ">";
                    case TypeDefKind.DynArray:
                        return "::rkit::Span<" + DataName(typeDef.SubType!, namespacePrefix) + " const>";
                    default:
                        throw new Exception("Internal error");
                }
            }
        }

        private string DataName(TypeDef typeDef)
        {
            return DataName(typeDef, "");
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
                else if (lexer.TokenIsString(token, "builderloader"))
                    builder.ParseBuilderLoader(lexer);
                else if (lexer.TokenIsString(token, "formattype"))
                    builder.ParseFormatType(lexer);
                else
                    throw ByteBlobLexer.MakeTokenException(token, "Unexpected def type '" + lexer.TokenToString(token) + "'");
            }

            builder.Export();
        }
    }
}
