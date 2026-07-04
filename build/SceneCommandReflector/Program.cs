using System.Runtime.InteropServices;

namespace SceneCommandReflector
{
    enum ParamType
    {
        UInt,
        Float,
        Str,
        EntityID,
        EntityType,
        HexUInt,
        Label,
    }

    struct ParamDef
    {
        public ParamType Type { get; }
        public string Name { get; }

        public ParamDef(ParamType type, string name)
        {
            Type = type;
            Name = name;
        }
    }

    struct SceneCommandDef
    {
        public string Name { get; }
        public IReadOnlyList<ParamDef> Params { get; }
        public char Delimiter { get; }
        public uint NumOptionalParameters { get; }
        public bool MayFail { get; }

        public SceneCommandDef(string name, IReadOnlyList<ParamDef> parameters, uint numOptionalParameters, char delimiter, bool mayFail)
        {
            Name = name;
            Params = parameters;
            NumOptionalParameters = numOptionalParameters;
            Delimiter = delimiter;
            MayFail = mayFail;
        }
    }

    internal class Program
    {
        static void CompileSceneDefs(string inFile, string outDir)
        {
            string[] contents = File.ReadAllLines(inFile);

            List<SceneCommandDef> commandDefs = new List<SceneCommandDef>();

            foreach (string line in contents)
            {
                string[] unfilteredTokens = line.Split(' ');
                List<string> filteredTokens = new List<string>();

                foreach (string token in unfilteredTokens)
                {
                    if (token.Length == 0)
                        continue;

                    filteredTokens.Add(token);
                }

                if (filteredTokens.Count == 0)
                    continue;

                commandDefs.Add(ParseCommandDef(filteredTokens));
            }

            ExportCommandDefs(outDir, commandDefs);
        }
        private static void ExportCommandDefs(string outDir, IReadOnlyList<SceneCommandDef> commandDefs)
        {
            using (StreamWriter stream = new StreamWriter(Path.Combine(outDir, "include", "anox", "Data", "SceneCommandOpcodes.generated.h")))
            {
                stream.NewLine = "\n";

                stream.WriteLine("#pragma once");
                stream.WriteLine();
                stream.WriteLine("#include <stdint.h>");
                stream.WriteLine();
                stream.WriteLine("namespace anox::data");
                stream.WriteLine("{");
                stream.WriteLine("\tenum class SceneCommandOpcode : uint8_t");
                stream.WriteLine("\t{");

                foreach (SceneCommandDef def in commandDefs)
                {
                    stream.WriteLine("\t\tk" + def.Name + ",");
                }

                stream.WriteLine();
                stream.WriteLine("\t\tkCount");

                stream.WriteLine("\t};");
                stream.WriteLine("}");
            }

            using (StreamWriter stream = new StreamWriter(Path.Combine(outDir, "Anox_Build", "SceneCommandsDefs.generated.inl")))
            {
                stream.NewLine = "\n";

                stream.WriteLine("#include \"AnoxSceneCommandMetadata.h\"");
                stream.WriteLine();
                stream.WriteLine("namespace anox::buildsystem");
                stream.WriteLine("{");
                stream.WriteLine("\tconst SceneCommandParamDef g_sceneCommandParamDefs[] =");
                stream.WriteLine("\t{");

                foreach (SceneCommandDef cmdDef in commandDefs)
                {
                    foreach (ParamDef paramDef in cmdDef.Params)
                    {
                        stream.WriteLine("\t\t{");
                        stream.WriteLine("\t\t\t\"" + paramDef.Name + "\", " + paramDef.Name.Length.ToString() + ",");
                        stream.WriteLine("\t\t\tSceneCommandParamType::" + paramDef.Type.ToString());
                        stream.WriteLine("\t\t},");
                    }
                }

                stream.WriteLine("\t};");
                stream.WriteLine();
                stream.WriteLine("\tconst SceneCommandDef g_sceneCommands[] =");
                stream.WriteLine("\t{");

                int paramDefsOffset = 0;
                foreach (SceneCommandDef cmdDef in commandDefs)
                {
                    stream.WriteLine("\t\t{");
                    stream.WriteLine("\t\t\t\"" + cmdDef.Name.ToLowerInvariant() + "\", " + cmdDef.Name.Length.ToString() + ",");
                    stream.WriteLine("\t\t\t" + paramDefsOffset.ToString() + ", " + cmdDef.Params.Count.ToString() + ",");
                    stream.WriteLine("\t\t\t" + cmdDef.NumOptionalParameters.ToString() + ",");
                    stream.WriteLine("\t\t\t'" + cmdDef.Delimiter + "',");
                    stream.WriteLine("\t\t\t" + cmdDef.MayFail.ToString().ToLowerInvariant() + ",");
                    stream.WriteLine("\t\t},");

                    paramDefsOffset += cmdDef.Params.Count;
                }
                stream.WriteLine("\t};");
                stream.WriteLine("}");
            }
        }

        private static SceneCommandDef ParseCommandDef(IReadOnlyList<string> filteredTokens)
        {
            string name = filteredTokens[0];

            List<ParamDef> parameters = new List<ParamDef>();
            char delimiter = ',';
            uint numOptionalParameters = 0;

            bool isOptional = false;
            bool mayFail = false;

            for (int tokenIndex = 1; tokenIndex < filteredTokens.Count; tokenIndex++)
            {
                string token = filteredTokens[tokenIndex];
                string? tokenValue = null;

                int openParenLoc = token.IndexOf('(');
                if (openParenLoc >= 0)
                {
                    int closeParenLoc = token.IndexOf(')', openParenLoc + 1);

                    if (closeParenLoc < 0 || closeParenLoc != token.Length - 1)
                        throw new Exception("Malformed parameterized token: " + token);

                    tokenValue = token.Substring(openParenLoc + 1, closeParenLoc - openParenLoc - 1);
                    token = token.Substring(0, openParenLoc);
                }

                if (token == "optional")
                {
                    isOptional = true;
                    continue;
                }

                if (token == "mayfail")
                {
                    mayFail = true;
                    continue;
                }

                if (token == "delimiter")
                {
                    if (tokenValue == null || tokenValue.Length != 1)
                        throw new Exception("Malformed delimiter instruction: " + token);

                    delimiter = tokenValue[0];
                    continue;
                }

                if (isOptional)
                    numOptionalParameters++;

                ParamType paramType;
                if (token == "uint")
                    paramType = ParamType.UInt;
                else if (token == "float")
                    paramType = ParamType.Float;
                else if (token == "str")
                    paramType = ParamType.Str;
                else if (token == "entid")
                    paramType = ParamType.EntityID;
                else if (token == "hexuint")
                    paramType = ParamType.HexUInt;
                else if (token == "entitytype")
                    paramType = ParamType.EntityType;
                else if (token == "label")
                    paramType = ParamType.Label;
                else
                    throw new Exception("Invalid type: " + token);

                if (tokenValue == null)
                    throw new Exception("Missing name: " + token);

                parameters.Add(new ParamDef(paramType, tokenValue));
            }

            return new SceneCommandDef(name, parameters, numOptionalParameters, delimiter, mayFail);
        }

        static void Main(string[] args)
        {
            if (args.Length != 2)
                throw new Exception("Usage: SceneCommandReflector <scene defs file> <output directory>");

            CompileSceneDefs(args[0], args[1]);
        }
    }
}
