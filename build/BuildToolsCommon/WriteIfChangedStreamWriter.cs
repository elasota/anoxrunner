using System;

namespace BuildToolsCommon
{
    public class WriteIfChangedStreamWriter : StreamWriter
    {
        public WriteIfChangedStreamWriter(string outPath)
            : base(new WriteIfChangedStream(outPath))
        {
        }
        public WriteIfChangedStreamWriter(string outPath, System.Text.Encoding encoding)
            : base(new WriteIfChangedStream(outPath), encoding)
        {
        }
    }
}
