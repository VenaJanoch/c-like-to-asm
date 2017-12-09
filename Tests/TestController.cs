using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace Tests
{
    [TestClass]
    public class TestController
    {
        private TestContext testContext;

        public TestContext TestContext
        {
            get { return testContext; }
            set { testContext = value; }
        }

        [TestInitialize]
        public void Initialize()
        {

        }

        [TestMethod]

        [DeploymentItem("Tests.xml")]
        [DeploymentItem("c-like-to-x86.exe")]
        [DeploymentItem("Dos.exe")]

        [DeploymentItem("Sources\\", ".\\Sources\\")]
        [DeploymentItem("Inputs\\", ".\\Inputs\\")]
        [DeploymentItem("Outputs\\", ".\\Outputs\\")]

        [DataSource("Microsoft.VisualStudio.TestTools.DataSource.XML",
            "|DataDirectory|\\Tests.xml", "Test",
            DataAccessMethod.Sequential)]
        public void Run()
        {
            const string targetPath = "_tmp.exe";

            // Load data for test
            string sourcePath = (string)TestContext.DataRow["Source"];
            string expectedOutputPath = (string)TestContext.DataRow["Output"];

            string args, inputPath;
            int expectedExitCode;
            try {
                args = (string)TestContext.DataRow["Args"];         // Optimal
            } catch {
                args = null;
            }

            try {
                inputPath = (string)TestContext.DataRow["Input"];   // Optimal
            } catch {
                inputPath = "";
            }

            try {
                int.TryParse((string)TestContext.DataRow["ExitCode"], out expectedExitCode); // Optimal
            } catch {
                expectedExitCode = 0;
            }

            string input;
            if (string.IsNullOrEmpty(inputPath)) {
                input = "";
            } else {
                input = File.ReadAllText(Path.Combine("Inputs", inputPath)).Replace("\r\n", "\r").Replace("\n", "\r");
            }
            string expectedOutput = File.ReadAllText(Path.Combine("Outputs", expectedOutputPath));

            // Compile source code
            Compile(Path.Combine("Sources", sourcePath), targetPath);

            // Run compiled program
            string output, error;
            int exitCode = CreateDosProcess(targetPath, args, input, out output, out error);

            Assert.AreEqual(expectedExitCode, exitCode, "Unexpected Exit code of DOS Process.");

            Assert.AreEqual(0, error.Length, "DOS Process wrote to stderr: " + error);

            // Check output
            Assert.AreEqual(expectedOutput, output, "DOS Process output mismatch.");

            // Remove compiled program
            try {
                File.Delete(targetPath);
            } catch {
                // Nothing to do...
            }
        }

        [TestCleanup]
        public void Cleanup()
        {

        }

        private void Compile(string sourcePath, string targetPath)
        {
            Process p = new Process {
                StartInfo = new ProcessStartInfo {
                    FileName = "c-like-to-x86.exe",
                    Arguments = "\"" + sourcePath.Replace("\"", "\\\"") + "\" \"" + targetPath.Replace("\"", "\\\"") + "\"",
                    CreateNoWindow = true,
                    WindowStyle = ProcessWindowStyle.Hidden,
                    RedirectStandardInput = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false
                }
            };

            p.Start();

            p.WaitForExit(10000);

            if (!p.HasExited) {
                try {
                    p.Kill();
                } catch {
                    // Nothing to do...
                }

                Assert.Fail("Compiler timed-out.");
            }

            Assert.AreEqual(0, p.ExitCode, "Compilation failed.");
        }

        private int CreateDosProcess(string target, string args, string stdin, out string stdout, out string stderr)
        {
            Process p = new Process {
                StartInfo = new ProcessStartInfo {
                    FileName = "Dos.exe",
                    Arguments = "\"" + target.Replace("\"", "\\\"") + "\"" + (string.IsNullOrEmpty(args) ? args : (" " + args)),
                    CreateNoWindow = true,
                    WindowStyle = ProcessWindowStyle.Hidden,
                    RedirectStandardInput = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false
                }
            };

            p.Start();

            p.StandardInput.Write(stdin);
            p.StandardInput.Close();

            p.WaitForExit(3000);

            if (!p.HasExited) {
                try {
                    p.Kill();
                } catch {
                    // Nothing to do...
                }

                Assert.Fail("DOS Process timed-out.");
            }

            stdout = p.StandardOutput.ReadToEnd();
            stderr = p.StandardError.ReadToEnd();

            return p.ExitCode;
        }
    }
}