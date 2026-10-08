# mdwriter 📝

> A fork of [**ghostwriter**](https://invent.kde.org/office/ghostwriter) — the distraction‑free Markdown
> editor — rebranded, reskinned for **VS Code** users, and packaged as its own **`.deb`** so it can be
> installed **alongside** the stock `ghostwriter` package.

| Attribute | Detail |
|---|---|
| **Upstream** | [ghostwriter](https://invent.kde.org/office/ghostwriter) ([GitHub mirror](https://github.com/wereturtle/ghostwriter)) |
| **Fork base** | tag `v23.08.5` (Qt5 / KF5) |
| **Package / binary** | `mdwriter` (coexists with stock `ghostwriter`) |
| **License** | GPL‑3.0‑or‑later (inherited from ghostwriter) |

---

## 🔀 mdwriter vs original ghostwriter

mdwriter is the **same editor under the hood** — every upstream feature is retained — but it is
**rebranded**, **reskinned for VS Code users**, and adds a few workflow extras.

| Area | Original **ghostwriter** | **mdwriter** (this fork) |
|---|---|---|
| 🏷️ Identity | app / binary / IDs = `ghostwriter` | `mdwriter` with its own desktop + AppStream id; **installs alongside** ghostwriter |
| 🖥️ Default layout | source editor **+** live preview, side‑by‑side | **preview‑first** — opens on the rendered preview; `Ctrl+Shift+E` toggles the source editor |
| 📐 Live‑preview width | capped at **50 %** of the pane | **full width** when the editor is hidden |
| 🔤 Fonts (first run) | `DejaVu Sans Mono` / `Noto Serif` | VS Code–matched: **`Liberation Mono`** (code) / **`Ubuntu`** (preview text) |
| 🎨 Theme import | native `.json` themes + theme editor | ➕ **Import from VS Code…** — built‑in **and** extension themes |
| 🗂️ File browser | recent‑files list only | ➕ **Files sidebar tab** that **follows the current document**, with ↑/⌂ navigation and double‑click open |
| 🔄 Reload from disk | menu item, **no shortcut** | menu item **+ `Ctrl+R`** |
| 🖼️ App icon | ghostwriter quill | original **“M↓”** mark |
| ⚙️ Base / toolchain | upstream `HEAD` (Qt6 + KF6) | pinned **`v23.08.5`** (Qt5 + KF5) |

### ✅ Upstream features retained (unchanged)

Distraction‑free Markdown editing · live HTML preview (cmark‑gfm) · **bold / italic / strikethrough**,
lists, blockquotes, code fences, task lists, links & images · **Focus mode**, **Hemingway mode**,
full‑screen · theme manager + theme editor + dark mode · **Outline**, **Session statistics**,
**Document statistics**, **Cheat Sheet** sidebar tabs · live word count & reading time · find & replace ·
spell check (Sonnet/Hunspell) · auto‑save & backups · recent files & session restore ·
HTML / ODT / PDF export and Pandoc / MultiMarkdown / cmark integration · translations.

---

## 🚀 Build

Requires Qt 5.15+ and KF5 (≥ 5.90), CMake, and the usual KDE build tools.

```bash
sudo apt-get install -y build-essential cmake pkg-config gettext extra-cmake-modules \
  qtbase5-dev qtbase5-dev-tools qt5-qmake qttools5-dev qttools5-dev-tools \
  libqt5svg5-dev libqt5webchannel5-dev qtwebengine5-dev libkf5sonnet-dev \
  libkf5coreaddons-dev libkf5xmlgui-dev libkf5configwidgets-dev \
  libkf5widgetsaddons-dev libhunspell-dev debhelper devscripts fakeroot

# Build the .deb
dpkg-buildpackage -us -uc -b

# Install (coexists with stock ghostwriter)
sudo apt-get install -y ../mdwriter_*_amd64.deb
```

Or build directly with CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build -j"$(nproc)"
```

## 🧪 Verify

```bash
mdwriter                                # opens in preview-only mode
desktop-file-validate /usr/share/applications/io.github.mhsihar.mdwriter.desktop
```

Checklist: opens **preview‑only** · `Ctrl+Shift+E` toggles the source editor · `Ctrl+R` reloads from disk ·
theme list has `VS Code · …` entries · the **Files** sidebar tab follows the current document.

---

## 🙏 Credits & licence

`mdwriter` is a modified fork of **ghostwriter** by *Megan Conkle* and contributors. Upstream authors
and credits are preserved in the About dialog. Distributed under the **GNU GPL‑3.0‑or‑later**; the
corresponding source for any distributed binary is this repository.
