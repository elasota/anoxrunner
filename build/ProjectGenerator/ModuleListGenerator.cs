using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace ProjectGenerator
{
    internal class ModuleListGenerator
    {
        private struct TaintAwareModuleRef
        {
            public bool IsDevOnly;
            public string DepName;
        }

        internal void Generate(GlobalConfiguration config, ProjectDefs projDefs, Dictionary<ProjectDef, ProjectResolver> resolvers, TargetDefs targetDefs, OutputFileCollection outputFiles)
        {
            foreach (KeyValuePair<string, ProjectDef> projDefKVP in projDefs.Defs)
            {
                string projName = projDefKVP.Key;
                ProjectDef projDef = projDefKVP.Value;
                if (projDefKVP.Value.ProjectType != ProjectDef.Type.Executable)
                    continue;

                Dictionary<string, bool> depIsForRelease = new Dictionary<string, bool>();
                Queue<TaintAwareModuleRef> todoItems = new Queue<TaintAwareModuleRef>();

                foreach (string projRef in projDef.Refs)
                    todoItems.Enqueue(new TaintAwareModuleRef { DepName = projRef, IsDevOnly = projDef.DevOnly });

                Dictionary<string, bool> modulesToExport = new Dictionary<string, bool>();

                HashSet<string> modulesSet = new HashSet<string>();

                {
                    TaintAwareModuleRef thisDep;
                    while (todoItems.TryDequeue(out thisDep))
                    {
                        bool existingDepIsForRelease = false;
                        if (depIsForRelease.TryGetValue(thisDep.DepName, out existingDepIsForRelease))
                        {
                            // If the existing dep is for release, then it's not being promoted
                            // If the existing dep is not-for-release and this dep is dev-only, it's also not being promoted
                            if (existingDepIsForRelease || existingDepIsForRelease != thisDep.IsDevOnly)
                                continue;
                        }

                        ProjectDef dependencyProjDef = projDefs.Defs[thisDep.DepName];

                        bool isDevOnly = thisDep.IsDevOnly || dependencyProjDef.DevOnly;

                        depIsForRelease[thisDep.DepName] = !isDevOnly;

                        if (dependencyProjDef.ProjectType == ProjectDef.Type.Module)
                            modulesSet.Add(thisDep.DepName);

                        switch (dependencyProjDef.ProjectType)
                        {
                            case ProjectDef.Type.Module:
                            case ProjectDef.Type.LinkedModule:
                                foreach (string depRef in dependencyProjDef.Refs)
                                    todoItems.Enqueue(new TaintAwareModuleRef { DepName = depRef, IsDevOnly = isDevOnly });
                                break;
                            default:
                                break;
                        }
                    }
                }

                List<string> releaseModules = new List<string>();
                List<string> devModules = new List<string>();

                foreach (string moduleName in modulesSet)
                {
                    if (depIsForRelease[moduleName])
                        releaseModules.Add(moduleName);
                    else
                        devModules.Add(moduleName);
                }

                releaseModules.Sort();
                devModules.Sort();

                string outputPath = Path.Combine(config.RootPath!, projName, projName + ".generated.cpp");

                Func<string, string> protoLine = string (string moduleName) =>
                {
                    return "\textern const ModuleInfo g_module_" + moduleName + ";";
                };

                Func<string, string> linkerLine = string (string moduleName) =>
                {
                    return "\t\t&g_module_" + moduleName + ",";
                };

                using (StreamWriter sw = new StreamWriter(outputPath, false, Encoding.ASCII))
                {
                    sw.WriteLine("#include \"rkit/Core/ModuleGlue.h\"");
                    sw.WriteLine();
                    sw.WriteLine("#if RKIT_MODULE_LINKER_TYPE == RKIT_MODULE_LINKER_TYPE_STATIC");
                    sw.WriteLine();
                    sw.WriteLine("namespace rkit::moduleloader");
                    sw.WriteLine("{");
                    sw.WriteLine();

                    sw.WriteLine("#if RKIT_IS_DEBUG != 0");
                    foreach (string depName in devModules)
                        sw.WriteLine(protoLine(depName));

                    sw.WriteLine("#endif");

                    foreach (string depName in releaseModules)
                        sw.WriteLine(protoLine(depName));

                    sw.WriteLine();
                    sw.WriteLine("\tstatic const ModuleInfo *g_moduleInfo[] =");
                    sw.WriteLine("\t{");
                    sw.WriteLine("#if RKIT_IS_DEBUG != 0");

                    foreach (string depName in devModules)
                        sw.WriteLine(linkerLine(depName));

                    sw.WriteLine("#endif");

                    foreach (string depName in releaseModules)
                        sw.WriteLine(linkerLine(depName));

                    sw.WriteLine("\t};");
                    sw.WriteLine();
                    sw.WriteLine("\tconst ModuleList g_moduleList = { g_moduleInfo, sizeof(g_moduleInfo) / sizeof(g_moduleInfo[0]) };");
                    sw.WriteLine("}");
                    sw.WriteLine();
                    sw.WriteLine("#endif");
                }
            }
        }
    }
}
