using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ProjectGenerator
{
    internal class TargetPlatform
    {
        public string ArchName { get; private set; }
        public string SolutionPlatformName { get; private set; }

        public TargetPlatform(string solutionPlatformName, string archName)
        {
            SolutionPlatformName = solutionPlatformName;
            ArchName = archName;
        }
    }
}
