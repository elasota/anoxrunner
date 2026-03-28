


namespace GameObjectReflector
{
    internal class ClassDef2
    {
        public ClassDef2(string name, ClassType classType, TypeDefAttribute[] typeDefAttributes, List<string> parentClasses, FieldDef[] fieldDefs)
        {
            Name = name;
            ClassType = classType;
            TypeDefAttributes = typeDefAttributes;
            ParentClasses = parentClasses;
            FieldDefs = fieldDefs;
        }

        public string Name { get; }
        public ClassType ClassType { get; }
        public TypeDefAttribute[] TypeDefAttributes { get; }
        public List<string> ParentClasses { get; }
        public FieldDef[] FieldDefs { get; }

        internal T GetAttributeOfType<T>()
            where T : TypeDefAttribute
        {
            T? attrib = TryGetAttributeOfType<T>();
            if (attrib == null)
                throw new ArgumentException();

            return attrib!;
        }

        internal T TryGetAttributeOfType<T>()
            where T : TypeDefAttribute
        {
            foreach (TypeDefAttribute attrib in TypeDefAttributes)
            {
                T? recastAttrib = (attrib as T);
                if (recastAttrib != null)
                    return recastAttrib;
            }

            return null;
        }
    }
}