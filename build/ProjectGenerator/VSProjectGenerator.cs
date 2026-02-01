using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Reflection.Metadata;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Linq;
using static System.Net.Mime.MediaTypeNames;

namespace ProjectGenerator
{
    internal class VSProjectGenerator
    {
        private class InternalState
        {
            public ProjectDefs ProjectDefs { get; set; }
            public Dictionary<string, System.Guid> ProjectGuids { get; } = new Dictionary<string, System.Guid>();
            public GlobalConfiguration GlobalConfiguration { get; internal set; }
            public IReadOnlyDictionary<ProjectDef, ProjectResolver> Resolvers { get; private set; }
            public TargetDefs TargetDefs { get; private set; }

            public InternalState(ProjectDefs projDefs, GlobalConfiguration globalConfiguration, IReadOnlyDictionary<ProjectDef, ProjectResolver> resolvers, TargetDefs targetDefs)
            {
                ProjectDefs = projDefs;
                GlobalConfiguration = globalConfiguration;
                Resolvers = resolvers;
                TargetDefs = targetDefs;
            }
        }

        private static readonly string _xmlNS = "http://schemas.microsoft.com/developer/msbuild/2003";
        private static readonly string _vcppGuidStr = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942";
        private static readonly string _slnFolderGuidStr = "2150E333-8FDC-42A3-9474-1A3956D46DE8";
        private static readonly string _slnFolderImplGuidStr = "8EC462FD-D22E-90A8-E5CE-7E832BA40C5D";

        internal static string ComputeRelativePath(string projectDirectory, string itemPath)
        {
            char dirSeparatorChar = Path.DirectorySeparatorChar;
            char altDirSeparatorChar = Path.AltDirectorySeparatorChar;

            projectDirectory = projectDirectory.Replace(altDirSeparatorChar, dirSeparatorChar);
            itemPath = itemPath.Replace(altDirSeparatorChar, dirSeparatorChar);

            projectDirectory = Path.TrimEndingDirectorySeparator(projectDirectory) + dirSeparatorChar;

            ReadOnlySpan<char> projSpan = projectDirectory.AsSpan();
            ReadOnlySpan<char> itemPathSpan = itemPath.AsSpan();

            int shorterLength = Math.Min(projSpan.Length, itemPathSpan.Length);

            int commonBaseStart = 0;
            for (int i = 0; i < shorterLength; i++)
            {
                char cProj = projSpan[i];
                char cItem = itemPathSpan[i];
                if (cProj != cItem)
                    break;

                if (cProj == dirSeparatorChar)
                    commonBaseStart = i + 1;
            }

            string upDirPrefixes = "";

            for (int i = commonBaseStart; i < projSpan.Length; i++)
            {
                if (projSpan[i] == dirSeparatorChar)
                    upDirPrefixes += ".." + dirSeparatorChar;
            }

            return upDirPrefixes + itemPath.Substring(commonBaseStart);
        }

        internal static string SimplifyNamespace(string str)
        {
            return str.Replace("_", "");
        }

        internal void AppendSimpleKey(XmlElement element, string key, string value)
        {
            XmlDocument document = element.OwnerDocument;

            XmlElement childElement = document.CreateElement(key, _xmlNS);
            childElement.AppendChild(document.CreateTextNode(value));

            element.AppendChild(childElement);
        }

        internal void AddProjectConfigurations(XmlElement projElement, TargetDefs targetDefs)
        {
            XmlDocument document = projElement.OwnerDocument;

            XmlElement itemGroupElement = document.CreateElement("ItemGroup", _xmlNS);
            itemGroupElement.SetAttribute("Label", "ProjectConfigurations");

            foreach (TargetPlatform platform in targetDefs.Platforms)
            {
                foreach (TargetConfiguration config in targetDefs.Configurations)
                {
                    AddProjectConfiguration(itemGroupElement, config, platform);
                }
            }
            projElement.AppendChild(itemGroupElement);
        }

        private void AddProjectConfiguration(XmlElement itemGroupElement, TargetConfiguration config, TargetPlatform platform)
        {
            XmlDocument document = itemGroupElement.OwnerDocument;

            XmlElement projectConfigurationElement = document.CreateElement("ProjectConfiguration", _xmlNS);
            projectConfigurationElement.SetAttribute("Include", config.Name + "|" + platform.ArchName);

            AppendSimpleKey(projectConfigurationElement, "Configuration", config.Name);
            AppendSimpleKey(projectConfigurationElement, "Platform", platform.ArchName);

            itemGroupElement.AppendChild(projectConfigurationElement);
        }

