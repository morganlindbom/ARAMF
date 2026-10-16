# ARAMF Worker Station – Windows-pilot

[Ladda ner Windows-programmet (ZIP)](https://github.com/morganlindbom/ARAMF/releases/download/worker-station-pilot-v0.1.0/ARAMF-Worker-Station-Windows.zip) · [Release och kontrollsumma](https://github.com/morganlindbom/ARAMF/releases/tag/worker-station-pilot-v0.1.0)

Ett fristående C++17/Qt 6-program för att testa en AI-arbetare på en extra Windows-dator. Packa upp hela ZIP-filen och starta `AramfWorkerStation.exe`. Logga in med ChatGPT och använd provuppdraget.

- [Instruktioner på svenska](LAS-MIG.txt)
- [Källkod och CMake-projekt](source/)
- [Provuppdrag](Provuppdrag.aramf-job.json)
- [Testresultat från pilotpaketet](TESTRESULTAT.txt)

Källkoden är en oförändrad kopia från det publicerade pilotpaketet v0.1.0. Piloten byggs separat från ARAMF. Automatisk export från medlemssidan och oberoende certifiering ingår ännu inte.

## Bygga

Kräver C++17-kompilator, CMake, Ninja och Qt 6 Widgets.

```powershell
cmake -S worker-station/source -B build-worker-station -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt>
cmake --build build-worker-station
```

Det färdiga ZIP-paketet innehåller även Codex och runtimebibliotek. Se LAS-MIG.txt för paketets innehåll och licenser.
