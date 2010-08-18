using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.CodeDom.Compiler;
using Microsoft.CSharp;

namespace bcompile
{
    class Program
    {
        private static String batchContainer;

        static void Main(string[] args)
        {
            if (args.Length <= 2)
            {
                Console.WriteLine("insufficient arguments submitted!" + Environment.NewLine + "usage: bcompile infile.bat outfile.exe -[hidden|visible]" +
                Environment.NewLine);
                return;
            }

            // create variables to hold arguments
            String srcFile = "";
            String outFile = "";
            String optionFlag = "";
            Boolean isHidden = false;
            Boolean isCompiled = false;

            try
            {
                srcFile = args[0].ToString(); //first argument: source file
                outFile = args[1].ToString(); //second argument: target file
                optionFlag = args[2].ToString(); //third argument: -hidden or -visible 
                if (optionFlag == "-hidden")
                {
                    isHidden = true;
                }
                else
                    isHidden = false;


                Console.WriteLine(Environment.NewLine +
                Convert.ToString(srcFile) + " ==> " + Convert.ToString(outFile) + "  (" + Convert.ToString(optionFlag) + ") " +
                Environment.NewLine);

                Program.batchContainer = File.ReadAllText(srcFile);
                
                try
                {  
                    isCompiled = Program.compile(outFile, isHidden);
                }
                catch
                {
                    // display indication of invalid input from command line
                    Console.WriteLine(Environment.NewLine + "error during compile operation!" +
                    Environment.NewLine);
                    return;                    
                }

                /*
                Random rnd = new Random();
                int i  = rnd.Next(100000000,999999999);
                Console.WriteLine(i);
                */

                return;
            }
            catch
            {
                // display indication of invalid input from command line
                Console.WriteLine(Environment.NewLine + "invalid input!" +
                Environment.NewLine);
                return;
            }
        }

        private static bool compile(string path, bool hidden)
        {
            bool result = false;
            string filepath = "";

            Random rnd = new Random();
            int i = rnd.Next(100000000, 999999999);

            if (hidden)
            {
                filepath = Environment.GetEnvironmentVariable("TEMP") + "\\" + i + "hideit.bat";
            }
            else
                filepath = Environment.GetEnvironmentVariable("TEMP") + "\\" + i + "it.bat";

            using (StreamWriter sw = new StreamWriter(filepath))
            {
                sw.Write(Program.batchContainer);
            }

            using (CSharpCodeProvider code = new CSharpCodeProvider())
            {
                CompilerParameters compar = new CompilerParameters();

                string option = "/target:winexe";

                compar.CompilerOptions = option;
                compar.GenerateExecutable = true;
                compar.IncludeDebugInformation = false;

                if (File.Exists(filepath))
                {
                    compar.EmbeddedResources.Add(filepath);
                }

                compar.OutputAssembly = path;
                compar.GenerateInMemory = false;

                compar.ReferencedAssemblies.Add("System.dll");
                compar.ReferencedAssemblies.Add("System.Data.dll");
                compar.ReferencedAssemblies.Add("System.Deployment.dll");
                compar.ReferencedAssemblies.Add("System.Drawing.dll");
                compar.ReferencedAssemblies.Add("System.Windows.Forms.dll");
                compar.ReferencedAssemblies.Add("System.Xml.dll");

                compar.TreatWarningsAsErrors = false;

                CompilerResults res = code.CompileAssemblyFromSource(compar, Properties.Resources.Program);

                if (res.Errors.Count > 0)
                {
                    result = false;
                }
                else
                    result = true;
            }

            return result;
        }
    }
}