        internal static T? ResolveConfigProperty<T>(TargetConfiguration config, Func<TargetConfiguration, T?> propertyReader)
        {
            T? property = propertyReader(config);
            if (property != null)
                return property;

            foreach (TargetConfiguration inherited in config.Inherit)
            {
                property = ResolveConfigProperty<T>(inherited, propertyReader);
                if (property != null)
                    return property;
            }

            return default(T?);
        }

        internal static ProjectDef.Type ResolveProjectType(TargetConfiguration config, ProjectDef.Type projType)
        {
            if (projType == ProjectDef.Type.Module)
            {
                ProjectDef.Type? configProjType = ResolveConfigProperty(config, ProjectDef.Type? (TargetConfiguration config) => config.ModuleProjectType);
                if (configProjType == null)
                    throw new Exception($"Couldn't resolve module type for config {config.Name}");
                else
                    return (ProjectDef.Type)configProjType;
            }
            else if (projType == ProjectDef.Type.LinkedModule)
            {
                ProjectDef.Type? configProjType = ResolveConfigProperty(config, ProjectDef.Type? (TargetConfiguration config) => config.LinkedModuleProjectType);
                if (configProjType == null)
                    throw new Exception($"Couldn't resolve linked module type for config {config.Name}");
                else
                    return (ProjectDef.Type)configProjType;
            }

            return projType;
        }

        internal void AddConfigurations(XmlElement projElement, ProjectDef projectDef, TargetDefs targetDefs)
        {
            XmlDocument document = projElement.OwnerDocument;

            foreach (TargetPlatform platform in targetDefs.Platforms)
            {
                foreach (TargetConfiguration config in targetDefs.Configurations)
                {
                    XmlElement propertyGroupElement = document.CreateElement("PropertyGroup", _xmlNS);
                    propertyGroupElement.SetAttribute("Condition", $"'$(Configuration)|$(Platform)'=='{config.Name}|{platform.ArchName}'");
                    propertyGroupElement.SetAttribute("Label", "Configuration");

                    ProjectDef.Type projType = ResolveProjectType(config, projectDef.ProjectType);

                    string configTypeName;
                    switch (projType)
                    {
                        case ProjectDef.Type.LooseDll:
                        case ProjectDef.Type.LinkedDll:
                        case ProjectDef.Type.AlwaysLinkedDll:
                        case ProjectDef.Type.NeverLinkedDll:
                            configTypeName = "DynamicLibrary";
                            break;
                        case ProjectDef.Type.Lib:
                            configTypeName = "StaticLibrary";
                            break;
                        case ProjectDef.Type.Executable:
                            configTypeName = "Application";
                            break;
                        default:
                            throw new Exception("Not implemented project type");
                    }

                    bool? useDebugLibraries = ResolveConfigProperty(config, bool? (TargetConfiguration config) => config.UseDebugLibraries);
                    string? platformToolset = ResolveConfigProperty(config, string? (TargetConfiguration config) => config.PlatformToolset);

                    if (platformToolset == null)
                        throw new Exception($"Configuration {config.Name} missing platform toolset");

                    AppendSimpleKey(propertyGroupElement, "ConfigurationType", configTypeName);
                    AppendSimpleKey(propertyGroupElement, "UseDebugLibraries", (useDebugLibraries ?? false) ? "true" : "false");
                    AppendSimpleKey(propertyGroupElement, "PlatformToolset", platformToolset);
                    AppendSimpleKey(propertyGroupElement, "CharacterSet", "Unicode");

                    if (projType == ProjectDef.Type.Executable)
                    {
                        AppendSimpleKey(propertyGroupElement, "CopyLocalDeploymentContent", "true");
                        //AppendSimpleKey(propertyGroupElement, "CopyCppRuntimeToOutputDir", "true");
                    }

                    projElement.AppendChild(propertyGroupElement);
                }
            }
        }

        private void AddGlobals(XmlElement projElement, string name, System.Guid guid)
        {
            XmlDocument document = projElement.OwnerDocument;

            XmlElement propertyGroupElement = document.CreateElement("PropertyGroup", _xmlNS);
            propertyGroupElement.SetAttribute("Label", "Globals");

            AppendSimpleKey(propertyGroupElement, "VCProjectVersion", "17.0");
            AppendSimpleKey(propertyGroupElement, "Keyword", "Win32Proj");
            AppendSimpleKey(propertyGroupElement, "ProjectGuid", "{" + guid.ToString() + "}");
            AppendSimpleKey(propertyGroupElement, "RootNamespace", SimplifyNamespace(name));
            AppendSimpleKey(propertyGroupElement, "WindowsTargetPlatformVersion", "10.0");

            projElement.AppendChild(propertyGroupElement);
        }

