namespace BuildToolsCommon
{
    public class DeduplicatedList<T>
        where T : IEquatable<T>
    {
        private Dictionary<T, int> _dict = new Dictionary<T, int>();

        public int Count { get { return _dict.Count; } }

        public int Add(T item)
        {
            int index = 0;
            if (!_dict.TryGetValue(item, out index))
            {
                index = _dict.Count;
                _dict.Add(item, index);
            }

            return index;
        }

        public T[] Unroll()
        {
            T[] array = new T[_dict.Count];

            foreach (KeyValuePair<T, int> kvp in _dict)
                array[kvp.Value] = kvp.Key;

            return array;
        }
    }
}
