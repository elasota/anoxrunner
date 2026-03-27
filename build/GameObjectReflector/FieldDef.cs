namespace GameObjectReflector
{
    internal class FieldDef
    {
        public FieldDef(FieldAttribute[] fieldAttributes, string fieldName, FieldType fieldType)
        {
            FieldAttributes = fieldAttributes;
            FieldName = fieldName;
            FieldType = fieldType;
        }

        public FieldAttribute[] FieldAttributes { get; }
        public string FieldName { get; }
        public FieldType FieldType { get; }
    }
}