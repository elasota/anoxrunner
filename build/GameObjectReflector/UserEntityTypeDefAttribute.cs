namespace GameObjectReflector
{
    internal class UserEntityTypeDefAttribute : TypeDefAttribute
    {
        public UserEntityTypeDefAttribute(string edefName)
        {
            EdefName = edefName;
        }

        public string EdefName { get; }
    }
}