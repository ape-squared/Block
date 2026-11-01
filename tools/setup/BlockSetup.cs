using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace Block.Setup
{
    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new WizardForm());
        }
    }

    static class Brand
    {
        public static readonly Color Signal = Color.FromArgb(0xD4, 0xFF, 0x3F);
        public static readonly Color Surface0 = Color.FromArgb(0x0B, 0x0C, 0x0E);
        public static readonly Color Surface1 = Color.FromArgb(0x13, 0x14, 0x17);
        public static readonly Color Surface2 = Color.FromArgb(0x1B, 0x1D, 0x21);
        public static readonly Color Surface3 = Color.FromArgb(0x25, 0x28, 0x2D);
        public static readonly Color Line = Color.FromArgb(0x2E, 0x32, 0x38);
        public static readonly Color Ink = Color.FromArgb(0xEC, 0xEE, 0xF0);
        public static readonly Color InkMuted = Color.FromArgb(0x9A, 0xA0, 0xA8);
        public static readonly Color Ok = Color.FromArgb(0x3D, 0xDC, 0x97);
    }

    sealed class WizardForm : Form
    {
        readonly string _repoRoot;
        readonly string _configurePs1;
        readonly string _assets;
        Image _lockup;
        Image _titleIcon;
        int _step;
        bool _agree = true;
        readonly ListView _viewers;
        readonly ListView _addons;
        readonly TextBox _log;
        readonly Label _progressLabel;
        readonly Label _progressPct;
        readonly Panel _progressTrack;
        readonly Panel _progressFill;
        readonly Panel _pageWelcome;
        readonly Panel _pageOptions;
        readonly Panel _pageInstall;
        readonly Panel _pageDone;
        readonly Panel _stepNav;
        readonly Button _btnSecondary;
        readonly Button _btnPrimary;
        readonly Label _statusHint;
        readonly Label _addonHint;

        public WizardForm()
        {
            _repoRoot = FindRepoRoot();
            _configurePs1 = Path.Combine(_repoRoot, "tools", "configure_gshade_support.ps1");
            _assets = Path.Combine(_repoRoot, "tools", "setup", "Assets");
            if (!Directory.Exists(_assets))
                _assets = Path.Combine(_repoRoot, "tools", "Assets");
            if (!Directory.Exists(_assets))
                _assets = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets");

            TryLoadImages();

            Text = "Block Setup";
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(760, 500);
            BackColor = Brand.Surface1;
            DoubleBuffered = true;
            var ico = Path.Combine(_assets, "block.ico");
            if (File.Exists(ico)) Icon = new Icon(ico);

            // Title bar
            var title = new Panel { Height = 36, Dock = DockStyle.Top, BackColor = Brand.Surface0 };
            title.Paint += (s, e) =>
            {
                e.Graphics.DrawLine(new Pen(Brand.Line), 0, title.Height - 1, title.Width, title.Height - 1);
            };
            title.MouseDown += (s, e) => { if (e.Button == MouseButtons.Left) NativeDrag(); };
            if (_titleIcon != null)
            {
                var pic = new PictureBox
                {
                    Image = _titleIcon, SizeMode = PictureBoxSizeMode.Zoom,
                    Location = new Point(14, 10), Size = new Size(16, 16)
                };
                title.Controls.Add(pic);
            }
            title.Controls.Add(new Label
            {
                Text = "Block Setup", ForeColor = Brand.InkMuted, Font = new Font("Segoe UI", 9f, FontStyle.Regular),
                Location = new Point(36, 9), AutoSize = true
            });
            var btnClose = MakeChromeButton("✕", 714);
            btnClose.Click += (s, e) => Close();
            var btnMin = MakeChromeButton("─", 668);
            btnMin.Click += (s, e) => WindowState = FormWindowState.Minimized;
            title.Controls.Add(btnClose);
            title.Controls.Add(btnMin);
            Controls.Add(title);

            // Body
            var body = new Panel { Location = new Point(0, 36), Size = new Size(760, 412), BackColor = Brand.Surface1 };

            // Sidebar
            var side = new Panel { Location = new Point(0, 0), Size = new Size(200, 412), BackColor = Brand.Surface0 };
            side.Paint += (s, e) => e.Graphics.DrawLine(new Pen(Brand.Line), side.Width - 1, 0, side.Width - 1, side.Height);
            if (_lockup != null)
            {
                side.Controls.Add(new PictureBox
                {
                    Image = _lockup, SizeMode = PictureBoxSizeMode.Zoom,
                    Location = new Point(20, 24), Size = new Size(120, 36)
                });
            }
            else
            {
                side.Controls.Add(new Label
                {
                    Text = "BLOCK", ForeColor = Brand.Signal, Font = new Font("Segoe UI", 18f, FontStyle.Bold),
                    Location = new Point(20, 24), AutoSize = true
                });
            }
            _stepNav = new Panel { Location = new Point(20, 78), Size = new Size(160, 200), BackColor = Brand.Surface0 };
            side.Controls.Add(_stepNav);
            side.Controls.Add(new Label
            {
                Text = "v0.3.2 · x64", ForeColor = Brand.InkMuted, Font = new Font("Consolas", 8.5f),
                Location = new Point(20, 380), AutoSize = true
            });
            body.Controls.Add(side);

            // Pages host
            var pages = new Panel { Location = new Point(200, 0), Size = new Size(560, 412), BackColor = Brand.Surface1 };

            _pageWelcome = BuildWelcome();
            _pageOptions = BuildOptions(out _viewers, out _addons, out _statusHint, out _addonHint);
            _pageInstall = BuildInstall(out _log, out _progressLabel, out _progressPct, out _progressTrack, out _progressFill);
            _pageDone = BuildDone();
            foreach (var p in new[] { _pageWelcome, _pageOptions, _pageInstall, _pageDone })
            {
                p.Location = new Point(30, 28);
                p.Size = new Size(500, 360);
                p.Visible = false;
                pages.Controls.Add(p);
            }
            body.Controls.Add(pages);
            Controls.Add(body);

            // Footer
            var footer = new Panel { Height = 52, Dock = DockStyle.Bottom, BackColor = Brand.Surface0 };
            footer.Paint += (s, e) => e.Graphics.DrawLine(new Pen(Brand.Line), 0, 0, footer.Width, 0);
            _btnSecondary = MakeBtn("Cancel", false);
            _btnSecondary.Location = new Point(560, 12);
            _btnSecondary.Click += Secondary_Click;
            _btnPrimary = MakeBtn("Next", true);
            _btnPrimary.Location = new Point(658, 12);
            _btnPrimary.Click += Primary_Click;
            footer.Controls.Add(_btnSecondary);
            footer.Controls.Add(_btnPrimary);
            Controls.Add(footer);

            // Outer border
            Paint += (s, e) => e.Graphics.DrawRectangle(new Pen(Brand.Line), 0, 0, Width - 1, Height - 1);

            RefreshViewers();
            RefreshAddons();
            ShowStep(0);
        }

        void TryLoadImages()
        {
            try
            {
                var lockup = Path.Combine(_assets, "lockup.png");
                var icon = Path.Combine(_assets, "title-icon.png");
                if (File.Exists(lockup)) _lockup = Image.FromFile(lockup);
                if (File.Exists(icon)) _titleIcon = Image.FromFile(icon);
            }
            catch { }
        }

        Panel BuildWelcome()
        {
            var p = new Panel { BackColor = Brand.Surface1 };
            p.Controls.Add(new Label
            {
                Text = "Welcome to Block.", AutoSize = true, Location = new Point(0, 0),
                Font = new Font("Segoe UI", 22f, FontStyle.Bold), ForeColor = Brand.Ink
            });
            // paint the period lime via overlay label
            p.Controls.Add(new Label
            {
                Text = ".", AutoSize = true, Location = new Point(268, 0),
                Font = new Font("Segoe UI", 22f, FontStyle.Bold), ForeColor = Brand.Signal
            });
            p.Controls.Add(new Label
            {
                Text = "Post-processing for Second Life viewers. Block injects into the OpenGL viewer you choose (Firestorm, Alchemy, Black Dragon, and more), uses your GShade pack when present, and opens with Shift+B.",
                Location = new Point(0, 48), Size = new Size(500, 68), ForeColor = Brand.InkMuted, Font = new Font("Segoe UI", 10f)
            });
            var card = new Panel { Location = new Point(0, 126), Size = new Size(500, 110), BackColor = Brand.Surface2 };
            card.Paint += (s, e) =>
            {
                e.Graphics.DrawRectangle(new Pen(Brand.Line), 0, 0, card.Width - 1, card.Height - 1);
            };
            card.Controls.Add(Bullet(10, 14, "Firestorm, Alchemy, Black Dragon, Cool VL, Singularity…"));
            card.Controls.Add(Bullet(10, 42, "GShade / Heard presets + OpenGL-safe addons"));
            card.Controls.Add(Bullet(10, 70, "Overlay hotkey: Shift+B"));
            p.Controls.Add(card);
            var agree = new CheckBox
            {
                Checked = true, Location = new Point(0, 248), Size = new Size(500, 40),
                Text = "I agree to the licence. Block is independent and not affiliated with Linden Lab.",
                ForeColor = Brand.InkMuted, Font = new Font("Segoe UI", 9.5f), FlatStyle = FlatStyle.Flat
            };
            agree.CheckedChanged += (s, e) => { _agree = agree.Checked; if (_step == 0) _btnPrimary.Enabled = _agree; };
            p.Controls.Add(agree);
            return p;
        }

        Label Bullet(int x, int y, string text)
        {
            return new Label
            {
                Text = "•  " + text, Location = new Point(x, y), AutoSize = true,
                ForeColor = Brand.Ink, Font = new Font("Segoe UI", 9.5f), BackColor = Brand.Surface2
            };
        }

        Panel BuildOptions(out ListView list, out ListView addons, out Label status, out Label addonHint)
        {
            var p = new Panel { BackColor = Brand.Surface1 };
            p.Controls.Add(new Label
            {
                Text = "Options", AutoSize = true, Location = new Point(0, 0),
                Font = new Font("Segoe UI", 16f, FontStyle.Bold), ForeColor = Brand.Ink
            });
            p.Controls.Add(new Label
            {
                Text = "Viewers", AutoSize = true, Location = new Point(0, 36),
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = Brand.InkMuted
            });
            list = new ListView
            {
                Location = new Point(0, 56), Size = new Size(500, 100),
                View = View.Details, FullRowSelect = true, CheckBoxes = true,
                BackColor = Brand.Surface2, ForeColor = Brand.Ink, BorderStyle = BorderStyle.FixedSingle,
                Font = new Font("Segoe UI", 10f), HeaderStyle = ColumnHeaderStyle.None,
                OwnerDraw = false
            };
            list.Columns.Add("Name", 150);
            list.Columns.Add("Path", 260);
            list.Columns.Add("Status", 70);
            p.Controls.Add(list);

            p.Controls.Add(new Label
            {
                Text = "Addons", AutoSize = true, Location = new Point(0, 164),
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = Brand.InkMuted
            });
            addons = new ListView
            {
                Location = new Point(0, 184), Size = new Size(500, 118),
                View = View.Details, FullRowSelect = true, CheckBoxes = true,
                BackColor = Brand.Surface2, ForeColor = Brand.Ink, BorderStyle = BorderStyle.FixedSingle,
                Font = new Font("Segoe UI", 10f), HeaderStyle = ColumnHeaderStyle.None
            };
            addons.Columns.Add("Addon", 240);
            addons.Columns.Add("Source", 180);
            addons.Columns.Add("Note", 60);
            p.Controls.Add(addons);

            addonHint = new Label
            {
                Location = new Point(0, 306), Size = new Size(500, 32),
                Font = new Font("Segoe UI", 8.5f), ForeColor = Brand.InkMuted,
                Text = "Check addons to copy into each viewer’s gshade-addons. DLSS / renodx blocked for OpenGL.\nEmpty catalog? Drop *.addon64 into %LOCALAPPDATA%\\Block\\addons-catalog or the viewer’s gshade-addons."
            };
            p.Controls.Add(addonHint);

            var rescan = MakeBtn("Rescan", false);
            rescan.Location = new Point(0, 348);
            rescan.Size = new Size(96, 28);
            rescan.Click += (s, e) => { RefreshViewers(); RefreshAddons(); };
            var browse = MakeBtn("Browse…", false);
            browse.Location = new Point(104, 348);
            browse.Size = new Size(96, 28);
            browse.Click += Browse_Click;
            var openCatalog = MakeBtn("Open catalog", false);
            openCatalog.Location = new Point(208, 348);
            openCatalog.Size = new Size(110, 28);
            openCatalog.Click += (s, e) =>
            {
                var catalog = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Block", "addons-catalog");
                try { Directory.CreateDirectory(catalog); } catch { }
                try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo { FileName = catalog, UseShellExecute = true }); } catch { }
            };
            p.Controls.Add(openCatalog);
            status = new Label
            {
                AutoSize = true, Location = new Point(326, 354),
                Font = new Font("Consolas", 9f), ForeColor = Brand.Ok,
                Text = File.Exists(InjectorDll()) ? "Injector ready" : "Build injector first"
            };
            p.Controls.Add(rescan);
            p.Controls.Add(browse);
            p.Controls.Add(status);
            return p;
        }

        Panel BuildInstall(out TextBox log, out Label label, out Label pct, out Panel track, out Panel fill)
        {
            var p = new Panel { BackColor = Brand.Surface1 };
            p.Controls.Add(new Label
            {
                Text = "Installing Block", AutoSize = true, Location = new Point(0, 0),
                Font = new Font("Segoe UI", 16f, FontStyle.Bold), ForeColor = Brand.Ink
            });
            label = new Label { Text = "Preparing…", Location = new Point(0, 40), AutoSize = true, Font = new Font("Consolas", 9.5f), ForeColor = Brand.Ink };
            pct = new Label { Text = "0%", Location = new Point(450, 40), AutoSize = true, Font = new Font("Consolas", 9.5f), ForeColor = Brand.Signal };
            p.Controls.Add(label);
            p.Controls.Add(pct);
            track = new Panel { Location = new Point(0, 68), Size = new Size(500, 8), BackColor = Brand.Surface3 };
            fill = new Panel { Location = new Point(0, 0), Size = new Size(0, 8), BackColor = Brand.Signal };
            track.Controls.Add(fill);
            p.Controls.Add(track);
            log = new TextBox
            {
                Location = new Point(0, 92), Size = new Size(500, 230), Multiline = true, ReadOnly = true,
                ScrollBars = ScrollBars.Vertical, BackColor = Brand.Surface0, ForeColor = Brand.Ink,
                Font = new Font("Consolas", 9f), BorderStyle = BorderStyle.FixedSingle
            };
            p.Controls.Add(log);
            return p;
        }

        Panel BuildDone()
        {
            var p = new Panel { BackColor = Brand.Surface1 };
            var badge = new Panel { Location = new Point(0, 8), Size = new Size(56, 56), BackColor = Brand.Signal };
            badge.Paint += (s, e) =>
            {
                e.Graphics.SmoothingMode = SmoothingMode.None;
                using (var f = new Font("Segoe UI", 20f, FontStyle.Bold))
                using (var br = new SolidBrush(Brand.Surface0))
                {
                    var sz = e.Graphics.MeasureString("✓", f);
                    e.Graphics.DrawString("✓", f, br, (56 - sz.Width) / 2, (56 - sz.Height) / 2);
                }
            };
            p.Controls.Add(badge);
            p.Controls.Add(new Label
            {
                Text = "Block is ready.", AutoSize = true, Location = new Point(0, 80),
                Font = new Font("Segoe UI", 22f, FontStyle.Bold), ForeColor = Brand.Ink
            });
            p.Controls.Add(new Label
            {
                Text = ".", AutoSize = true, Location = new Point(232, 80),
                Font = new Font("Segoe UI", 22f, FontStyle.Bold), ForeColor = Brand.Signal
            });
            p.Controls.Add(new Label
            {
                Text = "OpenGL viewers (Firestorm, Alchemy, Black Dragon, …): launch normally, Shift+B. Aurora (Vulkan): use \"Launch Aurora with Block.bat\" — bare .exe will not load Block.",
                Location = new Point(0, 128), Size = new Size(500, 90), ForeColor = Brand.InkMuted, Font = new Font("Segoe UI", 10f)
            });
            return p;
        }

        void ShowStep(int step)
        {
            _step = step;
            _pageWelcome.Visible = step == 0;
            _pageOptions.Visible = step == 1;
            _pageInstall.Visible = step == 2;
            _pageDone.Visible = step == 3;
            RebuildStepNav();
            _btnSecondary.Visible = step != 3;
            _btnSecondary.Text = step == 0 ? "Cancel" : "Back";
            _btnSecondary.Enabled = step != 2;
            _btnPrimary.Text = step == 0 ? "Next" : step == 1 ? "Install" : step == 2 ? "…" : "Finish";
            _btnPrimary.Enabled = step != 2 && (step != 0 || _agree);
        }

        void RebuildStepNav()
        {
            _stepNav.Controls.Clear();
            string[] names = { "Welcome", "Options", "Install", "Done" };
            for (int i = 0; i < names.Length; i++)
            {
                int y = i * 36;
                bool active = i == _step, done = i < _step;
                var badge = new Panel
                {
                    Location = new Point(0, y), Size = new Size(20, 20),
                    BackColor = (active || done) ? Brand.Signal : Brand.Surface2
                };
                int idx = i;
                badge.Paint += (s, e) =>
                {
                    string t = idx < _step ? "✓" : (idx + 1).ToString();
                    using (var f = new Font("Consolas", 8f, FontStyle.Bold))
                    using (var br = new SolidBrush((idx <= _step) ? Brand.Surface0 : Brand.InkMuted))
                    {
                        var sz = e.Graphics.MeasureString(t, f);
                        e.Graphics.DrawString(t, f, br, (20 - sz.Width) / 2, (20 - sz.Height) / 2 + 1);
                    }
                };
                _stepNav.Controls.Add(badge);
                _stepNav.Controls.Add(new Label
                {
                    Text = names[i], Location = new Point(28, y + 2), AutoSize = true,
                    Font = new Font("Segoe UI", 9f, active ? FontStyle.Bold : FontStyle.Regular),
                    ForeColor = active ? Brand.Ink : Brand.InkMuted, BackColor = Brand.Surface0
                });
            }
        }

        void RefreshViewers()
        {
            _viewers.Items.Clear();
            foreach (var v in FindViewers())
            {
                var item = new ListViewItem(v.Name);
                item.SubItems.Add(v.Path);
                bool vk = IsVulkanViewer(v.Path);
                string status = vk
                    ? (Directory.Exists(Path.Combine(v.Path, "block-vulkan")) ? "Vulkan" : "Vulkan?")
                    : (File.Exists(Path.Combine(v.Path, "opengl32.dll")) ? "Installed" : "Ready");
                item.SubItems.Add(status);
                item.Tag = v.Path;
                if (v.Name.IndexOf("Firestorm-Release", StringComparison.OrdinalIgnoreCase) >= 0)
                    item.Checked = true;
                _viewers.Items.Add(item);
            }
        }

        static bool IsBlockedOpenGlAddon(string fileName)
        {
            var n = Path.GetFileNameWithoutExtension(fileName).ToLowerInvariant();
            return n.Contains("renodx") || n.Contains("dlss") || n.Contains("ngx") ||
                   n.Contains("dxgi") || n.Contains("fsr3") || n.Contains("xess");
        }

        void AddAddonFromDir(string dir, string label, HashSet<string> seen)
        {
            if (!Directory.Exists(dir)) return;
            foreach (var f in Directory.GetFiles(dir, "*.addon*"))
            {
                var name = Path.GetFileName(f);
                if (!seen.Add(name)) continue;
                bool blocked = IsBlockedOpenGlAddon(name);
                var item = new ListViewItem(Path.GetFileNameWithoutExtension(name));
                item.SubItems.Add(label);
                item.SubItems.Add(blocked ? "blocked" : "ok");
                item.Tag = f;
                item.Checked = !blocked;
                if (blocked)
                {
                    item.ForeColor = Brand.InkMuted;
                    item.Checked = false;
                }
                _addons.Items.Add(item);
            }
        }

        void RefreshAddons()
        {
            _addons.Items.Clear();
            var catalog = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Block", "addons-catalog");
            try { Directory.CreateDirectory(catalog); } catch { }
            var payloadAddons = Path.Combine(_repoRoot, "payload", "addons");
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            AddAddonFromDir(payloadAddons, "payload", seen);
            AddAddonFromDir(catalog, "catalog", seen);

            // Also surface addons already in viewers' gshade-addons
            foreach (ListViewItem v in _viewers.Items)
            {
                var viewerAddons = Path.Combine((string)v.Tag, "gshade-addons");
                AddAddonFromDir(viewerAddons, Path.GetFileName((string)v.Tag), seen);
            }

            if (_addonHint != null)
            {
                int n = _addons.Items.Count;
                int blocked = 0;
                foreach (ListViewItem i in _addons.Items)
                    if (i.SubItems[2].Text == "blocked") blocked++;
                _addonHint.Text = n == 0
                    ? "No *.addon64 found. Copy safe OpenGL addons into %LOCALAPPDATA%\\Block\\addons-catalog (Open catalog) or each viewer’s gshade-addons, then Rescan. DLSS/renodx stay blocked."
                    : n + " addon(s) listed" + (blocked > 0 ? " (" + blocked + " blocked for OpenGL)." : ".") +
                      " Checked items copy into the selected viewer’s gshade-addons.";
            }
        }

        IEnumerable<string> ApplySelectedAddons(string viewerDir, List<AddonPick> picks)
        {
            var addonsRoot = Path.Combine(viewerDir, "gshade-addons");
            Directory.CreateDirectory(addonsRoot);

            // Remove blocked leftovers from prior installs
            foreach (var f in Directory.GetFiles(addonsRoot, "*.addon*"))
            {
                if (!IsBlockedOpenGlAddon(f)) continue;
                string removeMsg = null;
                try
                {
                    File.Delete(f);
                    removeMsg = "Removed blocked addon " + Path.GetFileName(f);
                }
                catch (Exception ex)
                {
                    removeMsg = "WARN  could not remove " + Path.GetFileName(f) + ": " + ex.Message;
                }
                if (removeMsg != null) yield return removeMsg;
            }

            var disabled = new List<string>();
            int enabled = 0;
            foreach (var pick in picks ?? new List<AddonPick>())
            {
                var name = pick.FileName;
                if (string.IsNullOrEmpty(name)) continue;
                if (pick.Blocked || IsBlockedOpenGlAddon(name))
                {
                    disabled.Add(Path.GetFileNameWithoutExtension(name).ToLowerInvariant());
                    continue;
                }
                var dest = Path.Combine(addonsRoot, name);
                if (pick.Enabled && File.Exists(pick.SourcePath))
                {
                    File.Copy(pick.SourcePath, dest, true);
                    enabled++;
                    yield return "Addon enabled → " + name;
                }
                else
                {
                    disabled.Add(Path.GetFileNameWithoutExtension(name).ToLowerInvariant());
                    if (File.Exists(dest))
                    {
                        try { File.Delete(dest); } catch { }
                    }
                    yield return "Addon skipped → " + name;
                }
            }

            // Write DisabledAddons into ReShade.ini [ADDON]
            var iniPath = Path.Combine(viewerDir, "ReShade.ini");
            if (File.Exists(iniPath))
            {
                var text = File.ReadAllText(iniPath);
                var value = string.Join(",", disabled.Distinct().ToArray());
                text = UpsertIni(text, "ADDON", "DisabledAddons", value);
                text = UpsertIni(text, "ADDON", "AddonPath", ".\\gshade-addons");
                File.WriteAllText(iniPath, text);
                yield return "DisabledAddons=" + (string.IsNullOrEmpty(value) ? "(none)" : value);
            }
            yield return "Addons applied (" + enabled + " enabled in gshade-addons)";
        }

        static string UpsertIni(string text, string section, string key, string value)
        {
            var sectionHeader = "[" + section + "]";
            var keyLine = key + "=" + value;
            var idx = text.IndexOf(sectionHeader, StringComparison.OrdinalIgnoreCase);
            if (idx < 0)
                return text.TrimEnd() + "\r\n\r\n" + sectionHeader + "\r\n" + keyLine + "\r\n";

            var after = idx + sectionHeader.Length;
            var next = text.IndexOf("\n[", after, StringComparison.Ordinal);
            if (next < 0) next = text.Length;
            var block = text.Substring(after, next - after);
            var keyPat = new System.Text.RegularExpressions.Regex(
                @"(?m)^\s*" + System.Text.RegularExpressions.Regex.Escape(key) + @"\s*=.*$",
                System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            string newBlock;
            if (keyPat.IsMatch(block))
                newBlock = keyPat.Replace(block, keyLine, 1);
            else
                newBlock = block.TrimEnd() + "\r\n" + keyLine + "\r\n";
            return text.Substring(0, after) + newBlock + text.Substring(next);
        }

        void Browse_Click(object sender, EventArgs e)
        {
            using (var dlg = new FolderBrowserDialog())
            {
                dlg.Description = "Select viewer install folder";
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                var item = new ListViewItem(Path.GetFileName(dlg.SelectedPath));
                item.SubItems.Add(dlg.SelectedPath);
                item.SubItems.Add(File.Exists(Path.Combine(dlg.SelectedPath, "opengl32.dll")) ? "Installed" : "Ready");
                item.Tag = dlg.SelectedPath;
                item.Checked = true;
                _viewers.Items.Add(item);
            }
        }

        void Secondary_Click(object sender, EventArgs e)
        {
            if (_step == 0) Close();
            else if (_step == 1) ShowStep(0);
        }

        sealed class AddonPick
        {
            public string SourcePath;
            public string FileName;
            public bool Enabled;
            public bool Blocked;
        }

        sealed class InstallJob
        {
            public List<string> Viewers;
            public List<AddonPick> Addons;
        }

        List<AddonPick> SnapshotAddons()
        {
            var list = new List<AddonPick>();
            foreach (ListViewItem item in _addons.Items)
            {
                var src = item.Tag as string;
                if (string.IsNullOrEmpty(src)) continue;
                list.Add(new AddonPick
                {
                    SourcePath = src,
                    FileName = Path.GetFileName(src),
                    Enabled = item.Checked,
                    Blocked = item.SubItems.Count > 2 && item.SubItems[2].Text == "blocked"
                });
            }
            return list;
        }

        void Primary_Click(object sender, EventArgs e)
        {
            if (_step == 0) { if (!_agree) return; ShowStep(1); return; }
            if (_step == 1)
            {
                var targets = _viewers.CheckedItems.Cast<ListViewItem>().Select(i => (string)i.Tag).ToList();
                if (targets.Count == 0) { MessageBox.Show(this, "Select at least one viewer.", "Block Setup"); return; }
                if (!File.Exists(InjectorDll())) { MessageBox.Show(this, "Build injector first:\ntools\\build_reshade_block.ps1", "Block Setup"); return; }
                // Uncheck blocked addons if user forced them
                foreach (ListViewItem item in _addons.Items)
                {
                    if (item.SubItems.Count > 2 && item.SubItems[2].Text == "blocked")
                        item.Checked = false;
                }
                ShowStep(2);
                _log.Clear();
                BeginInstall(new InstallJob { Viewers = targets, Addons = SnapshotAddons() });
                return;
            }
            if (_step == 3) Close();
        }

        void BeginInstall(InstallJob job)
        {
            _btnPrimary.Enabled = false;
            _btnSecondary.Enabled = false;
            var worker = new System.ComponentModel.BackgroundWorker { WorkerReportsProgress = true };
            worker.DoWork += (s, e) =>
            {
                var j = (InstallJob)e.Argument;
                for (int i = 0; i < j.Viewers.Count; i++)
                {
                    var path = j.Viewers[i];
                    worker.ReportProgress((i * 100) / j.Viewers.Count, "Installing " + Path.GetFileName(path));
                    foreach (var line in Install(path, j.Addons))
                        worker.ReportProgress(-1, line);
                    worker.ReportProgress(((i + 1) * 100) / j.Viewers.Count, "Installed " + Path.GetFileName(path));
                }
            };
            worker.ProgressChanged += (s, e) =>
            {
                if (e.ProgressPercentage < 0)
                {
                    _log.AppendText(e.UserState + "\r\n");
                }
                else
                {
                    SetProgress(e.ProgressPercentage, e.UserState as string ?? "");
                }
            };
            worker.RunWorkerCompleted += (s, e) =>
            {
                if (e.Error != null)
                {
                    _log.AppendText("ERROR  " + e.Error.Message + "\r\n");
                    MessageBox.Show(this, e.Error.Message, "Block Setup");
                    ShowStep(1);
                    return;
                }
                SetProgress(100, "Complete");
                ShowStep(3);
            };
            worker.RunWorkerAsync(job);
        }

        void SetProgress(int pct, string label)
        {
            _progressPct.Text = pct + "%";
            _progressLabel.Text = label;
            _progressFill.Width = Math.Max(0, _progressTrack.Width * pct / 100);
        }

        static bool IsVulkanViewer(string viewerDir)
        {
            // Aurora is a Vulkan-native SL viewer; OpenGL injector never hooks the world.
            var name = Path.GetFileName(viewerDir) ?? "";
            if (name.IndexOf("Aurora", StringComparison.OrdinalIgnoreCase) >= 0) return true;
            if (File.Exists(Path.Combine(viewerDir, "aurora-viewer.exe"))) return true;
            return false;
        }

        string VulkanLayerDll()
        {
            foreach (var rel in new[] {
                Path.Combine("build-win", "runtime", "vulkan", "block.dll"),
                Path.Combine("build-win", "src", "hooks", "block.dll")
            })
            {
                var p = Path.Combine(_repoRoot, rel);
                if (File.Exists(p)) return p;
            }
            return Path.Combine(_repoRoot, "build-win", "runtime", "vulkan", "block.dll");
        }

        IEnumerable<string> InstallVulkan(string viewerDir)
        {
            var layerDll = VulkanLayerDll();
            if (!File.Exists(layerDll))
                throw new Exception("Missing Vulkan layer. Build with tools\\build_windows.bat then tools\\stage_block_runtime.ps1");

            var vkDir = Path.Combine(viewerDir, "block-vulkan");
            Directory.CreateDirectory(vkDir);
            File.Copy(layerDll, Path.Combine(vkDir, "block.dll"), true);
            yield return "Copied block.dll → block-vulkan\\";

            var json = @"{
  ""file_format_version"": ""1.0.0"",
  ""layer"": {
    ""name"": ""VK_LAYER_BLOCK_post_processing_64"",
    ""type"": ""GLOBAL"",
    ""library_path"": ""block.dll"",
    ""api_version"": ""1.3.0"",
    ""implementation_version"": ""1"",
    ""description"": ""Block post-processing (Vulkan)"",
    ""functions"": {
      ""vkGetInstanceProcAddr"": ""vkShade_GetInstanceProcAddr"",
      ""vkGetDeviceProcAddr"": ""vkShade_GetDeviceProcAddr""
    },
    ""enable_environment"": { ""ENABLE_BLOCK"": ""1"" },
    ""disable_environment"": { ""DISABLE_BLOCK"": ""1"" }
  }
}";
            File.WriteAllText(Path.Combine(vkDir, "Block_x86_64.json"), json);
            var iniSrc = Path.Combine(_repoRoot, "config", "Block.ini");
            if (File.Exists(iniSrc))
                File.Copy(iniSrc, Path.Combine(vkDir, "Block.ini"), true);

            // Strip unused OpenGL injector if a previous Setup left one
            var gl = Path.Combine(viewerDir, "opengl32.dll");
            if (File.Exists(gl))
            {
                string glMsg;
                try
                {
                    File.Move(gl, gl + ".opengl-unused");
                    glMsg = "Moved opengl32.dll aside (Aurora is Vulkan — OpenGL injector cannot hook the world)";
                }
                catch (Exception ex)
                {
                    glMsg = "WARN  could not move opengl32.dll: " + ex.Message;
                }
                yield return glMsg;
            }

            var bat = "@echo off\r\nsetlocal\r\ncd /d \"%~dp0\"\r\n" +
                "set \"ENABLE_BLOCK=1\"\r\nset \"DISABLE_BLOCK=\"\r\n" +
                "set \"VK_LAYER_PATH=%~dp0block-vulkan\"\r\n" +
                "set \"VK_INSTANCE_LAYERS=VK_LAYER_BLOCK_post_processing_64\"\r\n" +
                "set \"AURORA_VULKAN_LAYERS=keep\"\r\n" +
                "echo Starting with Block Vulkan layer (Shift+B overlay)\r\n" +
                "start \"\" \"%~dp0aurora-viewer.exe\" %*\r\n";
            File.WriteAllText(Path.Combine(viewerDir, "Launch Aurora with Block.bat"), bat);
            yield return "Wrote Launch Aurora with Block.bat";

            // Preferences › Graphics › allow overlays
            string settingsMsg;
            try
            {
                var settings = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "Aurora", "AuroraViewer", "config", "settings.json");
                if (File.Exists(settings))
                {
                    var text = File.ReadAllText(settings);
                    if (text.IndexOf("\"allow_overlays\": false", StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        text = System.Text.RegularExpressions.Regex.Replace(
                            text, "\"allow_overlays\"\\s*:\\s*false", "\"allow_overlays\": true",
                            System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                        File.WriteAllText(settings, text);
                        settingsMsg = "Enabled allow_overlays in Aurora settings.json";
                    }
                    else
                        settingsMsg = "Aurora settings: allow_overlays already on (or key missing)";
                }
                else
                    settingsMsg = "WARN  Aurora settings.json not found — turn on Preferences › Graphics › allow overlays";
            }
            catch (Exception ex)
            {
                settingsMsg = "WARN  could not update Aurora settings: " + ex.Message;
            }
            yield return settingsMsg;

            yield return "Launch via \"Launch Aurora with Block.bat\" (not the bare .exe shortcut).";
        }

        IEnumerable<string> Install(string viewerDir, List<AddonPick> picks)
        {
            yield return "Installing → " + viewerDir;

            if (IsVulkanViewer(viewerDir))
            {
                yield return "Detected Vulkan viewer (Aurora) — installing block.dll layer, not opengl32.dll";
                foreach (var line in InstallVulkan(viewerDir))
                    yield return line;
                yield return "Done.";
                yield break;
            }

            var dll = InjectorDll();
            var dest = Path.Combine(viewerDir, "opengl32.dll");

            // Black Dragon / GShade often use a symlink opengl32.dll → GShade64.dll.
            // File.Copy onto a symlink does not install Block — delete the reparse point first.
            string replaceMsg = null;
            if (File.Exists(dest))
            {
                Exception replaceErr = null;
                try
                {
                    var attrs = File.GetAttributes(dest);
                    bool isLink = (attrs & FileAttributes.ReparsePoint) != 0;
                    if (isLink)
                    {
                        File.Delete(dest);
                        replaceMsg = "Removed GShade symlink opengl32.dll → installing real Block DLL";
                    }
                    else
                    {
                        var bak = dest + ".pre-block-" + DateTime.Now.ToString("yyyyMMdd-HHmmss");
                        File.Copy(dest, bak, true);
                        File.Delete(dest);
                        replaceMsg = "Backed up existing opengl32.dll";
                    }
                }
                catch (Exception ex)
                {
                    replaceErr = ex;
                    try { File.Delete(dest); } catch { }
                }
                if (replaceMsg != null) yield return replaceMsg;
                if (replaceErr != null) yield return "WARN  replace opengl32: " + replaceErr.Message;
            }

            File.Copy(dll, dest, true);
            var len = new FileInfo(dest).Length;
            if (len < 1000000)
                throw new Exception("Injector copy failed (opengl32.dll is " + len + " bytes). Is the path a symlink you cannot replace?");
            yield return "Copied injector (" + len + " bytes)";

            var psi = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                Arguments = "-NoProfile -ExecutionPolicy Bypass -File \"" + _configurePs1 + "\" -ViewerDir \"" + viewerDir + "\"",
                UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, CreateNoWindow = true
            };
            using (var p = Process.Start(psi))
            {
                var stdout = p.StandardOutput.ReadToEnd();
                var stderr = p.StandardError.ReadToEnd();
                p.WaitForExit();
                foreach (var line in (stdout + "\n" + stderr).Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
                    yield return line;
                if (p.ExitCode != 0) throw new Exception("configure failed: " + p.ExitCode);
            }

            // Selected catalog addons → viewer gshade-addons (OpenGL-safe; no DLSS/renodx)
            foreach (var line in ApplySelectedAddons(viewerDir, picks))
                yield return line;

            yield return "Done.";
        }

        string InjectorDll()
        {
            foreach (var rel in new[] {
                Path.Combine("build-win", "runtime", "opengl", "opengl32.dll"),
                Path.Combine("build-win", "hook", "opengl32.dll")
            })
            {
                var p = Path.Combine(_repoRoot, rel);
                if (File.Exists(p)) return p;
            }
            return Path.Combine(_repoRoot, "build-win", "runtime", "opengl", "opengl32.dll");
        }

        static List<ViewerInfo> FindViewers()
        {
            // Folder-name globs under common install roots (OpenGL SL viewers + Aurora Vulkan).
            var patterns = new[]
            {
                "Firestorm*", "SecondLife*", "Second Life*",
                "Alchemy*", "Black Dragon*", "BlackDragon*",
                "Cool VL Viewer*", "CoolVL*", "Singularity*", "Catznip*",
                "Kokua*", "Dolphin*", "Dayturn*", "Niran*",
                "LL*", "Linden*",
                "Aurora*", "AuroraViewer*", "Aurora Viewer*"
            };
            var roots = new List<string>();
            var pf = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
            var x86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
            var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            var roaming = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
            var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            var downloads = Path.Combine(home ?? "", "Downloads");
            var desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            if (!string.IsNullOrEmpty(pf)) roots.Add(pf);
            if (!string.IsNullOrEmpty(x86) && !roots.Contains(x86)) roots.Add(x86);
            if (!string.IsNullOrEmpty(local))
            {
                roots.Add(local);
                roots.Add(Path.Combine(local, "Programs"));
                roots.Add(Path.Combine(local, "Aurora"));
                // Alchemy / Firestorm sometimes install under Local\*\viewer
                try
                {
                    foreach (var d in new DirectoryInfo(local).GetDirectories("*Alchemy*"))
                        roots.Add(d.FullName);
                    foreach (var d in new DirectoryInfo(local).GetDirectories("*Firestorm*"))
                        roots.Add(d.FullName);
                    foreach (var d in new DirectoryInfo(local).GetDirectories("*Black*Dragon*"))
                        roots.Add(d.FullName);
                }
                catch { }
            }
            if (!string.IsNullOrEmpty(roaming))
            {
                try
                {
                    foreach (var d in new DirectoryInfo(roaming).GetDirectories("*Alchemy*"))
                        roots.Add(d.FullName);
                }
                catch { }
            }
            // Portable / extracted packs under Downloads / Desktop
            foreach (var portableRoot in new[] { downloads, desktop })
            {
                if (!Directory.Exists(portableRoot)) continue;
                roots.Add(portableRoot);
                try
                {
                    foreach (var pat in new[] { "Aurora*", "Firestorm*", "Alchemy*", "Black*Dragon*", "Cool*", "Singularity*", "Kokua*", "Dolphin*" })
                    {
                        foreach (var d in new DirectoryInfo(portableRoot).GetDirectories(pat))
                            roots.Add(d.FullName);
                    }
                }
                catch { }
            }
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var found = new List<ViewerInfo>();
            foreach (var root in roots)
            {
                if (!Directory.Exists(root)) continue;
                foreach (var pat in patterns)
                {
                    DirectoryInfo[] dirs;
                    try { dirs = new DirectoryInfo(root).GetDirectories(pat); } catch { continue; }
                    foreach (var d in dirs)
                    {
                        if (!LooksLikeViewerDir(d)) continue;
                        if (!seen.Add(d.FullName)) continue;
                        found.Add(new ViewerInfo { Name = d.Name, Path = d.FullName });
                    }
                }
                // Root itself may be a portable viewer folder
                try
                {
                    var self = new DirectoryInfo(root);
                    if (LooksLikeViewerDir(self) && IsKnownViewerName(self.Name) && seen.Add(self.FullName))
                        found.Add(new ViewerInfo { Name = self.Name, Path = self.FullName });
                }
                catch { }
            }
            return found.OrderBy(ViewerSortKey).ThenBy(v => v.Name).ToList();
        }

        static string ViewerSortKey(ViewerInfo v)
        {
            var n = v.Name.ToLowerInvariant();
            if (n.Contains("firestorm")) return "0";
            if (n.Contains("alchemy")) return "1";
            if (n.Contains("black") && n.Contains("dragon")) return "2";
            if (n.Contains("second")) return "3";
            if (n.Contains("cool")) return "4";
            if (n.Contains("singularity")) return "5";
            if (n.Contains("catznip")) return "6";
            if (n.Contains("kokua")) return "7";
            if (n.Contains("aurora")) return "9";
            return "8";
        }

        static bool IsKnownViewerName(string name)
        {
            var n = (name ?? "").ToLowerInvariant();
            return n.Contains("firestorm") || n.Contains("secondlife") || n.Contains("second life") ||
                   n.Contains("alchemy") || n.Contains("black dragon") || n.Contains("blackdragon") ||
                   n.Contains("cool vl") || n.Contains("coolvl") || n.Contains("singularity") ||
                   n.Contains("catznip") || n.Contains("kokua") || n.Contains("dolphin") ||
                   n.Contains("dayturn") || n.Contains("niran") || n.Contains("aurora");
        }

        static bool LooksLikeViewerDir(DirectoryInfo d)
        {
            // Skip cache-only AppData folders (no viewer binary)
            try
            {
                foreach (var exe in d.GetFiles("*.exe"))
                {
                    var n = exe.Name.ToLowerInvariant();
                    if (n.Contains("plugin") || n.Contains("crash") || n.Contains("updater") || n.Contains("dullahan") ||
                        n.Contains("setup") || n.Contains("uninstall") || n.Contains("blocksetup"))
                        continue;
                    if (exe.Length > 2 * 1024 * 1024) return true;
                    if (n.Contains("firestorm") || n.Contains("secondlife") || n.Contains("aurora") ||
                        n.Contains("alchemy") || n.Contains("singularity") || n.Contains("dragon") ||
                        n.Contains("catznip") || n.Contains("kokua") || n.Contains("dolphin") ||
                        n.Contains("dayturn") || n.Contains("cool") || n.Contains("viewer") ||
                        n.Contains("slviewer") || n.Contains("llviewer"))
                        return true;
                }
                // Program Files installs often keep the exe with the folder name
                if (d.FullName.IndexOf("Program Files", StringComparison.OrdinalIgnoreCase) >= 0)
                    return d.GetFiles("*.exe").Length > 0 || File.Exists(Path.Combine(d.FullName, "GShade.ini"))
                        || File.Exists(Path.Combine(d.FullName, "opengl32.dll"))
                        || File.Exists(Path.Combine(d.FullName, "ReShade.ini"));
            }
            catch { }
            return false;
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

        Button MakeBtn(string text, bool primary)
        {
            var b = new Button
            {
                Text = text, Size = new Size(90, 32), FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9f, FontStyle.Bold), Cursor = Cursors.Hand,
                BackColor = primary ? Brand.Signal : Brand.Surface2,
                ForeColor = primary ? Brand.Surface0 : Brand.Ink
            };
            b.FlatAppearance.BorderColor = primary ? Brand.Signal : Brand.Line;
            b.FlatAppearance.BorderSize = 1;
            return b;
        }

        Button MakeChromeButton(string text, int x)
        {
            var b = new Button
            {
                Text = text, Location = new Point(x, 0), Size = new Size(46, 36),
                FlatStyle = FlatStyle.Flat, BackColor = Brand.Surface0, ForeColor = Brand.InkMuted,
                Font = new Font("Segoe UI", 9f), Cursor = Cursors.Hand
            };
            b.FlatAppearance.BorderSize = 0;
            b.FlatAppearance.MouseOverBackColor = Brand.Surface2;
            return b;
        }

        void NativeDrag()
        {
            Capture = false;
            Message m = Message.Create(Handle, 0xA1, new IntPtr(2), IntPtr.Zero);
            WndProc(ref m);
        }

        sealed class ViewerInfo { public string Name; public string Path; }
    }
}