        private void GenerateProject(GlobalConfiguration gconfig, string name, ProjectDef projDef, ProjectResolver resolver, TargetDefs targetDefs, OutputFileCollection outputFiles, InternalState state)
        {
            GenerateProjectFile(gconfig, name, projDef, resolver, targetDefs, outputFiles, state);
            GenerateFiltersFile(gconfig, name, projDef, resolver, targetDefs, outputFiles, state);
        }

        private void GenerateProjectFile(GlobalConfiguration gconfig, string name, ProjectDef projDef, ProjectResolver resolver, TargetDefs targetDefs, OutputFileCollection outputFiles, InternalState state)
        {
            string projectDir = name;
            string projectPath = Path.Combine(projectDir, name + ".vcxproj");

            XmlDocument xmlDocument = new XmlDocument();
            xmlDocument.AppendChild(xmlDocument.CreateXmlDeclaration("1.0", "utf-8", null));

            XmlElement projElement = xmlDocument.CreateElement("Project", _xmlNS);
            projElement.SetAttribute("DefaultTargets", "Build");

            AddProjectConfigurations(projElement, targetDefs);
            AddGlobals(projElement, name, state.ProjectGuids[name]);

            {
                XmlElement propsImportElement = xmlDocument.CreateElement("Import", _xmlNS);
                propsImportElement.SetAttribute("Project", @"$(VCTargetsPath)\Microsoft.Cpp.Default.props");
                projElement.AppendChild(propsImportElement);
            }

            AddConfigurations(projElement, projDef, targetDefs);

            {
                XmlElement propsImportElement = xmlDocument.CreateElement("Import", _xmlNS);
                propsImportElement.SetAttribute("Project", @"$(VCTargetsPath)\Microsoft.Cpp.props");
                projElement.AppendChild(propsImportElement);
            }

            AddPropertySheetImports(projElement, projectDir, projDef, targetDefs);

            AddFiles(projElement, gconfig.RootPath!, projectDir, resolver);

            AddProjectRefs(projElement, projectDir, projDef, resolver, state);

            {
                XmlElement propsImportElement = xmlDocument.CreateElement("Import", _xmlNS);
                propsImportElement.SetAttribute("Project", @"$(VCTargetsPath)\Microsoft.Cpp.targets");
                projElement.AppendChild(propsImportElement);
            }

            xmlDocument.AppendChild(projElement);

            Stream outStream = outputFiles.Open(projectPath);
            using (XmlTextWriter writer = new XmlTextWriter(outStream, Encoding.UTF8))
            {
                writer.Formatting = Formatting.Indented;
                xmlDocument.WriteTo(writer);
            }
        }

        private void GenerateFiltersFile(GlobalConfiguration gconfig, string name, ProjectDef projDef, ProjectResolver resolver, TargetDefs targetDefs, OutputFileCollection outputFiles, InternalState state)
        {
            string projectDir = name;
            string projectPath = Path.Combine(projectDir, name + ".vcxproj.filters");

            XmlDocument xmlDocument = new XmlDocument();
            xmlDocument.AppendChild(xmlDocument.CreateXmlDeclaration("1.0", "utf-8", null));

            XmlElement projElement = xmlDocument.CreateElement("Project", _xmlNS);
            projElement.SetAttribute("ToolsVersion", "4.0");

            AddFilterGroups(projElement, projectDir, resolver.ResolvedFiles);

            xmlDocument.AppendChild(projElement);

            Stream outStream = outputFiles.Open(projectPath);
            using (XmlTextWriter writer = new XmlTextWriter(outStream, Encoding.UTF8))
            {
                writer.Formatting = Formatting.Indented;
                xmlDocument.WriteTo(writer);
            }
        }

        private static void AddFilterGroup(XmlElement itemGroupElement, string name, string guidStr, string? extensions)
        {
            XmlDocument document = itemGroupElement.OwnerDocument;

            XmlElement filterElement = document.CreateElement("Filter", _xmlNS);
            filterElement.SetAttribute("Include", name);

            XmlElement uidElement = document.CreateElement("UniqueIdentifier", _xmlNS);
            uidElement.AppendChild(document.CreateTextNode("{" + guidStr + "}"));

            filterElement.AppendChild(uidElement);

            if (extensions != null)
            {
                XmlElement extensionsElement = document.CreateElement("Extensions", _xmlNS);
                extensionsElement.AppendChild(document.CreateTextNode(extensions));

                filterElement.AppendChild(extensionsElement);
            }

            itemGroupElement.AppendChild(filterElement);
        }

