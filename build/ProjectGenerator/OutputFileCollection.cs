using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ProjectGenerator
{
    internal class OutputFileCollection
    {
        public Dictionary<string, MemoryStream> Files { get; set; } = new Dictionary<string, MemoryStream>();


        public Stream Open(string filename)
        {
            MemoryStream stream = new MemoryStream();
            Files[filename] = stream;
            return stream;
        }
    }
}
