#include <iostream>
#include <fstream>
#include "TSystem.h"
#include "TSystemDirectory.h"

void CheckTrimFiles(const char* targetDir) {
    // Pobierz bieżącą ścieżkę roboczą
    TString currentDir = gSystem->pwd();
    
    // Utwórz pełną ścieżkę do folderu docelowego
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Utwórz pełną ścieżkę do folderu trim_files
    TString trimDirPath = TString::Format("%s/%s/trim_files", currentDir.Data(), targetDir);

    // Sprawdź czy folder docelowy istnieje w bieżącej lokalizacji
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory '" << targetDir << "' does not exist in current location!" << std::endl;
        std::cerr << "Current location: " << currentDir << std::endl;
        std::cerr << "Expected path:    " << fullTargetPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder exists):  1" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
        std::cout << "\nSummary: [TARGET FOLDER MISSING]" << std::endl;
        return;
    }
    
    // Sprawdź czy folder trim_files istnieje
    if (gSystem->AccessPathName(trimDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'trim_files' does not exist in target folder!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << trimDirPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder exists):  1" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
        std::cout << "\nSummary: [TRIM_FILES FOLDER MISSING]" << std::endl;
        return;
    }

    TSystemDirectory trimDir("trim_files", trimDirPath);
    TList* files = trimDir.GetListOfFiles();
    
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << trimDirPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder access):  1" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
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
        if (fileName.EndsWith("_elect.txt")) {
            electronCount++;
            TString filePath = TString::Format("%s/%s", trimDirPath.Data(), fileName.Data());
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
        // Check hole files
        else if (fileName.EndsWith("_holes.txt")) {
            holeCount++;
            TString filePath = TString::Format("%s/%s", trimDirPath.Data(), fileName.Data());
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole file: " << filePath << std::endl;
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
    std::cout << "Current location:      " << currentDir << std::endl;
    std::cout << "Target directory:      " << targetDir << std::endl;
    std::cout << "Full target path:      " << fullTargetPath << std::endl;
    std::cout << "Trim files location:   " << trimDirPath << std::endl;
    std::cout << "Folder status:         EXISTS | FLAG 0: OK" << std::endl;
    std::cout << "Electron files: " << electronCount << "/8 | "
              << "FLAG 1: " << (flag_electron_count ? "TRIGGERED" : "OK") 
              << (electronCount < 8 ? " (MISSING)" : (electronCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "Hole files:     " << holeCount << "/8 | "
              << "FLAG 2: " << (flag_hole_count ? "TRIGGERED" : "OK")
              << (holeCount < 8 ? " (MISSING)" : (holeCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "File access:    " << (openErrors ? "ERRORS" : "OK") << std::endl;
              
    std::cout << "\n===== Final Flags Status =====" << std::endl;
    std::cout << "FLAG 0 (Folder exists):  " << 0 << std::endl;
    std::cout << "FLAG 1 (Electron count): " << flag_electron_count << std::endl;
    std::cout << "FLAG 2 (Hole count):     " << flag_hole_count << std::endl;
    
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