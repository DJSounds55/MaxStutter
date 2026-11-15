# Max Stutter – VST3/AU-Effekt für DJ.Studio

Echtzeit-Stutter im Max-Mix-Stil. Stutter, Roll, Gate-Chop, Tape-Stop, Backspin.

## Bauen (einmalig)
Voraussetzungen: CMake >= 3.22, Git, C++-Compiler
(Windows: Visual Studio 2022 mit "Desktopentwicklung mit C++"; macOS: Xcode Command Line Tools).

Windows (PowerShell):
    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release
macOS:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Ergebnis: build/MaxStutter_artefacts/Release/VST3/Max Stutter.vst3 (macOS zusätzlich AU).
Windows: den Ordner "Max Stutter.vst3" nach C:\Program Files\Common Files\VST3 kopieren.
macOS: wird automatisch nach ~/Library/Audio/Plug-Ins/VST3 (und /Components) kopiert.

## In DJ.Studio
1. DJ.Studio öffnen, im Effects-Tab "Rescan" klicken, dann "+ Add VST" -> Max Stutter.
2. Parameter "Stutter" ist automatisierbar: Automationslinie auf "an" setzen, genau auf den Beat / den Wortanfang.
3. Modus, Teilung, Gate, Decay, Tonhöhe usw. einstellen oder ebenfalls automatisieren.

## Bedienung
- Stutter (Schalter): beim Einschalten wird ab genau diesem Moment der erste Slice (Länge = Teilung) durchgespielt
  und aufgenommen, danach wiederholt (D-D-D-Day). Ausschalten = normal weiter.
- Roll: Teilung halbiert sich pro Takt (bis 1/32).
- Echo (letzter Slice): wiederholt sofort den Slice, der direkt VOR dem Einschalten lief (Day-day-day).
- Gate-Chop: Eingangssignal wird im Raster zerhackt.
- Tape-Stop / Backspin: Länge über "Länge Tape-Stop/Backspin".
- BPM: "BPM vom Host" an = Tempo aus DJ.Studio, falls geliefert; sonst "BPM manuell" verwenden.

## Ohne Compiler: in der Cloud bauen lassen (GitHub)
1. Kostenloses GitHub-Konto, neues Repository anlegen, den ganzen Inhalt dieses Ordners hochladen
   (inkl. .github/workflows/build.yml – falls der versteckte Ordner fehlt: "Add file > Create new file",
   Pfad .github/workflows/build.yml eintippen und den Inhalt einfügen).
2. Reiter "Actions" -> "Build VST3" -> "Run workflow". Nach einigen Minuten unter "Artifacts"
   MaxStutter-windows-latest bzw. MaxStutter-macos-latest herunterladen.
3. Darin liegt ...\VST3\Max Stutter.vst3 -> in den VST3-Ordner kopieren (Windows: C:\Program Files\Common Files\VST3).
   macOS (unsigniert): einmal im Terminal  xattr -dr com.apple.quarantine "Max Stutter.vst3"
