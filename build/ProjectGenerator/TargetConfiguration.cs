
namespace ProjectGenerator
{
    internal class TargetConfiguration
    {
        public string Name { get; private set; }
        public List<string> Configs { get; } = new List<string>();
        public List<TargetConfiguration> Inherit { get; } = new List<TargetConfiguration>();
        public ProjectDef.Type? ModuleProjectType { get; set; }
        public ProjectDef.Type? LinkedModuleProjectType { get; set; }
        public string? PlatformToolset { get; set; }
        public bool? UseDebugLibraries { get; set; }
        public bool IsDevConfig { get; set; }

        public TargetConfiguration(string name)
        {
            Name = name;
        }
    }
}