        private static void AddFilterGroups(XmlElement projElement, string projectDir, IReadOnlyList<ProjectResolver.ResolvedFile> resolvedFiles)
        {
            XmlDocument document = projElement.OwnerDocument;

            HashSet<ProjectDef.FileType> fileTypesSet = new HashSet<ProjectDef.FileType>();

            List<string> fileBins = new List<string>();
            List<string> uniqueFileBins = new List<string>();
            HashSet<string> uniqueFileBinsSet = new HashSet<string>();

            List<string> reservedBinsList = new List<string>();
            reservedBinsList.Add("Source Files");
            reservedBinsList.Add("Header Files");
            reservedBinsList.Add("Resource Files");

            HashSet<string> reservedBinsSet = new HashSet<string>();
            foreach (string binName in reservedBinsList)
            {
                reservedBinsSet.Add(binName.ToUpperInvariant());
            }

            foreach (ProjectResolver.ResolvedFile file in resolvedFiles)
            {
                fileTypesSet.Add(file.FileType);

                string bin = "";
                switch (file.FileType)
                {
                    case ProjectDef.FileType.Include:
                        bin = "Header Files";
                        break;
                    case ProjectDef.FileType.Source:
                        bin = "Source Files";
                        break;
                    case ProjectDef.FileType.Resource:
                        bin = "Resource Files";
                        break;
                    case ProjectDef.FileType.Misc:
                        bin = "";
                        break;
                    case ProjectDef.FileType.Content:
                        bin = "Content";
                        break;
                    default:
                        throw new NotImplementedException();
                }

                string fullFilterPath = Path.Combine(bin, file.FilterPath);

                string fullBin = Path.GetDirectoryName(fullFilterPath).Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);

                AddBin(fullBin, uniqueFileBins, uniqueFileBinsSet, reservedBinsSet);

                fileBins.Add(fullBin);
            }

            uniqueFileBins.Sort(int (string a, string b) => string.Compare(a, b, StringComparison.OrdinalIgnoreCase));

            {
                XmlElement itemGroupElement = document.CreateElement("ItemGroup", _xmlNS);

                if (fileTypesSet.Contains(ProjectDef.FileType.Source))
                    AddFilterGroup(itemGroupElement, "Source Files", "4FC737F1-C7A5-4376-A066-2A32D752A2FF", "cpp;c;cc;cxx;c++;cppm;ixx;def;odl;idl;hpj;bat;asm;asmx");

                if (fileTypesSet.Contains(ProjectDef.FileType.Include))
                    AddFilterGroup(itemGroupElement, "Header Files", "93995380-89BD-4b04-88EB-625FBE52EBFB", "h;hh;hpp;hxx;h++;hm;inl;inc;ipp;xsd");

                if (fileTypesSet.Contains(ProjectDef.FileType.Resource))
                    AddFilterGroup(itemGroupElement, "Resource Files", "67DA6AB6-F800-4c08-8B7A-83BB121AAD01", "rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx;tiff;tif;png;wav;mfcribbon-ms");

                if (fileTypesSet.Contains(ProjectDef.FileType.Content))
                    AddFilterGroup(itemGroupElement, "Content", UpperGuid(StringToGuid("Content")), null);

                foreach (string bin in uniqueFileBins)
                {
                    AddFilterGroup(itemGroupElement, bin, UpperGuid(StringToGuid(@"#FilterPrefix#" + bin)), null);
                }

                projElement.AppendChild(itemGroupElement);
            }

            AddFiltersItemGroup(projElement, projectDir, resolvedFiles, fileBins, "ClCompile",
                bool (ProjectDef.FileType fileType) => (fileType == ProjectDef.FileType.Source));

            AddFiltersItemGroup(projElement, projectDir, resolvedFiles, fileBins, "ClInclude",
                bool (ProjectDef.FileType fileType) => (fileType == ProjectDef.FileType.Include));

            AddFiltersItemGroup(projElement, projectDir, resolvedFiles, fileBins, "None",
                bool (ProjectDef.FileType fileType) =>
                (fileType == ProjectDef.FileType.Content || fileType == ProjectDef.FileType.Misc)
                );
        }
        private static void AddFiltersItemGroup(XmlElement projElement, string projectDir, IReadOnlyList<ProjectResolver.ResolvedFile> resolvedFiles, IReadOnlyList<string> fileBins, string elementType, Func<ProjectDef.FileType, bool> filter)
        {
            if (resolvedFiles.Count != fileBins.Count)
                throw new ArgumentException();

            XmlDocument document = projElement.OwnerDocument;

            XmlElement? itemGroupElement = null;

            for (int i = 0; i < resolvedFiles.Count; i++)
            {
                ProjectResolver.ResolvedFile file = resolvedFiles[i];
                string fileBin = fileBins[i];

                if (!filter(file.FileType))
                    continue;

                if (itemGroupElement == null)
                    itemGroupElement = document.CreateElement("ItemGroup", _xmlNS);

                XmlElement itemElement = document.CreateElement(elementType, _xmlNS);
                itemElement.SetAttribute("Include", ComputeRelativePath(projectDir, file.FilePath));

                if (!string.IsNullOrEmpty(fileBin))
                {
                    XmlElement filterElement = document.CreateElement("Filter", _xmlNS);
                    filterElement.AppendChild(document.CreateTextNode(fileBin));

                    itemElement.AppendChild(filterElement);
                }

                itemGroupElement.AppendChild(itemElement);
            }

            if (itemGroupElement != null)
                projElement.AppendChild(itemGroupElement);
        }

