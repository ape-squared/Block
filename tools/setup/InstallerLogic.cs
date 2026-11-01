using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace Block.Setup
{
    public sealed class ViewerInfo
    {
        public string Name { get; set; }
        public string Path { get; set; }
        public bool Installed { get; set; }
        public string Status => Installed ? "Installed" : "Ready";
    }

    public static class InstallerLogic
    {
        public static string RepoRoot { get; private set; }
        public static string ConfigureScript => Path.Combine(RepoRoot, "tools", "configure_gshade_support.ps1");

        static InstallerLogic()
        {
            RepoRoot = FindRepoRoot();
        }

        static string FindRepoRoot()
        {
            var dir = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
            while (dir != null)
            {
                if (File.Exists(Path.Combine(dir.FullName, "tools", "configure_gshade_support.ps1")))
                    return dir.FullName;
                dir = dir.Parent;
            }
            return AppDomain.CurrentDomain.BaseDirectory;
        }

        public static string InjectorDll()
        {
            foreach (var rel in new[]
            {
                Path.Combine("build-win", "runtime", "opengl", "opengl32.dll"),
                Path.Combine("build-win", "hook", "opengl32.dll")
            })
            {
                var p = Path.Combine(RepoRoot, rel);
                if (File.Exists(p)) return p;
            }
            return Path.Combine(RepoRoot, "build-win", "runtime", "opengl", "opengl32.dll");
        }

        public static List<ViewerInfo> FindViewers()
        {
            var patterns = new[]
            {
                "Firestorm*", "SecondLife*", "Alchemy*", "Cool VL Viewer*",
                "Black Dragon*", "Singularity*", "Catznip*"
            };
            var roots = new List<string>();
            var pf = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
            var x86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
            if (!string.IsNullOrEmpty(pf)) roots.Add(pf);
            if (!string.IsNullOrEmpty(x86) && !roots.Contains(x86)) roots.Add(x86);

            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var found = new List<ViewerInfo>();
            foreach (var root in roots)
            {
                if (!Directory.Exists(root)) continue;
                foreach (var pat in patterns)
                {
                    DirectoryInfo[] dirs;
                    try { dirs = new DirectoryInfo(root).GetDirectories(pat); }
                    catch { continue; }
                    foreach (var d in dirs)
                    {
                        if (!seen.Add(d.FullName)) continue;
                        found.Add(new ViewerInfo
                        {
                            Name = d.Name,
                            Path = d.FullName,
                            Installed = File.Exists(Path.Combine(d.FullName, "opengl32.dll"))
                        });
                    }
                }
            }
            return found.OrderBy(v => v.Name).ToList();
        }

        public static IEnumerable<string> Install(string viewerDir)
        {
            var dll = InjectorDll();
            if (!File.Exists(dll))
                throw new FileNotFoundException("Build the injector first: tools\\build_reshade_block.ps1", dll);

            yield return "Installing → " + viewerDir;
            var dest = Path.Combine(viewerDir, "opengl32.dll");
            if (File.Exists(dest))
            {
                var bak = dest + ".pre-block-" + DateTime.Now.ToString("yyyyMMdd-HHmmss");
                File.Copy(dest, bak, true);
                yield return "Backed up existing opengl32.dll";
            }
            File.Copy(dll, dest, true);
            yield return "Copied injector (" + new FileInfo(dll).Length + " bytes)";

            var psi = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                Arguments = "-NoProfile -ExecutionPolicy Bypass -File \"" + ConfigureScript + "\" -ViewerDir \"" + viewerDir + "\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };
            using (var p = Process.Start(psi))
            {
                var stdout = p.StandardOutput.ReadToEnd();
                var stderr = p.StandardError.ReadToEnd();
                p.WaitForExit();
                foreach (var line in (stdout + "\n" + stderr).Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
                    yield return line;
                if (p.ExitCode != 0)
                    throw new Exception("configure_gshade_support failed (" + p.ExitCode + ")");
            }
            yield return "Done.";
        }
    }
}
