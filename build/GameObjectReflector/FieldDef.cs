
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

        internal T? TryGetAttributeOfType<T>()
            where T : FieldAttribute
        {
            foreach (FieldAttribute attrib in FieldAttributes)
            {
                T? recastAttrib = (attrib as T);
                if (recastAttrib != null)
                    return recastAttrib;
            }

            return null;
        }
    }
}