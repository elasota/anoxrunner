
namespace GameObjectReflector
{
    internal class ClassDef2
    {
        public ClassDef2(ClassType classType, TypeDefAttribute[] typeDefAttributes, List<string> parentClasses, FieldDef[] fieldDefs)
        {
            ClassType = classType;
            TypeDefAttributes = typeDefAttributes;
            ParentClasses = parentClasses;
            FieldDefs = fieldDefs;
        }

        public ClassType ClassType { get; }
        public TypeDefAttribute[] TypeDefAttributes { get; }
        public List<string> ParentClasses { get; }
        public FieldDef[] FieldDefs { get; }
    }
}