        private static void AddBin(string fullBin, List<string> fileBins, HashSet<string> fileBinsSet, HashSet<string> reservedBinsSet)
        {
            if (fullBin.Length > 0)
            {
                string binKey = fullBin.ToUpperInvariant();

                if (!reservedBinsSet.Contains(binKey) && !fileBinsSet.Contains(binKey))
                {
                    for (int i = fullBin.Length; i > 0; i--)
                    {
                        char ch = fullBin[i - 1];

                        if (ch == Path.DirectorySeparatorChar)
                        {
                            AddBin(fullBin.Substring(0, i - 1), fileBins, fileBinsSet, reservedBinsSet);
                            break;
                        }
                    }

                    fileBinsSet.Add(binKey);
                    fileBins.Add(fullBin);
                }
            }
        }

        private void AddProjectRefs(XmlElement projElement, string projectDir, ProjectDef projDef, ProjectResolver resolver, InternalState state)
        {
            XmlDocument document = projElement.OwnerDocument;

            IReadOnlyList<string> projectRefs = projDef.Refs;

            if (projectRefs.Count == 0)
                return;

            List<string> unconditionalRefs = new List<string>();
            List<string> conditionalRefs = new List<string>();

            foreach (string projRef in projDef.Refs)
            {
                ProjectDef otherDef = state.ProjectDefs.Defs[projRef];
                if (otherDef.ProjectType == ProjectDef.Type.Module)
                    conditionalRefs.Add(projRef);
                else
                    unconditionalRefs.Add(projRef);
            }

            if (unconditionalRefs.Count > 0)
            {
                List<string> filteredDefs = new List<string>();

                foreach (string projRef in unconditionalRefs)
                {
                    ProjectDef.Type type = state.ProjectDefs.Defs[projRef].ProjectType;

                    if (IsValidReference(type))
                        filteredDefs.Add(projRef);
                }

                XmlElement itemGroupElement = document.CreateElement("ItemGroup", _xmlNS);

                AddProjectRefsList(itemGroupElement, projectDir, filteredDefs, state);

                projElement.AppendChild(itemGroupElement);
            }

            foreach (TargetConfiguration config in state.TargetDefs.Configurations)
            {
                List<string> filteredDefs = new List<string>();

                foreach (string projRef in conditionalRefs)
                {
                    ProjectDef.Type type = ResolveProjectType(config, state.ProjectDefs.Defs[projRef].ProjectType);

                    if (IsValidReference(type))
                        filteredDefs.Add(projRef);
                }

                if (filteredDefs.Count > 0)
                {
                    XmlElement itemGroupElement = document.CreateElement("ItemGroup", _xmlNS);
                    itemGroupElement.SetAttribute("Condition", $"'$(Configuration)'=='{config.Name}'");

                    AddProjectRefsList(itemGroupElement, projectDir, filteredDefs, state);

                    projElement.AppendChild(itemGroupElement);
                }
            }
        }

        private static bool IsValidReference(ProjectDef.Type type)
        {
            switch (type)
            {
                case ProjectDef.Type.Lib:
                case ProjectDef.Type.LinkedModule:
                case ProjectDef.Type.LinkedDll:
                case ProjectDef.Type.AlwaysLinkedDll:
                    return true;
                default:
                    return false;
            }    
        }

        private void AddProjectRefsList(XmlElement itemGroupElement, string projectDir, List<string> refs, InternalState state)
        {
            XmlDocument xmlDocument = itemGroupElement.OwnerDocument;

            foreach (string refName in refs)
            {
                string projectPath = Path.Combine(refName, Path.GetFileName(refName) + ".vcxproj");
                string relPath = ComputeRelativePath(projectDir, projectPath);

                XmlElement projectRefElement = xmlDocument.CreateElement("ProjectReference", _xmlNS);
                projectRefElement.SetAttribute("Include", relPath);

                XmlElement projectElement = xmlDocument.CreateElement("Project", _xmlNS);
                projectElement.AppendChild(xmlDocument.CreateTextNode("{" + state.ProjectGuids[refName].ToString() + "}"));

                projectRefElement.AppendChild(projectElement);

                itemGroupElement.AppendChild(projectRefElement);
            }
        }

