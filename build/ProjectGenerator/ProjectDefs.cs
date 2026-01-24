using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ProjectGenerator
{
    internal class ProjectDef
    {
        public enum FileType
        {
            Auto,
            Source,
            Include,
            Content,
            Misc,
            Resource,
        }


        public struct DirectoryMapping
        {
            public string SourceDirectory { get; set; }
            public string FilterDirectory { get; set; }
            public bool? Recursive { get; set; }
        }

        public struct ExtraFile
        {
            public string FilePath { get; set; }
            public FileType FileType { get; set; }

            public ExtraFile(JsonElement element)
            {
                FileType = FileType.Auto;
                FilePath = "";

                if (element.ValueKind == JsonValueKind.String)
                    FilePath = element.GetString()!;
                else if (element.ValueKind == JsonValueKind.Object)
                {
                    bool isPathDefined = false;
                    foreach (JsonProperty prop in element.EnumerateObject())
                    {
                        if (prop.Name == "path")
                        {
                            if (prop.Value.ValueKind != JsonValueKind.String)
                                throw new Exception("Invalid element type for 'extra_files.path'");

                            isPathDefined = true;
                            FilePath = prop.Value.GetString()!;
                        }
                        else if (prop.Name == "type")
                        {
                            if (prop.Value.ValueKind != JsonValueKind.String)
                                throw new Exception("Invalid element type for 'extra_files.type'");

                            string typeStr = prop.Value.GetString()!;
                            if (typeStr == "auto")
                                FileType = FileType.Auto;
                            else if (typeStr == "source")
                                FileType = FileType.Source;
                            else if (typeStr == "include")
                                FileType = FileType.Include;
                            else if (typeStr == "content")
                                FileType = FileType.Content;
                            else
                                throw new Exception("Invalid value for 'extra_files.type'");
                        }
                        else
                            throw new Exception($"Invalid property '{prop.Name}' for 'extra_files'");
                    }

                    if (!isPathDefined)
                        throw new Exception("Path wasn't defined for 'extra_files' entry");
                }
                else
                    throw new Exception("Invalid element type for 'extra_files'");
            }
        }

        public enum Type
        {
            Unknown,

            Module,
            LinkedModule,

            Lib,
            Executable,
            LooseDll,
            LinkedDll,
            AlwaysLinkedDll,
            NeverLinkedDll,
        }

        public Type ProjectType { get; private set; }
        public IReadOnlyList<string> Configs { get => _configs; }
        public IReadOnlyList<DirectoryMapping> DirectoryMappings { get => _dirMappings; }
        public IReadOnlyList<ExtraFile> ExtraFiles { get => _extraFiles; }
        public IReadOnlyList<string> Refs { get => _refs; }

        public bool DevOnly { get; private set; }

        private List<string> _configs = new List<string>();
        private List<DirectoryMapping> _dirMappings = new List<DirectoryMapping>();
        private List<ExtraFile> _extraFiles = new List<ExtraFile>();
        private List<string> _refs = new List<string>();

        public ProjectDef(JsonElement json)
        {
            if (json.ValueKind != JsonValueKind.Object)
                throw new JsonException("Invalid project def type");

            foreach (JsonProperty property in json.EnumerateObject())
            {
                string propertyName = property.Name;

                if (propertyName == "type")
                {
                    if (property.Value.ValueKind != JsonValueKind.String)
                        throw new JsonException("Invalid project def 'type' type");

                    string? typeStr = property.Value.GetString();
                    if (typeStr == "module")
                        ProjectType = Type.Module;
                    else if (typeStr == "linked_module")
                        ProjectType = Type.LinkedModule;
                    else if (typeStr == "lib")
                        ProjectType = Type.Lib;
                    else if (typeStr == "dll")
                        ProjectType = Type.AlwaysLinkedDll;
                    else if (typeStr == "loose_dll")
                        ProjectType = Type.NeverLinkedDll;
                    else if (typeStr == "exe")
                        ProjectType = Type.Executable;
                    else
                        throw new JsonException("Invalid project def 'type' value");
                }
                else if (propertyName == "configs")
                {
                    if (property.Value.ValueKind != JsonValueKind.Array)
                        throw new JsonException("Invalid project def 'configs' type");

                    foreach (JsonElement configElement in property.Value.EnumerateArray())
                    {
                        if (configElement.ValueKind != JsonValueKind.String)
                            throw new JsonException("Invalid project def config type");

                        _configs.Add(configElement.GetString()!);
                    }
                }
                else if (propertyName == "dev_only")
                {
                    if (property.Value.ValueKind == JsonValueKind.True)
                        DevOnly = true;
                    else if (property.Value.ValueKind == JsonValueKind.False)
                        DevOnly = false;
                    else
                        throw new JsonException("Invalid project def 'dev_only' type");
                }
                else if (propertyName == "extra_files")
                {
                    if (property.Value.ValueKind != JsonValueKind.Array)
                        throw new JsonException("Invalid project def 'extra_files' type");

                    foreach (JsonElement efElement in property.Value.EnumerateArray())
                        _extraFiles.Add(new ExtraFile(efElement));
                }
                else if (propertyName == "refs")
                {
                    if (property.Value.ValueKind != JsonValueKind.Array)
                        throw new JsonException("Invalid project def 'extra_files' type");

                    foreach (JsonElement refElement in property.Value.EnumerateArray())
                    {
                        if (refElement.ValueKind != JsonValueKind.String)
                            throw new JsonException("Invalid project def 'refs' type");

                        _refs.Add(refElement.GetString()!);
                    }
                }
                else if (propertyName == "extra_dirs")
                {
                    if (property.Value.ValueKind != JsonValueKind.Array)
                        throw new JsonException("Invalid project def 'extra_dirs' type");

                    foreach (JsonElement refElement in property.Value.EnumerateArray())
                    {
                        if (refElement.ValueKind == JsonValueKind.String)
                        {
                            _dirMappings.Add(new DirectoryMapping()
                            {
                                SourceDirectory = refElement.GetString()!,
                                FilterDirectory = "",
                            });
                        }
                        else if (refElement.ValueKind == JsonValueKind.Object)
                        {
                            string? path = null;
                            string target = "";
                            bool? recursive = null;
                            foreach (JsonProperty prop in refElement.EnumerateObject())
                            {

                                if (prop.Name == "path")
                                {
                                    if (prop.Value.ValueKind != JsonValueKind.String)
                                        throw new JsonException("Invalid value type for 'extra_dirs.path'");

                                    path = prop.Value.GetString();
                                }
                                else if (prop.Name == "target")
                                {
                                    if (prop.Value.ValueKind != JsonValueKind.String)
                                        throw new JsonException("Invalid value type for 'extra_dirs.target'");

                                    target = prop.Value.GetString();
                                }
                                else if (prop.Name == "target")
                                {
                                    if (prop.Value.ValueKind == JsonValueKind.True)
                                        recursive = true;
                                    else if (prop.Value.ValueKind == JsonValueKind.False)
                                        recursive = false;
                                    else
                                        throw new JsonException("Invalid value type for 'extra_dirs.recursive'");
                                }
                            }

                            if (path == null)
                                throw new JsonException("'extra_dirs' entry had no path'");

                            _dirMappings.Add(new DirectoryMapping()
                            {
                                SourceDirectory = path,
                                FilterDirectory = target,
                                Recursive = recursive,
                            });
                        }
                        else
                            throw new JsonException("Invalid project def 'extra_dirs' type");
                    }
                }
                else
                    throw new JsonException($"Unknown project def property {propertyName}");
            }

            if (this.ProjectType == Type.Unknown)
                throw new JsonException("Project def had no type");
        }
    }

    internal class ProjectDefs
    {
        public IReadOnlyDictionary<string, ProjectDef> Defs { get => _defs; }
        private Dictionary<string, ProjectDef> _defs = new Dictionary<string, ProjectDef>();

        public ProjectDefs(string jsonPath)
        {
            JsonDocumentOptions options = new JsonDocumentOptions();
            options.AllowTrailingCommas = true;
            options.CommentHandling = JsonCommentHandling.Skip;

            JsonElement rootElement;
            using (FileStream fs = new FileStream(jsonPath, FileMode.Open, FileAccess.Read))
            {
                rootElement = JsonDocument.Parse(fs, options).RootElement;
            }

            if (rootElement.ValueKind != JsonValueKind.Object)
                throw new JsonException("Invalid project defs type");

            foreach (JsonProperty property in rootElement.EnumerateObject())
            {
                _defs[property.Name] = new ProjectDef(property.Value);
            }
        }
    }
}
