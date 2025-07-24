#include <iostream>
#include <fstream>
#include "TSystem.h"
#include "TSystemDirectory.h"

void CheckTrimFiles() {
    const char* dirPath = "trimfiles";
    
    // Sprawdź czy folder trimfiles istnieje
    if (gSystem->AccessPathName(dirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'trimfiles' does not exist in current path!" << std::endl;
        std::cerr << "Current path: " << gSystem->pwd() << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
        std::cout << "FLAG 3 (Folder exists):  1" << std::endl;
        std::cout << "\nSummary: [FOLDER MISSING]" << std::endl;
        return;
    }

    TSystemDirectory dir("trimfiles", dirPath);
    TList* files = dir.GetListOfFiles();
    
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << dirPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
        std::cout << "FLAG 3 (Folder access):  1" << std::endl;
        std::cout << "\nSummary: [DIRECTORY ACCESS ERROR]" << std::endl;
        return;
    }

    int electronCount = 0;
    int holeCount = 0;
    bool openErrors = false;

    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Ignore directories and special files
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        // Check electron files
        if (fileName.EndsWith("_electrons.txt")) {
            electronCount++;
            ifstream f_test(Form("%s/%s", dirPath, fileName.Data()));
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron file: " << fileName << std::endl;
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
                std::cerr << "Error: Cannot open hole file: " << fileName << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
    }

    // Calculate flags
    bool flag_electron_count = (electronCount != 8);
    bool flag_hole_count = (holeCount != 8);
    bool flag_open_errors = openErrors;

    // Generate final report with flags
    std::cout << "\n===== Validation Report =====" << std::endl;
    std::cout << "Directory status:   EXISTS | FLAG 3: OK" << std::endl;
    std::cout << "Electron files: " << electronCount << "/8 | "
              << "FLAG 1: " << (flag_electron_count ? "TRIGGERED" : "OK") 
              << (electronCount < 8 ? " (MISSING)" : (electronCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "Hole files:     " << holeCount << "/8 | "
              << "FLAG 2: " << (flag_hole_count ? "TRIGGERED" : "OK")
              << (holeCount < 8 ? " (MISSING)" : (holeCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "File access:    " << (openErrors ? "ERRORS" : "OK") << std::endl;
              
    std::cout << "\n===== Final Flags Status =====" << std::endl;
    std::cout << "FLAG 1 (Electron count): " << flag_electron_count << std::endl;
    std::cout << "FLAG 2 (Hole count):     " << flag_hole_count << std::endl;
    std::cout << "FLAG 3 (Folder exists):  " << 0 << std::endl;  // 0 bo folder istnieje
    
    std::cout << "\nSummary: ";
    if (!flag_electron_count && !flag_hole_count && !flag_open_errors) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (flag_electron_count) std::cout << "[ELECTRON COUNT] ";
        if (flag_hole_count) std::cout << "[HOLE COUNT] ";
        if (flag_open_errors) std::cout << "[FILE ACCESS]";
    }
    std::cout << std::endl;
}