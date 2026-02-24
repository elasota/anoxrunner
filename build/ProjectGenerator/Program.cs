namespace ProjectGenerator
{
    internal class Program
    {
        static void Main(string[] args)
        {
            if (args.Length == 1)
                throw new ArgumentException("Usage: ProjectGenerator <root> [options]");

            GlobalConfiguration config = new GlobalConfiguration();

            config.RootPath = args[0];

            for (int i = 1; i < args.Length; i++)
            {
                string arg = args[i];

                if (arg == "-def")
                {
                    i++;
                    if (i == args.Length)
                        throw new ArgumentException("-defs not followed by defs path");

                    config.DefsPath = args[i];
                }

                if (arg == "-sln")
                {
                    i++;
                    if (i == args.Length)
                        throw new ArgumentException("-sln not followed by solution name");

                    config.SolutionName = args[i];
                }
            }

            if (config.DefsPath == null)
                throw new Exception("No defs path specified");

            if (config.SolutionName == null)
                throw new Exception("No solution name specified");

            ProjectDefs projDefs = new ProjectDefs(Path.Combine(config.DefsPath, "ProjectDefs.json"));

            TargetDefs targetDefs = new TargetDefs();
            targetDefs.Platforms.Add(new TargetPlatform("x86", "Win32"));
            targetDefs.Platforms.Add(new TargetPlatform("x64", "x64"));

            TargetConfiguration defaultConfig = new TargetConfiguration("Common");
            defaultConfig.Configs.Add("Common");
            defaultConfig.PlatformToolset = "v143";

            {
                TargetConfiguration debugConfig = new TargetConfiguration("Debug");
                debugConfig.Inherit.Add(defaultConfig);
                debugConfig.Configs.Add("Debug");
                debugConfig.ModuleProjectType = ProjectDef.Type.LooseDll;
                debugConfig.LinkedModuleProjectType = ProjectDef.Type.LinkedDll;
                debugConfig.UseDebugLibraries = true;
                debugConfig.IsDevConfig = true;
                targetDefs.Configurations.Add(debugConfig);
            }

            {
                TargetConfiguration releaseConfig = new TargetConfiguration("Release");
                releaseConfig.Inherit.Add(defaultConfig);
                releaseConfig.Configs.Add("Release");
                releaseConfig.ModuleProjectType = ProjectDef.Type.Lib;
                releaseConfig.LinkedModuleProjectType = ProjectDef.Type.Lib;
                targetDefs.Configurations.Add(releaseConfig);
            }

            Dictionary<ProjectDef, ProjectResolver> resolvers = new Dictionary<ProjectDef, ProjectResolver>();

            foreach (KeyValuePair<string, ProjectDef> projectDef in projDefs.Defs)
            {
                resolvers[projectDef.Value] = new ProjectResolver(projectDef.Key, projectDef.Value, projectDef.Key, config);
            }

            OutputFileCollection outputFiles = new OutputFileCollection();

            VSProjectGenerator projectGenerator = new VSProjectGenerator();
            projectGenerator.Generate(config, projDefs, resolvers, targetDefs, outputFiles);

            ModuleListGenerator moduleListgenerator = new ModuleListGenerator();
            moduleListgenerator.Generate(config, projDefs, resolvers, targetDefs, outputFiles);
        }
    }
}
