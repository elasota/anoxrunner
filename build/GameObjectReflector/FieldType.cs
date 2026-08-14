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
        Optional,
        Vector,
        Struct,
    }

    internal enum FieldResourceType
    {
        Invalid,
        Scene,
    }

    internal class FieldType : IEquatable<FieldType>
    {
        public FieldMainType MainType { get; }
        public FieldType? SubType { get; } = null;
        public FieldResourceType ResourceType { get; } = FieldResourceType.Invalid;
        public string? SubName { get; } = null;

        public FieldType(FieldMainType mainType)
        {
            MainType = mainType;
        }

        public FieldType(FieldResourceType resType)
        {
            MainType = FieldMainType.Resource;
            ResourceType = resType;
        }

        public FieldType(FieldMainType mainType, FieldType subType)
        {
            MainType = mainType;
            SubType = subType;
        }

        public FieldType(FieldMainType mainType, string subName)
        {
            MainType = mainType;
            SubName = subName;
        }

        public bool Equals(FieldType? other)
        {
            if (other == null)
                return false;

            if (MainType != other.MainType || ResourceType != other.ResourceType)
                return false;

            if (SubType != null)
            {
                if (other.SubType == null)
                    return false;

                if (!SubType.Equals(other.SubType))
                    return false;
            }
            else
            {
                if (other.SubType != null)
                    return false;
            }

            if (SubName != null)
            {
                if (other.SubName == null)
                    return false;

                if (!SubName.Equals(other.SubName))
                    return false;
            }
            else
            {
                if (other.SubName != null)
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
