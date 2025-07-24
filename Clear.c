// runClearAndPscan.C
#include <TSystem.h>
#include <TString.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TInterpreter.h>
#include <iostream>
#include <vector>

void Clear() {
    // Pobierz listę folderów w bieżącym katalogu
    TSystemDirectory dir(".", ".");
    TList *files = dir.GetListOfFiles();
    if (!files) {
        std::cerr << "Error: Could not get file list!" << std::endl;
        return;
    }

    std::vector<TString> dirNames;
    TIter next(files);
    TSystemFile *file;
    TObject *obj;
    while ((obj = next())) {
        file = dynamic_cast<TSystemFile*>(obj);
        if (!file) continue;
        
        TString name = file->GetName();
        if (name == "." || name == ".." || name.BeginsWith(".")) continue;

        if (file->IsDirectory()) {
            dirNames.push_back(name);
        }
    }
    delete files;

    if (dirNames.empty()) {
        std::cout << "No directories found!" << std::endl;
        return;
    }

    // Sprawdź dostępność programów
    bool useExecutableTrim = (gSystem->AccessPathName("CheckTrimFiles") == 0);
    bool useSourceTrim = (gSystem->AccessPathName("CheckTrimFiles.c") == 0);
    bool trimAvailable = useExecutableTrim || useSourceTrim;

    bool useExecutablePscan = (gSystem->AccessPathName("CheckPscanFiles") == 0);
    bool useSourcePscan = (gSystem->AccessPathName("CheckPscanFiles.c") == 0);
    bool pscanAvailable = useExecutablePscan || useSourcePscan;
    
    // DODANE: Sprawdzenie dostępności CheckConnFiles
    bool useExecutableConn = (gSystem->AccessPathName("CheckConnFiles") == 0);
    bool useSourceConn = (gSystem->AccessPathName("CheckConnFiles.c") == 0);
    bool connAvailable = useExecutableConn || useSourceConn;

    // Aktualizacja warunku błędów
    if (!trimAvailable && !pscanAvailable && !connAvailable) {
        std::cerr << "Error: No valid programs found!" << std::endl;
        return;
    }

    // Uruchom programy dla każdego folderu
    for (auto& dir : dirNames) {
        // 1. Uruchom CheckTrimFiles
        if (trimAvailable) {
            TString trimCommand;
            
            if (useExecutableTrim) {
                trimCommand = TString::Format("./CheckTrimFiles \"%s\"", dir.Data());
            } else {
                trimCommand = TString::Format(".x CheckTrimFiles.c+(\"%s\")", dir.Data());
            }

            std::cout << "\n[1/3] Executing CheckTrimFiles: " << trimCommand << std::endl;
            int trimStatus = gInterpreter->ProcessLine(trimCommand);
            
            if (trimStatus != 0) {
                std::cerr << "Error processing directory with CheckTrimFiles: " << dir << std::endl;
            }
        }

        // 2. Uruchom CheckPscanFiles
        if (pscanAvailable) {
            TString pscanCommand;
            
            if (useExecutablePscan) {
                pscanCommand = TString::Format("./CheckPscanFiles \"%s\"", dir.Data());
            } else {
                pscanCommand = TString::Format(".x CheckPscanFiles.c+(\"%s\")", dir.Data());
            }

            std::cout << "[2/3] Executing CheckPscanFiles: " << pscanCommand << std::endl;
            int pscanStatus = gInterpreter->ProcessLine(pscanCommand);
            
            if (pscanStatus != 0) {
                std::cerr << "Error processing directory with CheckPscanFiles: " << dir << std::endl;
            }
        }
        
        // DODANE: 3. Uruchom CheckConnFiles
        if (connAvailable) {
            TString connCommand;
            
            if (useExecutableConn) {
                connCommand = TString::Format("./CheckConnFiles \"%s\"", dir.Data());
            } else {
                connCommand = TString::Format(".x CheckConnFiles.c+(\"%s\")", dir.Data());
            }

            std::cout << "[3/3] Executing CheckConnFiles: " << connCommand << std::endl;
            int connStatus = gInterpreter->ProcessLine(connCommand);
            
            if (connStatus != 0) {
                std::cerr << "Error processing directory with CheckConnFiles: " << dir << std::endl;
            }
        }
    }
}