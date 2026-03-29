using Microsoft.VisualBasic.FileIO;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Reflection.Emit;
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

            DumpECFile(outFileBase, ec);
        }

        private static void DumpECFile(string outFileBase, EntityClassCollection ec)
        {
            List<string> sortedClasses = new List<string>();

            {
                List<string> sortedClassNames = new List<string>(ec.Classes.Keys);
                sortedClassNames.Sort();

                Dictionary<string, bool> classIsFinalized = new Dictionary<string, bool>();

                foreach (string className in sortedClassNames)
                    RecursiveDetermineDumpOrder(ec, className, classIsFinalized, sortedClasses);
            }

            string buildPath = Path.Combine(outFileBase, "Anox_Build");
            string gamePath = Path.Combine(outFileBase, "Anox_Game");
            string objectsPath = Path.Combine(outFileBase, "Anox_Game", "GameObjects");

            Directory.CreateDirectory(buildPath);
            Directory.CreateDirectory(gamePath);
            Directory.CreateDirectory(objectsPath);

            DumpLevelEntityDefs(buildPath, gamePath, objectsPath, ec, sortedClasses);

            int nn = 0;
        }

        private static void DumpLevelEntityDefs(string buildPath, string gamePath, string objectsPath, EntityClassCollection ec, IEnumerable<string> classNames)
        {
            Dictionary<string, int> classNameToClassID = new Dictionary<string, int>();
            List<string> levelClassList = new List<string>();

            foreach (string className in classNames)
            {
                ClassDef2 cdef = ec.Classes[className];

                if (cdef.ClassType == ClassType.Class)
                {
                    if (cdef.TryGetAttributeOfType<LevelTypeDefAttribute>() != null
                        || cdef.TryGetAttributeOfType<UserEntityTypeDefAttribute>() != null)
                        levelClassList.Add(className);
                }
            }

            HashSet<string> isOrIsUsedByLevelClass = new HashSet<string>();

            for (int i = 0; i < levelClassList.Count; i++)
                classNameToClassID[levelClassList[i]] = i;

            foreach (string className in levelClassList)
            {
                RecursiveAddUsedByLevelClass(isOrIsUsedByLevelClass, ec, className);
            }

            Dictionary<string, int> classSizes = new Dictionary<string, int>();
            Dictionary<string, int> fieldCounts = new Dictionary<string, int>();

            using (StreamWriter writer = new StreamWriter(Path.Combine(buildPath, "LevelEntities.generated.inl")))
            {
                writer.NewLine = "\n";

                writer.WriteLine("#include \"anox/Data/EntityDef.h\"");
                writer.WriteLine();

                writer.WriteLine("namespace anox::data::priv");
                writer.WriteLine("{");

                foreach (string className in levelClassList)
                {
                    Dictionary<string, LevelFieldInfo> fieldOffsetsDict = new Dictionary<string, LevelFieldInfo>();
                    int classSize = 0;
                    ResolveLevelFieldOffsets(ec, className, out classSize, fieldOffsetsDict, 0);

                    classSizes[className] = classSize;

                    List<string> sortedFieldNames = new List<string>(fieldOffsetsDict.Keys);
                    sortedFieldNames.Sort();

                    fieldCounts[className] = sortedFieldNames.Count;

                    if (sortedFieldNames.Count > 0)
                    {
                        writer.WriteLine($"\tstatic const EntityFieldDef2 gs_levelEntityClassFields_{className}[{sortedFieldNames.Count}] =");
                        writer.WriteLine("\t{");

                        foreach (string fieldName in sortedFieldNames)
                        {
                            LevelFieldInfo fieldInfo = fieldOffsetsDict[fieldName];
                            writer.WriteLine("\t\t{");
                            writer.WriteLine($"\t\t\t{fieldInfo.Offset},");
                            writer.Write($"\t\t\tEntityFieldType::");

                            string fieldTypeStr;
                            switch (fieldInfo.FieldType)
                            {
                                case FieldType.Float:
                                    fieldTypeStr = "kFloat";
                                    break;
                                case FieldType.UInt:
                                    fieldTypeStr = "kUInt";
                                    break;
                                case FieldType.Vec2:
                                    fieldTypeStr = "kVec2";
                                    break;
                                case FieldType.Vec3:
                                    fieldTypeStr = "kVec3";
                                    break;
                                case FieldType.Vec4:
                                    fieldTypeStr = "kVec4";
                                    break;
                                case FieldType.Bool:
                                    if (fieldInfo.IsOnOffBool)
                                        fieldTypeStr = "kBoolOnOff";
                                    else
                                        fieldTypeStr = "kBool";
                                    break;
                                case FieldType.ByteString:
                                    fieldTypeStr = "kByteString";
                                    break;
                                case FieldType.Label:
                                    fieldTypeStr = "kLabel";
                                    break;
                                case FieldType.BspModel:
                                    fieldTypeStr = "kBSPModel";
                                    break;
                                case FieldType.Broken:
                                    fieldTypeStr = "kIgnore";
                                    break;
                                case FieldType.EDef:
                                    fieldTypeStr = "kEntityDef";
                                    break;
                                default:
                                    throw new Exception("Unhandled field def");
                            }

                            writer.Write(fieldTypeStr);
                            writer.WriteLine(",");
                            writer.Write("\t\t\tu8\"");
                            writer.Write(fieldName);
                            writer.WriteLine("\",");
                            writer.WriteLine($"\t\t\t{fieldName.Length}");
                            writer.WriteLine("\t\t},");
                        }
                    }

                    writer.WriteLine("\t};");
                    writer.WriteLine();
                }

                writer.WriteLine("\tstatic const EntityClassDef2 gs_levelEntityClasses[" + levelClassList.Count + "] =");
                writer.WriteLine("\t{");

                for (int classIndex = 0; classIndex < levelClassList.Count; classIndex++)
                {
                    string className = levelClassList[classIndex];

                    ClassDef2 cdef = ec.Classes[className];

                    string levelClassName;

                    LevelTypeDefAttribute? levelTypeAttrib = cdef.TryGetAttributeOfType<LevelTypeDefAttribute>();
                    UserEntityTypeDefAttribute? userEntityTypeDefAttrib = cdef.TryGetAttributeOfType<UserEntityTypeDefAttribute>();

                    if (levelTypeAttrib != null)
                        levelClassName = levelTypeAttrib.NameInLevel;
                    else if (userEntityTypeDefAttrib != null)
                        levelClassName = "userentity_" + userEntityTypeDefAttrib.EdefName;
                    else
                        throw new Exception("Internal error: Failed to resolve type name");

                    writer.WriteLine("\t\t{");
                    writer.WriteLine($"\t\t\t{classIndex},");
                    writer.WriteLine($"\t\t\tu8\"{levelClassName}\",");
                    writer.WriteLine($"\t\t\t{levelClassName.Length},");
                    writer.WriteLine($"\t\t\t{classSizes[className]},");
                    writer.WriteLine($"\t\t\tgs_levelEntityClassFields_{className},");
                    writer.WriteLine($"\t\t\tsizeof(gs_levelEntityClassFields_{className}) / sizeof(gs_levelEntityClassFields_{className}[0]),");
                    writer.WriteLine("\t\t},");
                }

                writer.WriteLine("\t};");
                writer.WriteLine("}");
            }

            using (StreamWriter writer = new StreamWriter(Path.Combine(gamePath, "WorldObjectSpawnDispatcher.generated.inl")))
            {
                writer.NewLine = "\n";

                writer.WriteLine("namespace anox::game");
                writer.WriteLine("{");
                foreach (string className in levelClassList)
                    writer.WriteLine($"\tclass {className};");

                writer.WriteLine();
                writer.WriteLine("\trkit::Result WorldObjectFactory::CreateLevelObject(uint32_t levelObjectID, size_t &outSize, rkit::RCPtr<WorldObject> &outObject, void *&outFieldsRef, SerializeFromLevelFunction_t &outDeserializeFunction)");
                writer.WriteLine("\t{");
                writer.WriteLine("\t\toutObject.Reset();");
                writer.WriteLine("\t\toutSize = 0;");
                writer.WriteLine();
                writer.WriteLine("\t\tswitch (levelObjectID)");
                writer.WriteLine("\t\t{");
                for (int classIndex = 0; classIndex < levelClassList.Count; classIndex++)
                {
                    string className = levelClassList[classIndex];

                    ClassDef2 cdef = ec.Classes[className];
                    writer.WriteLine($"\t\tcase {classIndex}:");
                    writer.WriteLine($"\t\t\toutSize = {classSizes[className]};");
                    writer.WriteLine($"\t\t\treturn CreateLevelObjectTemplate<{className}>(outObject, outFieldsRef, outDeserializeFunction);");
                }
                writer.WriteLine("\t\tdefault:");
                writer.WriteLine("\t\t\tbreak;");
                writer.WriteLine("\t\t};");
                writer.WriteLine("\t\tRKIT_RETURN_OK;");
                writer.WriteLine("\t}");
                writer.WriteLine("}");
            }


            foreach (string className in classNames)
            {
                bool needsExplicitWorldObjectBase = false;

                using (StreamWriter writer = new StreamWriter(Path.Combine(objectsPath, className + ".generated.h")))
                {
                    writer.NewLine = "\n";

                    ClassDef2 cdef = ec.Classes[className];

                    writer.WriteLine("#pragma once");
                    writer.WriteLine();
                    writer.WriteLine("#include \"UserEntityDef.h\"");
                    writer.WriteLine("#include \"ObjectFields.h\"");
                    writer.WriteLine("#include \"rkit/Math/Vec.h\"");
                    writer.WriteLine("#include \"rkit/Core/String.h\"");

                    if (cdef.ClassType == ClassType.Class)
                    {
                        needsExplicitWorldObjectBase = true;
                        foreach (string baseClass in cdef.ParentClasses)
                        {
                            if (ec.Classes[baseClass].ClassType == ClassType.Class)
                            {
                                needsExplicitWorldObjectBase = false;
                                break;
                            }
                        }
                    }

                    if (cdef.ClassType == ClassType.Class)
                        writer.WriteLine("#include \"WorldObject.h\"");
                    else
                        writer.WriteLine("#include \"DynamicObject.h\"");

                    foreach (string baseClass in cdef.ParentClasses)
                        writer.WriteLine($"#include \"{baseClass}.h\"");

                    writer.WriteLine();
                    writer.WriteLine("namespace anox::game");
                    writer.WriteLine("{");
                    writer.WriteLine($"\tclass {className};");
                    writer.WriteLine("}");
                    writer.WriteLine();
                    writer.WriteLine("namespace anox::game::priv");
                    writer.WriteLine("{");
                    writer.WriteLine("\ttemplate<>");
                    writer.WriteLine($"\tstruct ObjectRTTIImpl<::anox::game::{className}>;");
                    writer.WriteLine("\ttemplate<>");
                    writer.WriteLine($"\tstruct ObjectFieldsImpl<::anox::game::{className}>;");
                    writer.WriteLine();
                    writer.WriteLine("\ttemplate<>");
                    writer.WriteLine($"\tstruct ObjectFieldsImpl<::anox::game::{className}>");
                    writer.WriteLine($"\t\t: public ObjectFieldsBase<::anox::game::{className}>");
                    writer.WriteLine("\t{");

                    foreach (FieldDef fieldDef in cdef.FieldDefs)
                    {
                        string? initValue = null;
                        string fieldType;
                        bool ignore = false;
                        switch (fieldDef.FieldType)
                        {
                            case FieldType.Float:
                                fieldType = "float";
                                initValue = "0.f";
                                break;
                            case FieldType.UInt:
                                fieldType = "uint32_t";
                                initValue = "0";
                                break;
                            case FieldType.Vec2:
                                fieldType = "::rkit::math::Vec2";
                                break;
                            case FieldType.Vec3:
                                fieldType = "::rkit::math::Vec3";
                                break;
                            case FieldType.Vec4:
                                fieldType = "::rkit::math::Vec4";
                                break;
                            case FieldType.Bool:
                                fieldType = "bool";
                                initValue = "false";
                                break;
                            case FieldType.ByteString:
                                fieldType = "::rkit::ByteString";
                                break;
                            case FieldType.Label:
                                fieldType = "::anox::Label";
                                break;
                            case FieldType.BspModel:
                                fieldType = "uint32_t";
                                initValue = "0";
                                break;
                            case FieldType.EDef:
                                fieldType = "::anox::game::UserEntityDef";
                                break;
                            case FieldType.Broken:
                                fieldType = "";
                                ignore = true;
                                break;
                            default:
                                throw new Exception("Unhandled field type");
                        };

                        if (fieldDef.TryGetAttributeOfType<AliasFieldAttribute>() != null)
                            ignore = true;

                        if (!ignore)
                        {
                            writer.Write("\t\t");
                            writer.Write(fieldType);
                            writer.Write(" m_");
                            writer.Write(fieldDef.FieldName);

                            if (initValue != null)
                            {
                                writer.Write(" = ");
                                writer.Write(initValue);
                            }

                            writer.WriteLine(";");
                        }
                    }
                    writer.WriteLine("\t};");
                    writer.WriteLine();
                    writer.WriteLine("\ttemplate<>");
                    writer.WriteLine($"\tstruct ObjectRTTIImpl<::anox::game::{className}>");
                    writer.WriteLine($"\t\t: private ObjectFieldsImpl<::anox::game::{className}>");

                    if (needsExplicitWorldObjectBase)
                        writer.WriteLine("\t\t, public ::anox::game::WorldObject");

                    if (cdef.ParentClasses.Count == 0)
                    {
                        if (cdef.ClassType == ClassType.Component)
                            writer.WriteLine("\t\t, public ::anox::game::DynamicObject");
                        else if (cdef.ClassType == ClassType.Class)
                        {
                        }
                        else
                            throw new Exception("Unhandled class type");
                    }
                    else
                    {
                        foreach (string baseClass in cdef.ParentClasses)
                            writer.WriteLine($"\t\t, public ::anox::game::{baseClass}");
                    }

                    writer.WriteLine("\t{");
                    writer.WriteLine("\t\tfriend struct ::anox::game::priv::PrivateAccessor;");
                    writer.WriteLine();



                    {
                        List<string> baseClassNames = new List<string>();
                        if (needsExplicitWorldObjectBase)
                            baseClassNames.Add("WorldObject");
                        baseClassNames.AddRange(cdef.ParentClasses);

                        writer.Write("\t\ttypedef ::rkit::TypeList<");
                        for (int i = 0; i < baseClassNames.Count; i++)
                        {
                            if (i != 0)
                                writer.Write(", ");
                            writer.Write(baseClassNames[i]);
                        }
                        writer.WriteLine("> BaseClasses_t;");
                    }
                    writer.WriteLine($"\t\ttypedef {className} ThisClass_t;");
                    writer.WriteLine($"\t\ttypedef ::anox::game::priv::AutoRTTI<ThisClass_t, BaseClasses_t> RTTIType_t;");

                    writer.WriteLine("\t\tconst RuntimeTypeInfo *GetMostDerivedType() override;");
                    writer.WriteLine("\t\tvoid *GetMostDerivedObject() override;");
                    writer.WriteLine("\t};");
                    writer.WriteLine();
                    writer.WriteLine("\ttemplate<>");
                    writer.WriteLine($"\tstruct ObjectRTTIResolver<::anox::game::{className}>");
                    writer.WriteLine("\t{");
                    writer.WriteLine($"\t\ttypedef ObjectFieldsImpl<::anox::game::{className}> FieldType_t;");
                    writer.WriteLine($"\t\ttypedef ObjectRTTIImpl<::anox::game::{className}> RTTIType_t;");
                    writer.WriteLine("\t};");
                    writer.WriteLine("}");
                }

                using (StreamWriter writer = new StreamWriter(Path.Combine(objectsPath, className + ".generated.inl")))
                {
                    writer.NewLine = "\n";

                    ClassDef2 cdef = ec.Classes[className];

                    writer.WriteLine("#include \"" + className + ".generated.h\"");
                    writer.WriteLine("#include \"AnoxWorldObjectFactory.h\"");
                    writer.WriteLine("#include \"EntityLevelLoader.h\"");
                    writer.WriteLine();
                    writer.WriteLine("namespace anox::game");
                    writer.WriteLine("{");

                    if (cdef.ClassType == ClassType.Class)
                    {
                        writer.WriteLine("\ttemplate<>");
                        writer.WriteLine($"\trkit::Result WorldObjectInstantiator<{className}>::CreateObject(rkit::UniquePtr<WorldObject> &outObject, ObjectFieldsBase<{className}> *&outFieldsRef)");
                        writer.WriteLine("\t{");
                        writer.WriteLine($"\t\trkit::UniquePtr<{className}> obj;");
                        writer.WriteLine($"\t\tRKIT_CHECK(rkit::New<{className}>(obj));");
                        writer.WriteLine($"\t\toutFieldsRef = ::anox::game::priv::PrivateAccessor::ImplicitCast<ObjectFieldsBase<{className}>>(obj.Get());");
                        writer.WriteLine("\t\toutObject = std::move(obj);");
                        writer.WriteLine("\t\tRKIT_RETURN_OK;");
                        writer.WriteLine("\t}");
                        writer.WriteLine();
                    }

                    if (isOrIsUsedByLevelClass.Contains(className))
                    {
                        writer.WriteLine("\ttemplate<>");
                        writer.WriteLine($"\trkit::Result WorldObjectInstantiator<{className}>::LoadObjectFromLevel(ObjectFieldsBase<{className}> &fieldsBase, const WorldObjectSpawnParams &spawnParams, const uint8_t *bytes)");
                        writer.WriteLine("\t{");
                        writer.WriteLine($"\t\tObjectFields<{className}> &fields = static_cast<ObjectFields<{className}> &>(fieldsBase);");

                        {
                            int classSize;
                            Dictionary<string, int> fieldOffsets = new Dictionary<string, int>();
                            Dictionary<string, int> parentClassOffsets = new Dictionary<string, int>();
                            Dictionary<string, LevelFieldInfo> levelFieldOffsets = new Dictionary<string, LevelFieldInfo>();
                            ResolveFieldOffsets(className, ec, className, out classSize, fieldOffsets, parentClassOffsets, levelFieldOffsets, 0);

                            foreach (string parentClass in cdef.ParentClasses)
                            {
                                writer.WriteLine($"\t\tWorldObjectInstantiator<{parentClass}>::LoadObjectFromLevel(");
                                writer.WriteLine($"\t\t\t*::anox::game::priv::PrivateAccessor::ImplicitCast<ObjectFieldsBase<{parentClass}>>(");
                                writer.WriteLine($"\t\t\t\t::anox::game::priv::PrivateAccessor::StaticCast<{className}>(&fields)");
                                writer.WriteLine($"\t\t\t), spawnParams, bytes + {parentClassOffsets[parentClass]});");
                            }

                            foreach (FieldDef fieldDef in cdef.FieldDefs)
                            {
                                if (fieldDef.TryGetAttributeOfType<AliasFieldAttribute>() != null || fieldDef.FieldType == FieldType.Broken)
                                    continue;

                                string fieldName = fieldDef.FieldName;
                                string fieldTypeName = fieldDef.FieldType.ToString();

                                if (FieldLoaderCanFault(fieldDef.FieldType))
                                {
                                    writer.WriteLine($"\t\tRKIT_CHECK(EntityLevelLoader::Load{fieldTypeName}(fields.m_{fieldName}, bytes + {fieldOffsets[fieldDef.FieldName]}, spawnParams));");
                                }
                                else
                                {
                                    writer.WriteLine($"\t\tEntityLevelLoader::Load{fieldTypeName}(fields.m_{fieldName}, bytes + {fieldOffsets[fieldDef.FieldName]});");
                                }
                            }
                        }

                        writer.WriteLine("\t\tRKIT_RETURN_OK;");
                        writer.WriteLine("\t}");
                    }
                    writer.WriteLine("}");
                    writer.WriteLine();

                    writer.WriteLine("namespace anox::game::priv");
                    writer.WriteLine("{");
                    writer.WriteLine($"\tconst RuntimeTypeInfo *ObjectRTTIImpl<::anox::game::{className}>::GetMostDerivedType()");
                    writer.WriteLine("\t{");


                    writer.WriteLine($"\t\treturn &AutoRTTI<::anox::game::{className}, BaseClasses_t>::ms_instance;");
                    writer.WriteLine("\t}");
                    writer.WriteLine();
                    writer.WriteLine($"\tvoid *ObjectRTTIImpl<::anox::game::{className}>::GetMostDerivedObject()");
                    writer.WriteLine("\t{");
                    writer.WriteLine($"\t\treturn static_cast<::anox::game::{className} *>(this);");
                    writer.WriteLine("\t}");
                    writer.WriteLine("}");
                }
            }
        }

        private static void RecursiveAddUsedByLevelClass(HashSet<string> isOrIsUsedByLevelClass, EntityClassCollection ec, string className)
        {
            if (isOrIsUsedByLevelClass.Add(className))
            {
                ClassDef2 cls = ec.Classes[className];
                foreach (string parentClass in cls.ParentClasses)
                    RecursiveAddUsedByLevelClass(isOrIsUsedByLevelClass, ec, parentClass);
            }
        }

        private static bool FieldLoaderCanFault(FieldType fieldType)
        {
            switch (fieldType)
            {
                case FieldType.Float:
                case FieldType.UInt:
                case FieldType.Vec2:
                case FieldType.Vec3:
                case FieldType.Vec4:
                case FieldType.Bool:
                case FieldType.Label:
                case FieldType.BspModel:
                    return false;
                case FieldType.ByteString:
                case FieldType.EDef:
                    return true;
                default:
                    throw new Exception("Unhandled field type");
            }
        }

        private static void ResolveLevelFieldOffsets(EntityClassCollection ec, string className, out int classSize, Dictionary<string, LevelFieldInfo> levelFieldOffsetsDict, int baseOffset)
        {
            Dictionary<string, int> fieldOffsetsDict = new Dictionary<string, int>();
            Dictionary<string, int> parentClassOffsetsDict = new Dictionary<string, int>();

            ResolveFieldOffsets(className, ec, className, out classSize, fieldOffsetsDict, parentClassOffsetsDict, levelFieldOffsetsDict, baseOffset);
        }

        private static void ResolveFieldOffsets(string topLevelClass, EntityClassCollection ec, string className, out int classSize, Dictionary<string, int> fieldOffsetsDict, Dictionary<string, int> parentClassesOffsetsDict, Dictionary<string, LevelFieldInfo> levelFieldOffsetsDict, int baseOffset)
        {
            classSize = 0;
            ClassDef2 cdef = ec.Classes[className];

            foreach (string parentClass in cdef.ParentClasses)
            {
                Dictionary<string, int> scratchFieldOffsetsDict = new Dictionary<string, int>();
                Dictionary<string, int> scratchParentClassOffsetsDict = new Dictionary<string, int>();
                int parentClassSize = 0;

                ResolveFieldOffsets(topLevelClass, ec, parentClass, out parentClassSize, scratchFieldOffsetsDict, scratchParentClassOffsetsDict, levelFieldOffsetsDict, baseOffset + classSize);

                parentClassesOffsetsDict[parentClass] = classSize + baseOffset;

                classSize += parentClassSize;
            }

            Dictionary<string, FieldDef> nameToField = new Dictionary<string, FieldDef>();

            foreach (FieldDef fieldDef in cdef.FieldDefs)
            {
                int fieldOffset = classSize + baseOffset;
                AliasFieldAttribute? aliasAttrib = fieldDef.TryGetAttributeOfType<AliasFieldAttribute>();
                if (aliasAttrib == null)
                {
                    int fieldSize = ResolveFieldSize(fieldDef.FieldType);
                    classSize += fieldSize;
                }
                else
                {
                    FieldDef existingFieldDef;
                    if (!nameToField.TryGetValue(aliasAttrib.AliasName, out existingFieldDef))
                        throw new ReflectorException("Field " + fieldDef.FieldName + " references alias field " + aliasAttrib.AliasName + " which hasn't been defined prior to it");

                    if (existingFieldDef.FieldType != fieldDef.FieldType)
                        throw new ReflectorException("Field " + fieldDef.FieldName + " references alias field " + aliasAttrib.AliasName + " which is a different type");

                    fieldOffset = fieldOffsetsDict[aliasAttrib.AliasName];
                }

                nameToField[fieldDef.FieldName] = fieldDef;

                if (!fieldOffsetsDict.TryAdd(fieldDef.FieldName, fieldOffset))
                    throw new ReflectorException("Field " + fieldDef.FieldName + " was defined multiple times in class " + topLevelClass);

                bool isOnOff = false;
                LevelFieldAttribute? levelFieldAttrib = fieldDef.TryGetAttributeOfType<LevelFieldAttribute>();
                if (levelFieldAttrib != null)
                {
                    isOnOff = levelFieldAttrib.IsOnOff;
                    List<KeyValuePair<string, LevelFieldInfo>> levelFieldInfos = new List<KeyValuePair<string, LevelFieldInfo>>();
                    string[]? scalarizedFields = levelFieldAttrib.ScalarizedFields;
                    if (scalarizedFields == null)
                    {
                        string name = fieldDef.FieldName;
                        if (levelFieldAttrib.OverrideName != null)
                            name = levelFieldAttrib.OverrideName;

                        levelFieldInfos.Add(new KeyValuePair<string, LevelFieldInfo>(name, new LevelFieldInfo(fieldDef.FieldType, fieldOffset)));
                    }
                    else
                    {
                        if (levelFieldAttrib.OverrideName != null)
                            throw new ReflectorException("Field " + fieldDef.FieldName + " can't use both name and scalarized");

                        int expectedScalarFields = 0;
                        FieldType scalarFieldFype;
                        switch (fieldDef.FieldType)
                        {
                            case FieldType.Vec2:
                                expectedScalarFields = 2;
                                scalarFieldFype = FieldType.Float;
                                break;
                            case FieldType.Vec3:
                                expectedScalarFields = 3;
                                scalarFieldFype = FieldType.Float;
                                break;
                            case FieldType.Vec4:
                                expectedScalarFields = 4;
                                scalarFieldFype = FieldType.Float;
                                break;
                            default:
                                throw new ReflectorException("Scalarized level field " + fieldDef.FieldName + " is not a scalarizable type");
                        }

                        if (expectedScalarFields != scalarizedFields!.Length)
                            throw new ReflectorException($"Scalarized level field {fieldDef.FieldName} requires {expectedScalarFields} but only {scalarizedFields!.Length} were provided");

                        int scalarSize = ResolveFieldSize(scalarFieldFype);

                        for (int i = 0; i < expectedScalarFields; i++)
                            levelFieldInfos.Add(new KeyValuePair<string, LevelFieldInfo>(scalarizedFields[i], new LevelFieldInfo(scalarFieldFype, fieldOffset + i * scalarSize)));
                    }

                    foreach (KeyValuePair<string, LevelFieldInfo> levelFieldInfo in levelFieldInfos)
                    {
                        LevelFieldInfo lfi = levelFieldInfo.Value;

                        if (lfi.IsOnOffBool && lfi.FieldType != FieldType.Bool)
                            throw new ReflectorException("Level field " + levelFieldInfo.Key + " is flagged as onoff but isn't bool");

                        if (!levelFieldOffsetsDict.TryAdd(levelFieldInfo.Key, lfi))
                            throw new ReflectorException("Level field " + levelFieldInfo.Key + " was set multiple times");
                    }
                }
            }
        }

        private static int ResolveFieldSize(FieldType fieldType)
        {
            switch (fieldType)
            {
                case FieldType.Bool:
                    return 1;
                case FieldType.Float:
                case FieldType.UInt:
                case FieldType.EDef:
                case FieldType.ByteString:
                case FieldType.Label:
                case FieldType.BspModel:
                    return 4;
                case FieldType.Vec2:
                    return 8;
                case FieldType.Vec3:
                    return 12;
                case FieldType.Vec4:
                    return 16;
                case FieldType.Broken:
                    return 0;
                default:
                    throw new Exception("Unhandled field type");
            }
        }

        private static void RecursiveDetermineDumpOrder(EntityClassCollection ec, string className, Dictionary<string, bool> classIsFinalized, List<string> sortedClasses)
        {
            if (classIsFinalized.TryGetValue(className, out bool isFinalized))
            {
                if (isFinalized)
                    return;
                else
                    throw new ReflectorException("Class " + className + " inherits from itself");
            }

            classIsFinalized[className] = false;

            HashSet<string> hasParentAlready = new HashSet<string>();
            foreach (string parentClass in ec.Classes[className].ParentClasses)
            {
                if (hasParentAlready.Contains(parentClass))
                    throw new ReflectorException("Class " + className + " includes parent class " + parentClass + " multiple times");

                hasParentAlready.Add(parentClass);

                RecursiveDetermineDumpOrder(ec, parentClass, classIsFinalized, sortedClasses);
            }

            sortedClasses.Add(className);
            classIsFinalized[className] = true;
        }

        private static void CompileECFile(EntityClassCollection ec, List<string> lines)
        {
            int lineNum = 0;

            List<TypeDefAttribute> typeDefAttribs = new List<TypeDefAttribute>();
            while (lineNum < lines.Count)
            {
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
                    ec.AddClass(ParseClass(ClassType.Component, typeDefAttribs.ToArray(), lines, ref lineNum, line, col));
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
            string className = PullTokenOfType(line, ref col, TokenType.Identifier);

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

            return new ClassDef2(className, classType, typeDefAttributes, parentClasses, fieldDefs.ToArray());
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