        private XmlElement ConvertFileListToElement(XmlDocument document, string directive, List<string> fileList)
        {
            fileList.Sort(int (string a, string b) =>
            {
                return string.Compare(a, b, StringComparison.OrdinalIgnoreCase);
            });

            XmlElement itemGroupElement = document.CreateElement("ItemGroup", _xmlNS);

            foreach (string file in fileList)
            {
                XmlElement clCompileElem = document.CreateElement(directive, _xmlNS);
                clCompileElem.SetAttribute("Include", file);

                itemGroupElement.AppendChild(clCompileElem);
            }

            return itemGroupElement;
        }

        private static void AddFilesWithFilter(XmlElement projElement, string projectDir, IEnumerable<ProjectResolver.ResolvedFile> files,
            Func<ProjectResolver.ResolvedFile, bool> includeFilter,
            Action<XmlElement, ProjectResolver.ResolvedFile> addAction)
        {
            XmlElement? itemGroupElement = null;

            foreach (ProjectResolver.ResolvedFile file in files)
            {
                if (includeFilter(file))
                {
                    if (itemGroupElement == null)
                        itemGroupElement = projElement.OwnerDocument.CreateElement("ItemGroup", _xmlNS);

                    addAction(itemGroupElement, file);
                }
            }

            if (itemGroupElement != null)
                projElement.AppendChild(itemGroupElement);
        }

        private void AddFiles(XmlElement projElement, string projRoot, string projectDir, ProjectResolver resolver)
        {
            XmlDocument document = projElement.OwnerDocument;

            AddFilesWithFilter(projElement, projectDir, resolver.ResolvedFiles,
                bool (ProjectResolver.ResolvedFile file) => (file.FileType == ProjectDef.FileType.Source),
                void (XmlElement itemGroup, ProjectResolver.ResolvedFile file) =>
                {
                    XmlElement itemElement = itemGroup.OwnerDocument.CreateElement("ClCompile", _xmlNS);
                    itemElement.SetAttribute("Include", ComputeRelativePath(projectDir, file.FilePath));

                    itemGroup.AppendChild(itemElement);
                });

            AddFilesWithFilter(projElement, projectDir, resolver.ResolvedFiles,
                bool (ProjectResolver.ResolvedFile file) => (file.FileType == ProjectDef.FileType.Include),
                void (XmlElement itemGroup, ProjectResolver.ResolvedFile file) =>
                {
                    XmlElement itemElement = itemGroup.OwnerDocument.CreateElement("ClInclude", _xmlNS);
                    itemElement.SetAttribute("Include", ComputeRelativePath(projectDir, file.FilePath));

                    itemGroup.AppendChild(itemElement);
                });

            AddFilesWithFilter(projElement, projectDir, resolver.ResolvedFiles,
                bool (ProjectResolver.ResolvedFile file) =>
                    (file.FileType == ProjectDef.FileType.Content || file.FileType == ProjectDef.FileType.Misc),
                void (XmlElement itemGroup, ProjectResolver.ResolvedFile file) =>
                {
                    XmlDocument document = itemGroup.OwnerDocument;

                    XmlElement itemElement = document.CreateElement("None", _xmlNS);
                    itemElement.SetAttribute("Include", ComputeRelativePath(projectDir, file.FilePath));

                    if (file.FileType == ProjectDef.FileType.Content)
                    {
                        XmlElement deploymentContentElement = document.CreateElement("DeploymentContent", _xmlNS);
                        deploymentContentElement.AppendChild(document.CreateTextNode("true"));

                        itemElement.AppendChild(deploymentContentElement);
                    }

                    itemGroup.AppendChild(itemElement);
                });
        }

        private void AddPropertySheetImports(XmlElement projElement, string projectDir, ProjectDef projectDef, TargetDefs targetDefs)
        {
            XmlDocument document = projElement.OwnerDocument;

            foreach (TargetPlatform platform in targetDefs.Platforms)
            {
                foreach (TargetConfiguration config in targetDefs.Configurations)
                {
                    List<string> propertySheets = new List<string>();
                    propertySheets.AddRange(CollectConfigPropertySheets(projectDir, config));
                    propertySheets.AddRange(projectDef.Configs);

                    propertySheets.Sort(int (string a, string b) => string.Compare(a, b, StringComparison.OrdinalIgnoreCase));

                    XmlElement importGroupElement = document.CreateElement("ImportGroup", _xmlNS);
                    importGroupElement.SetAttribute("Label", "PropertySheets");
                    importGroupElement.SetAttribute("Condition", $"'$(Configuration)|$(Platform)'=='{config.Name}|{platform.ArchName}'");

                    {
                        XmlElement importElement = document.CreateElement("Import", _xmlNS);
                        importElement.SetAttribute("Project", @"$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props");
                        importElement.SetAttribute("Condition", @"exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')");
                        importElement.SetAttribute("Label", "LocalAppDataPlatform");

                        importGroupElement.AppendChild(importElement);
                    }

                    foreach (string propertySheet in propertySheets)
                    {
                        XmlElement importElement = importGroupElement.OwnerDocument.CreateElement("Import", _xmlNS);
                        importElement.SetAttribute("Project", ComputeRelativePath(projectDir, propertySheet + ".props"));

                        importGroupElement.AppendChild(importElement);
                    }

                    projElement.AppendChild(importGroupElement);
                }
            }
        }

