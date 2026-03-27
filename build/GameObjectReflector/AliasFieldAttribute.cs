namespace GameObjectReflector
{
    internal class AliasFieldAttribute : FieldAttribute
    {
        public AliasFieldAttribute(string aliasName)
        {
            AliasName = aliasName;
        }

        public string AliasName { get; }
    }
}