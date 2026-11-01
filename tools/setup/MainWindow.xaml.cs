using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using WinForms = System.Windows.Forms;

namespace Block.Setup
{
    public partial class MainWindow : Window
    {
        readonly ObservableCollection<ViewerRow> _viewers = new ObservableCollection<ViewerRow>();
        int _step; // 0 welcome, 1 options, 2 install, 3 done
        readonly string[] _stepNames = { "Welcome", "Options", "Install", "Done" };

        public MainWindow()
        {
            InitializeComponent();
            ViewerList.ItemsSource = _viewers;
            StatusHint.Text = System.IO.File.Exists(InstallerLogic.InjectorDll()) ? "Injector ready" : "Build injector first";
            StatusHint.Foreground = (Brush)FindResource(System.IO.File.Exists(InstallerLogic.InjectorDll()) ? "Ok" : "InkMuted");
            BuildStepNav();
            ShowStep(0);
            RefreshViewers();
        }

        void BuildStepNav()
        {
            StepNav.Children.Clear();
            for (int i = 0; i < _stepNames.Length; i++)
            {
                var row = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 0, 0, 14) };
                var badge = new Border
                {
                    Width = 20, Height = 20, CornerRadius = new CornerRadius(10),
                    Child = new TextBlock
                    {
                        Text = i < _step ? "✓" : (i + 1).ToString(),
                        FontSize = 11, FontFamily = new FontFamily("Consolas"),
                        HorizontalAlignment = HorizontalAlignment.Center,
                        VerticalAlignment = VerticalAlignment.Center
                    }
                };
                var label = new TextBlock
                {
                    Text = _stepNames[i], FontSize = 12, Margin = new Thickness(8, 0, 0, 0),
                    VerticalAlignment = VerticalAlignment.Center
                };
                StyleStep(badge, label, i);
                row.Children.Add(badge);
                row.Children.Add(label);
                StepNav.Children.Add(row);
            }
        }

        void StyleStep(Border badge, TextBlock label, int index)
        {
            var signal = (Brush)FindResource("Signal");
            var surface2 = (Brush)FindResource("Surface2");
            var ink = (Brush)FindResource("Ink");
            var muted = (Brush)FindResource("InkMuted");
            var dark = new SolidColorBrush(Color.FromRgb(0x0B, 0x0C, 0x0E));

            bool active = index == _step;
            bool done = index < _step;
            badge.Background = (active || done) ? signal : surface2;
            ((TextBlock)badge.Child).Foreground = (active || done) ? dark : muted;
            label.Foreground = active ? ink : muted;
            label.FontWeight = active ? FontWeights.SemiBold : FontWeights.Normal;
        }

        void ShowStep(int step)
        {
            _step = step;
            PageWelcome.Visibility = step == 0 ? Visibility.Visible : Visibility.Collapsed;
            PageOptions.Visibility = step == 1 ? Visibility.Visible : Visibility.Collapsed;
            PageInstall.Visibility = step == 2 ? Visibility.Visible : Visibility.Collapsed;
            PageDone.Visibility = step == 3 ? Visibility.Visible : Visibility.Collapsed;
            BuildStepNav();

            BtnSecondary.Visibility = step == 3 ? Visibility.Collapsed : Visibility.Visible;
            BtnSecondary.Content = step == 0 ? "Cancel" : (step == 2 ? "Cancel" : "Back");
            BtnSecondary.IsEnabled = step != 2;
            BtnPrimary.Content = step == 0 ? "Next" : step == 1 ? "Install" : step == 2 ? "…" : "Finish";
            BtnPrimary.IsEnabled = step != 2;
            if (step == 0) BtnPrimary.IsEnabled = AgreeBox.IsChecked == true;
        }

        void RefreshViewers()
        {
            _viewers.Clear();
            foreach (var v in InstallerLogic.FindViewers())
            {
                _viewers.Add(new ViewerRow
                {
                    Name = v.Name,
                    Path = v.Path,
                    Installed = v.Installed,
                    IsSelected = v.Name.IndexOf("Firestorm-Release", StringComparison.OrdinalIgnoreCase) >= 0
                });
            }
        }

        void AppendLog(string line)
        {
            if (LogText.Text.Length > 0) LogText.Text += "\n";
            var colorTag = line.StartsWith("ERROR", StringComparison.OrdinalIgnoreCase) ? "err" :
                line.StartsWith("done", StringComparison.OrdinalIgnoreCase) ? "done" : "…";
            LogText.Text += colorTag.PadRight(4) + "  " + line;
            LogScroll.ScrollToEnd();
        }

        void SetProgress(double pct, string label)
        {
            ProgressPct.Text = ((int)pct) + "%";
            ProgressLabel.Text = label;
            var max = ((FrameworkElement)ProgressFill.Parent).ActualWidth;
            if (max <= 0) max = 480;
            ProgressFill.Width = Math.Max(0, max * pct / 100.0);
        }

        void TitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left) DragMove();
        }

        void Minimize_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
        void Close_Click(object sender, RoutedEventArgs e) => Close();
        void AgreeBox_Changed(object sender, RoutedEventArgs e)
        {
            if (_step == 0) BtnPrimary.IsEnabled = AgreeBox.IsChecked == true;
        }

        void Secondary_Click(object sender, RoutedEventArgs e)
        {
            if (_step == 0) Close();
            else if (_step == 1) ShowStep(0);
            else if (_step == 2) ShowStep(1);
        }

        async void Primary_Click(object sender, RoutedEventArgs e)
        {
            if (_step == 0)
            {
                if (AgreeBox.IsChecked != true) { MessageBox.Show(this, "Please agree to continue.", "Block Setup"); return; }
                ShowStep(1);
                return;
            }
            if (_step == 1)
            {
                var targets = _viewers.Where(v => v.IsSelected).Select(v => v.Path).ToList();
                if (targets.Count == 0) { MessageBox.Show(this, "Select at least one viewer.", "Block Setup"); return; }
                if (!System.IO.File.Exists(InstallerLogic.InjectorDll()))
                {
                    MessageBox.Show(this, "Build the injector first:\ntools\\build_reshade_block.ps1", "Block Setup");
                    return;
                }
                ShowStep(2);
                LogText.Text = "";
                await RunInstallAsync(targets);
                return;
            }
            if (_step == 3) Close();
        }

        async Task RunInstallAsync(System.Collections.Generic.List<string> targets)
        {
            try
            {
                int n = targets.Count;
                for (int i = 0; i < n; i++)
                {
                    var path = targets[i];
                    SetProgress((i * 100.0) / n, "Installing " + System.IO.Path.GetFileName(path));
                    await Task.Run(() =>
                    {
                        foreach (var line in InstallerLogic.Install(path))
                            Dispatcher.Invoke(() => AppendLog(line));
                    });
                    SetProgress(((i + 1) * 100.0) / n, "Installed " + System.IO.Path.GetFileName(path));
                }
                SetProgress(100, "Complete");
                AppendLog("done  All selected viewers configured.");
                await Task.Delay(350);
                ShowStep(3);
            }
            catch (Exception ex)
            {
                AppendLog("ERROR " + ex.Message);
                BtnSecondary.IsEnabled = true;
                BtnSecondary.Content = "Back";
                BtnPrimary.IsEnabled = true;
                BtnPrimary.Content = "Back";
                MessageBox.Show(this, ex.Message, "Block Setup");
                ShowStep(1);
            }
        }

        void Rescan_Click(object sender, RoutedEventArgs e) => RefreshViewers();

        void Browse_Click(object sender, RoutedEventArgs e)
        {
            using (var dlg = new WinForms.FolderBrowserDialog())
            {
                dlg.Description = "Select viewer install folder";
                if (dlg.ShowDialog() != WinForms.DialogResult.OK) return;
                _viewers.Add(new ViewerRow
                {
                    Name = System.IO.Path.GetFileName(dlg.SelectedPath),
                    Path = dlg.SelectedPath,
                    Installed = System.IO.File.Exists(System.IO.Path.Combine(dlg.SelectedPath, "opengl32.dll")),
                    IsSelected = true
                });
            }
        }

        void ViewerRow_Click(object sender, MouseButtonEventArgs e)
        {
            if (e.OriginalSource is CheckBox) return;
            var row = (sender as Border)?.Tag as ViewerRow;
            if (row != null) row.IsSelected = !row.IsSelected;
        }
    }

    public sealed class ViewerRow : INotifyPropertyChanged
    {
        bool _selected;
        public string Name { get; set; }
        public string Path { get; set; }
        public bool Installed { get; set; }
        public string Status => Installed ? "Installed" : "Ready";
        public bool IsSelected
        {
            get => _selected;
            set { if (_selected == value) return; _selected = value; OnPropertyChanged(); }
        }
        public event PropertyChangedEventHandler PropertyChanged;
        void OnPropertyChanged([CallerMemberName] string name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}