        private static void RecursiveCollectConfigPropertySheets(List<string> imports, string projectDir, TargetConfiguration config)
        {
            foreach (TargetConfiguration inheritedConfig in config.Inherit)
                RecursiveCollectConfigPropertySheets(imports, projectDir, inheritedConfig);

            imports.Add(config.Name);
        }

        private static IEnumerable<string> CollectConfigPropertySheets(string projectDir, TargetConfiguration config)
        {
            List<string> imports = new List<string>();

            RecursiveCollectConfigPropertySheets(imports, projectDir, config);

            return imports;
        }

        internal static System.Guid BytesToGuid(byte[] bytes)
        {
            byte[] shaHash = System.Security.Cryptography.SHA256.HashData(bytes);

            uint guidA = (uint)((shaHash[0] << 24)
                | (shaHash[1] << 16)
                | (shaHash[2] << 8)
                | (shaHash[3] << 0));

            ushort guidB = (ushort)((shaHash[4] << 8)
                | (shaHash[5] << 0));

            ushort guidC = (ushort)((shaHash[6] << 8)
                | (shaHash[7] << 0));

            return new System.Guid(guidA, guidB, guidC,
                shaHash[8], shaHash[9], shaHash[10], shaHash[11], shaHash[12], shaHash[13], shaHash[14], shaHash[15]);
        }

        internal static System.Guid StringToGuid(string str)
        {
            return BytesToGuid(Encoding.UTF8.GetBytes(str));
        }

        internal void Generate(GlobalConfiguration gconfig, ProjectDefs projDefs, IReadOnlyDictionary<ProjectDef, ProjectResolver> resolvers, TargetDefs targetDefs, OutputFileCollection outputFiles)
        {
            List<KeyValuePair<string, ProjectDef>> sortedDefs = projDefs.Defs.ToList();

            InternalState internalState = new InternalState(projDefs, gconfig, resolvers, targetDefs);

            sortedDefs.Sort(int (KeyValuePair<string, ProjectDef> a, KeyValuePair<string, ProjectDef> b) =>
            {
                return string.Compare(a.Key, b.Key, StringComparison.Ordinal);
            });

            foreach (KeyValuePair<string, ProjectDef> namedDef in sortedDefs)
            {
                internalState.ProjectGuids[namedDef.Key] = StringToGuid(namedDef.Key);
            }

            foreach (KeyValuePair<string, ProjectDef> namedDef in sortedDefs)
            {
                GenerateProject(gconfig, namedDef.Key, namedDef.Value, resolvers[namedDef.Value], targetDefs, outputFiles, internalState);
            }

            GenerateSolution(gconfig, sortedDefs, resolvers, outputFiles, internalState);

            foreach (string filePath in outputFiles.Files.Keys)
            {
                string fullPath = Path.Combine(gconfig.RootPath!, filePath);

                byte[] newContents = outputFiles.Files[filePath].ToArray();

                bool ignore = false;
                if (File.Exists(fullPath))
                {
                    byte[] existingContents = File.ReadAllBytes(fullPath);

                    ignore = (new ReadOnlySpan<byte>(existingContents)).SequenceEqual(new ReadOnlySpan<byte>(newContents));
                }

                if (!ignore)
                    File.WriteAllBytes(fullPath, newContents);
            }
        }

        private static string UpperGuid(System.Guid guid)
        {
            return guid.ToString().ToUpperInvariant();
        }

