using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ProjectGenerator
{
    internal class ProjectResolver
    {
        private static readonly string[] _headerExts = { ".h", ".hxx", ".inl" };
        private static readonly string[] _sourceExts = { ".c", ".cxx", ".cpp" };

        public IReadOnlyList<ResolvedFile> ResolvedFiles { get; private set; }
        public ProjectDef ProjectDef { get; private set; }

        public struct ResolvedFile
        {
            public string FilePath { get; internal set; }
            public string FilterPath { get; internal set; }
            public ProjectDef.FileType FileType { get; internal set; }
        }

        private static void RecursiveExpandExtraFile(List<ResolvedFile> resolvedFiles, string basePath, string suffixPath, string projectDir, string rootPath, ProjectDef.FileType fileType)
        {
            for (int i = 0; i < suffixPath.Length; i++)
            {
                char c = suffixPath[i];
                if (c == Path.DirectorySeparatorChar || c == Path.AltDirectorySeparatorChar)
                {
                    RecursiveExpandExtraFileDirectory(resolvedFiles, basePath, suffixPath.Substring(0, i), suffixPath.Substring(i + 1), projectDir, rootPath, fileType);
                    return;
                }
            }

            RecursiveExpandExtraFileFile(resolvedFiles, basePath, suffixPath, projectDir, rootPath, fileType);
        }

        private static void RecursiveExpandExtraFileFile(List<ResolvedFile> resolvedFiles, string basePath, string fileWildcard, string projectDir, string rootPath, ProjectDef.FileType fileType)
        {
            string? regEx = ConvertWildcardToRegEx(fileWildcard);

            if (regEx == null)
            {
                ExpandSingleFile(resolvedFiles, Path.Combine(basePath, fileWildcard), projectDir, rootPath, fileType);
                return;
            }

            throw new NotImplementedException();
        }

        private static void ExpandSingleFile(List<ResolvedFile> resolvedFiles, string filePath, string projectDir, string rootPath, ProjectDef.FileType fileType)
        {
            string absPath = Path.Combine(rootPath, filePath);

            if (!File.Exists(absPath))
                throw new Exception($"File '{absPath}' wasn't found");

            if (fileType == ProjectDef.FileType.Auto)
                fileType = ResolveFileTypeForFileName(Path.GetFileName(filePath));

            resolvedFiles.Add(new ResolvedFile
            {
                FilePath = filePath,
                FilterPath = Path.GetFileName(filePath),
                FileType = fileType
            });
        }

        private static string? ConvertWildcardToRegEx(string component)
        {
            if (!component.Contains('*') && !component.Contains('?'))
                return null;

            string escapedCharacters = @"\[]$^.()+{}|";

            StringBuilder sb = new StringBuilder();

            sb.Append('^');

            foreach (char c in component)
            {
                if (escapedCharacters.Contains(c))
                {
                    sb.Append('\\');
                    sb.Append(c);
                }
                else if (c == '*')
                    sb.Append(".*");
                else if (c == '?')
                    sb.Append(".");
            }
            sb.Append('$');

            return sb.ToString();
        }

        private static void RecursiveExpandExtraFileDirectory(List<ResolvedFile> resolvedFiles, string basePath, string dirName, string continuation, string projectDir, string rootPath, ProjectDef.FileType fileType)
        {
            string combinedDirectoryPath = Path.Combine(basePath, dirName);

            string? regEx = ConvertWildcardToRegEx(dirName);

            if (regEx == null)
            {
                RecursiveExpandExtraFile(resolvedFiles, Path.Combine(basePath, dirName), continuation, projectDir, rootPath, fileType);
                return;
            }

            throw new NotImplementedException();
        }

        private static void ExpandExtraFile(List<ResolvedFile> resolvedFiles, ProjectDef.ExtraFile extraFileBase, string projectDir, string rootPath)
        {
            RecursiveExpandExtraFile(resolvedFiles, "", extraFileBase.FilePath, projectDir, rootPath, extraFileBase.FileType);
        }

        private static ProjectDef.FileType ResolveFileTypeForFileName(string fileName)
        {
            string ext = Path.GetExtension(fileName);

            foreach (string sourceExt in _sourceExts)
            {
                if (string.Compare(ext, sourceExt, StringComparison.OrdinalIgnoreCase) == 0)
                    return ProjectDef.FileType.Source;
            }

            foreach (string sourceExt in _headerExts)
            {
                if (string.Compare(ext, sourceExt, StringComparison.OrdinalIgnoreCase) == 0)
                    return ProjectDef.FileType.Include;
            }

            return ProjectDef.FileType.Misc;
        }

        public ProjectResolver(ProjectDef projDef, string projectDir, GlobalConfiguration gconfig)
        {
            List<ProjectDef.DirectoryMapping> mappings = new List<ProjectDef.DirectoryMapping>();
            mappings.Add(new ProjectDef.DirectoryMapping()
            {
                SourceDirectory = projectDir,
                FilterDirectory = ""
            });

            mappings.AddRange(projDef.DirectoryMappings);

            List<ResolvedFile> resolvedFiles = new List<ResolvedFile>();
            foreach (ProjectDef.ExtraFile file in projDef.ExtraFiles)
            {
                ExpandExtraFile(resolvedFiles, file, projectDir, gconfig.RootPath!);
            }

            {
                ProjectDef.DirectoryMapping projDirMapping = new ProjectDef.DirectoryMapping()
                {
                    SourceDirectory = projectDir,
                    FilterDirectory = ""
                };
                ExpandDirMapping(resolvedFiles, projDirMapping, gconfig.RootPath!);
            }

            foreach (ProjectDef.DirectoryMapping dirMapping in projDef.DirectoryMappings)
            {
                ExpandDirMapping(resolvedFiles, dirMapping, gconfig.RootPath!);
            }

            resolvedFiles.Sort(int (ResolvedFile a, ResolvedFile b) =>
                {
                    return string.Compare(a.FilePath, b.FilePath, StringComparison.OrdinalIgnoreCase);
                });

            this.ResolvedFiles = resolvedFiles;
            this.ProjectDef = projDef;
        }

        private static void ExpandDirMapping(List<ResolvedFile> resolvedFiles, ProjectDef.DirectoryMapping projDirMapping, string rootPath)
        {
            bool recursive = projDirMapping.Recursive ?? true;

            string normalizedSourceDir = projDirMapping.SourceDirectory.Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
            string normalizedFilterDir = projDirMapping.FilterDirectory.Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);

            RecursiveExpandDirMapping(resolvedFiles, normalizedSourceDir, normalizedFilterDir, rootPath, recursive);
        }

        private static void RecursiveExpandDirMappingDirectory(List<ResolvedFile> resolvedFiles, DirectoryInfo sourceDirInfo, string sourceDirectory, string filterDirectory, bool recursive)
        {
            foreach (FileInfo file in sourceDirInfo.GetFiles())
            {
                ExpandDirMappingFile(resolvedFiles, file, sourceDirectory, filterDirectory, file.Name);
            }

            if (recursive)
            {
                foreach (DirectoryInfo dir in sourceDirInfo.GetDirectories())
                {
                    RecursiveExpandDirMappingDirectory(resolvedFiles, dir, Path.Combine(sourceDirectory, dir.Name), Path.Combine(filterDirectory, dir.Name), true);
                }
            }
        }

        private static void ExpandDirMappingFile(List<ResolvedFile> resolvedFiles, FileInfo file, string sourceDirectory, string filterDirectory, string name)
        {
            string fileName = file.Name;
            string? ext = Path.GetExtension(fileName);

            ProjectDef.FileType? fileType = null;

            if (ext != null)
            {
                foreach (string headerExt in _headerExts)
                {
                    if (headerExt == ext)
                    {
                        fileType = ProjectDef.FileType.Include;
                        break;
                    }
                }

                if (fileType == null)
                {
                    foreach (string sourceExt in _sourceExts)
                    {
                        if (sourceExt == ext)
                        {
                            fileType = ProjectDef.FileType.Source;
                            break;
                        }
                    }
                }
            }

            if (fileType != null)
            {
                ResolvedFile resolvedFile = new ResolvedFile()
                {
                    FilePath = Path.Combine(sourceDirectory, fileName),
                    FilterPath = Path.Combine(filterDirectory, fileName),
                    FileType = fileType.Value,
                };

                resolvedFiles.Add(resolvedFile);
            }
        }

        private static void RecursiveExpandDirMapping(List<ResolvedFile> resolvedFiles, string sourceDirectory, string filterDirectory, string rootPath, bool recursive)
        {
            string absPath = Path.Combine(rootPath, sourceDirectory);

            RecursiveExpandDirMappingDirectory(resolvedFiles, new DirectoryInfo(absPath), sourceDirectory, filterDirectory, recursive);
        }
    }
}
