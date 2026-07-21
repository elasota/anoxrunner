namespace GameObjectReflector
{
    internal struct LevelFieldInfo
    {
        public FieldMainType MainType { get; }
        public int Offset { get; }
        public bool IsOnOffBool { get; }

        public LevelFieldInfo(FieldMainType fieldType, int offset)
        {
            MainType = fieldType;
            Offset = offset;
            IsOnOffBool = false;
        }

        public LevelFieldInfo(FieldMainType fieldType, int offset, bool isOnOffBool)
        {
            MainType = fieldType;
            Offset = offset;
            IsOnOffBool = isOnOffBool;
        }
    }
}