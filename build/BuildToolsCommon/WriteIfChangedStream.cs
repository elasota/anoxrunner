using System;

namespace BuildToolsCommon
{
    public class WriteIfChangedStream : MemoryStream
    {
        private string _outPath;

        public WriteIfChangedStream(string outPath)
        {
            _outPath = outPath;
        }

        public override void Close()
        {
            int length = (int)this.Length;

            base.Close();

            using (FileStream fs = new FileStream(_outPath, FileMode.OpenOrCreate, FileAccess.ReadWrite))
            {
                bool isChanged = true;

                ReadOnlySpan<byte> newSpan = new ReadOnlySpan<byte>(GetBuffer()).Slice(0, length);

                if (fs.Length == length)
                {
                    byte[] existingBytes = new byte[length];

                    fs.ReadExactly(existingBytes);

                    ReadOnlySpan<byte> existingSpan = new ReadOnlySpan<byte>(existingBytes);

                    if (existingSpan.SequenceEqual<byte>(newSpan))
                        isChanged = false;
                }

                if (isChanged)
                {
                    fs.Position = 0;
                    fs.SetLength(0);
                    fs.Write(newSpan);
                }
            }
        }
    }
}
