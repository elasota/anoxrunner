namespace GameObjectReflector
{
    internal class LevelTypeDefAttribute : TypeDefAttribute
    {
        public LevelTypeDefAttribute(string nameInLevel)
        {
            NameInLevel = nameInLevel;
        }

        public string NameInLevel { get; }
    }
}