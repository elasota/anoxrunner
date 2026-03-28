
namespace GameObjectReflector
{
    internal class EntityClassCollection
    {
        private Dictionary<string, ClassDef2> _classesByName = new Dictionary<string, ClassDef2>();

        public IReadOnlyDictionary<string, ClassDef2> Classes { get =>_classesByName; }

        internal void AddClass(ClassDef2 value)
        {
            if (!_classesByName.TryAdd(value.Name, value))
                throw new ReflectorException("Class " + value.Name + " is already defined");
        }
    }
}