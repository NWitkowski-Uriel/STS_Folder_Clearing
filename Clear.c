// runClear.C
#include <TSystem.h>
#include <TString.h>
#include <TSystemDirectory.h>
#include <TList.h>
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
    TSystemFile *file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString name = file->GetName();
        if (file->IsDirectory() && name != "." && name != ".." && !name.BeginsWith(".")) {
            dirNames.push_back(name);
        }
    }
    delete files;

    if (dirNames.empty()) {
        std::cout << "No directories found!" << std::endl;
        return;
    }

    // Sprawdź dostępność programów
    bool useExecutable = !gSystem->AccessPathName("CheckTrimFiles");
    bool useSource = !gSystem->AccessPathName("CheckTrimFiles.c");
    
    if (!useExecutable && !useSource) {
        std::cerr << "Error: No CheckTrimFiles executable or source found!" << std::endl;
        return;
    }

    // Uruchom program dla każdego folderu
    for (auto& dir : dirNames) {
        TString command;
        
        if (useExecutable) {
            command = TString::Format("./CheckTrimFiles \"%s\"", dir.Data());
        } else {
            // Dynamiczna kompilacja i uruchomienie
            command = TString::Format(".x CheckTrimFiles.c+(\"%s\")", dir.Data());
        }

        std::cout << "Executing: " << command << std::endl;
        int status = gROOT->ProcessLine(command);
        
        if (status != 0) {
            std::cerr << "Error processing directory: " << dir << std::endl;
        }
    }
}