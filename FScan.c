#include <iostream>
#include <fstream>
#include "TSystem.h"
#include "TSystemDirectory.h"

void CheckTrimFiles() {
    const char* dirPath = "trimfiles";
    TSystemDirectory dir("trimfiles", dirPath);
    TList* files = dir.GetListOfFiles();
    
    if (!files) {
        std::cerr << "Error: Could not open directory: " << dirPath << std::endl;
        return;
    }

    int electronCount = 0;
    int holeCount = 0;
    bool openErrors = false;

    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Ignore directories
        if (file->IsDirectory()) continue;
        
        // Check electron files
        if (fileName.EndsWith("_electrons.txt")) {
            electronCount++;
            ifstream f_test(Form("%s/%s", dirPath, fileName.Data()));
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open file: " << fileName << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
        // Check hole files
        else if (fileName.EndsWith("_holes.txt")) {
            holeCount++;
            ifstream f_test(Form("%s/%s", dirPath, fileName.Data()));
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open file: " << fileName << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
    }

    // Check counts and set flags
    bool flagElectronCount = (electronCount != 8);
    bool flagHoleCount = (holeCount != 8);
    bool flagOpenErrors = openErrors;

    // Report results
    std::cout << "\n===== Validation Report =====" << std::endl;
    std::cout << "Electron files found: " << electronCount << " (expected 8) - "
              << (flagElectronCount ? "FAIL" : "OK") << std::endl;
    std::cout << "Hole files found: " << holeCount << " (expected 8) - "
              << (flagHoleCount ? "FAIL" : "OK") << std::endl;
    std::cout << "File access errors: " << (flagOpenErrors ? "YES" : "NONE") << std::endl;

    // Set final flags
    if (flagElectronCount || flagHoleCount || flagOpenErrors) {
        std::cout << "\nALERT: Validation failed!" << std::endl;
    } else {
        std::cout << "\nSUCCESS: All files validated" << std::endl;
    }
}