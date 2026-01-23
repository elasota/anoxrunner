using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ProjectGenerator
{
    internal class TargetDefs
    {
        public List<TargetPlatform> Platforms { get; set; } = new List<TargetPlatform>();
        public List<TargetConfiguration> Configurations { get; set; } = new List<TargetConfiguration>();
    }
}