        private void GenerateSolution(GlobalConfiguration gconfig, List<KeyValuePair<string, ProjectDef>> sortedDefs, IReadOnlyDictionary<ProjectDef, ProjectResolver> resolvers, OutputFileCollection outputFiles, InternalState state)
        {
            Stream outStream = outputFiles.Open(gconfig.SolutionName + ".sln");
            System.Guid solutionGuid = StringToGuid(gconfig.SolutionName!);

            using (TextWriter tw = new StreamWriter(outStream, Encoding.UTF8))
            {
                tw.WriteLine();
                tw.WriteLine("Microsoft Visual Studio Solution File, Format Version 12.00");
                tw.WriteLine("# Visual Studio Version 17");
                tw.WriteLine("VisualStudioVersion = 17.9.34607.119");
                tw.WriteLine("MinimumVisualStudioVersion = 10.0.40219.1");

                foreach (KeyValuePair<string, ProjectDef> projNameAndDef in sortedDefs)
                {
                    string projName = projNameAndDef.Key;
                    ProjectDef projDef = projNameAndDef.Value;

                    tw.Write("Project(\"{" + _vcppGuidStr + "}\") = \"");
                    tw.Write(Path.GetFileName(projName));
                    tw.Write("\", \"");
                    tw.Write(projName + "\\" + Path.GetFileName(projName) + ".vcxproj");
                    tw.Write("\", \"{");
                    tw.Write(UpperGuid(state.ProjectGuids[projName]));
                    tw.WriteLine("}\"");

                    List<string> filteredRefs = new List<string>();

                    foreach (string projRef in projDef.Refs)
                    {
                        switch (state.ProjectDefs.Defs[projRef].ProjectType)
                        {
                            case ProjectDef.Type.Module:
                            case ProjectDef.Type.Executable:
                            case ProjectDef.Type.NeverLinkedDll:
                                filteredRefs.Add(projRef);
                                break;
                            default:
                                break;
                        }
                    }

                    if (filteredRefs.Count > 0)
                    {
                        tw.WriteLine("\tProjectSection(ProjectDependencies) = postProject");
                        foreach (string projRef in filteredRefs)
                        {
                            string upperGuid = UpperGuid(state.ProjectGuids[projRef]);

                            tw.Write("\t\t{");
                            tw.Write(upperGuid);
                            tw.Write("} = {");
                            tw.Write(upperGuid);
                            tw.WriteLine("}");
                        }
                        tw.WriteLine("\tEndProjectSection");
                    }
                    tw.WriteLine("EndProject");
                }

                tw.Write("Project(\"{");
                tw.Write(_slnFolderGuidStr);
                tw.Write("}\") = \"Solution Items\", \"Solution Items\", \"{");
                tw.Write(_slnFolderImplGuidStr);
                tw.WriteLine("}\"");
                tw.WriteLine("\tProjectSection(SolutionItems) = preProject");
                tw.WriteLine("\t\trkit.natvis = rkit.natvis");
                tw.WriteLine("\tEndProjectSection");
                tw.WriteLine("EndProject");

                tw.WriteLine("Global");
                tw.WriteLine("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution");

                foreach (TargetConfiguration config in state.TargetDefs.Configurations)
                {
                    foreach (TargetPlatform platform in state.TargetDefs.Platforms)
                    {
                        tw.Write("\t\t");
                        tw.Write(config.Name);
                        tw.Write("|");
                        tw.Write(platform.SolutionPlatformName);
                        tw.Write(" = ");
                        tw.Write(config.Name);
                        tw.Write("|");
                        tw.WriteLine(platform.SolutionPlatformName);
                    }
                }
                tw.WriteLine("\tEndGlobalSection");
                tw.WriteLine("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution");
                foreach (KeyValuePair<string, ProjectDef> projNameAndDef in sortedDefs)
                {
                    ProjectDef projDef = projNameAndDef.Value;
                    string projGuidStr = UpperGuid(state.ProjectGuids[projNameAndDef.Key]);

                    foreach (TargetConfiguration config in state.TargetDefs.Configurations)
                    {
                        foreach (TargetPlatform platform in state.TargetDefs.Platforms)
                        {
                            tw.Write("\t\t{");
                            tw.Write(projGuidStr);
                            tw.Write("}.");
                            tw.Write(config.Name);
                            tw.Write("|");
                            tw.Write(platform.SolutionPlatformName);
                            tw.Write(".ActiveCfg = ");
                            tw.Write(config.Name);
                            tw.Write("|");
                            tw.WriteLine(platform.ArchName);

                            tw.Write("\t\t{");
                            tw.Write(projGuidStr);
                            tw.Write("}.");
                            tw.Write(config.Name);
                            tw.Write("|");
                            tw.Write(platform.SolutionPlatformName);
                            tw.Write(".Build.0 = ");
                            tw.Write(config.Name);
                            tw.Write("|");
                            tw.WriteLine(platform.ArchName);
                        }
                    }
                }
                tw.WriteLine("\tEndGlobalSection");
                tw.WriteLine("\tGlobalSection(SolutionProperties) = preSolution");
                tw.WriteLine("\t\tHideSolutionNode = FALSE");
                tw.WriteLine("\tEndGlobalSection");
                tw.WriteLine("\tGlobalSection(ExtensibilityGlobals) = postSolution");
                tw.Write("\t\tHideSolutionNode = {");
                tw.Write(UpperGuid(solutionGuid));
                tw.WriteLine("}");
                tw.WriteLine("\tEndGlobalSection");
                tw.WriteLine("EndGlobal");
            }
        }
    }
}
