namespace BuildToolsCommon
{
    public class DeduplicatedDict<TKey, TValue>
        where TKey : IEquatable<TKey>
    {
        private Dictionary<TKey, int> _dict = new Dictionary<TKey, int>();
        private List<TValue> _values = new List<TValue>();

        public bool TryAdd(TKey key, TValue value, out int index)
        {
            int tempIndex = _dict.Count;
            if (_dict.TryAdd(key, tempIndex))
            {
                _values[tempIndex] = value;
                index = tempIndex;
                return true;
            }

            index = 0;
            return false;
        }

        public int Add(TKey key, TValue value)
        {
            int index = 0;
            if (_dict.TryGetValue(key, out index))
                _values[index] = value;
            else
            {
                index = _dict.Count;

                _dict.Add(key, index);
                _values.Add(value);
            }

            return index;
        }

        public KeyValuePair<TKey, TValue>[] Unroll()
        {
            KeyValuePair<TKey, TValue>[] array = new KeyValuePair<TKey, TValue>[_dict.Count];

            foreach (KeyValuePair<TKey, int> kvp in _dict)
                array[kvp.Value] = new KeyValuePair<TKey, TValue>(kvp.Key, _values[kvp.Value]);

            return array;
        }

        public bool TryGetIndex(TKey key, out int index)
        {
            return _dict.TryGetValue(key, out index);
        }

        public int GetIndex(TKey key)
        {
            return _dict[key];
        }

        public bool TryGetValue(TKey key, out TValue value)
        {
            int index = 0;
            if (_dict.TryGetValue(key, out index))
            {
                value = _values[index];
                return true;
            }

            value = default;
            return false;
        }
    }
}
