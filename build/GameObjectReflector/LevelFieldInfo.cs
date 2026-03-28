namespace GameObjectReflector
{
    internal struct LevelFieldInfo
    {
        public FieldType FieldType { get; }
        public int Offset { get; }
        public bool IsOnOffBool { get; }

        public LevelFieldInfo(FieldType fieldType, int offset)
        {
            FieldType = fieldType;
            Offset = offset;
            IsOnOffBool = false;
        }

        public LevelFieldInfo(FieldType fieldType, int offset, bool isOnOffBool)
        {
            FieldType = fieldType;
            Offset = offset;
            IsOnOffBool = isOnOffBool;
        }
    }
}