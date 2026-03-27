
namespace GameObjectReflector
{
    internal class EntityClassCollection
    {
        private List<ClassDef2> _classes = new List<ClassDef2>();
        private List<ClassDef2> _components = new List<ClassDef2>();

        internal void AddClass(ClassDef2 value)
        {
            _classes.Add(value);
        }

        internal void AddComponent(ClassDef2 value)
        {
            _components.Add(value);
        }
    }
}