namespace GameObjectReflector
{
    internal class LevelFieldAttribute : FieldAttribute
    {
        public string[]? ScalarizedFields { get; internal set; } = null;
        public string? OverrideName { get; internal set; } = null;
        public bool IsOnOff { get; internal set; }
    }
}