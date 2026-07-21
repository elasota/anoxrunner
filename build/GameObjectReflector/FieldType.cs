namespace GameObjectReflector
{
    internal enum FieldMainType
    {
        Float,
        UInt,
        UInt64,
        Vec2,
        Vec3,
        Vec4,
        Bool,
        ByteString,
        Label,
        BspModel,
        Broken,
        EDef,
        Resource,
    }

    internal enum FieldResourceType
    {
        Invalid,
        Scene,
    }

    internal class FieldType : IEquatable<FieldType>
    {
        public FieldMainType MainType { get; }
        public FieldType? SubType { get; }
        public FieldResourceType ResourceType { get; }

        public FieldType(FieldMainType mainType)
        {
            MainType = mainType;
            SubType = null;
            ResourceType = FieldResourceType.Invalid;
        }

        public FieldType(FieldResourceType resType)
        {
            MainType = FieldMainType.Resource;
            SubType = null;
            ResourceType = resType;
        }

        public bool Equals(FieldType? other)
        {
            if (other == null)
                return false;

            if (MainType != other.MainType || ResourceType != other.ResourceType)
                return false;

            if (SubType != null)
            {
                if (other.SubType != null)
                    return false;

                if (!SubType.Equals(other.SubType))
                    return false;
            }

            return true;
        }

        public override bool Equals(object? other)
        {
            if (other == null)
                return false;

            if (other.GetType() != this.GetType())
                return false;

            return Equals((FieldType?)other);
        }

        public override int GetHashCode()
        {
            return MainType.GetHashCode();
        }
    }
}
