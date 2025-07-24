#include <iostream>
#include <fstream>
#include "TSystem.h"
#include "TSystemDirectory.h"

// Define flag constants using bitmask
#define FLAG_FOLDER_MISSING          0x01  // Target directory not found
#define FLAG_CONN_FOLDER_MISSING     0x02  // conn_check_files directory missing
#define FLAG_DIR_ACCESS_ERROR        0x04  // Error accessing directory
#define FLAG_ELECTRON_COUNT          0x08  // Incorrect electron file count
#define FLAG_HOLE_COUNT              0x10  // Incorrect hole file count
#define FLAG_FILE_OPEN_ERROR         0x20  // File opening error

int CheckConnFiles(const char* targetDir) {
    int resultFlags = 0;  // Initialize flag container

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Construct full path to conn_check_files directory
    TString connDirPath = TString::Format("%s/%s/conn_check_files", currentDir.Data(), targetDir);

    // Verify target directory exists
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory '" << targetDir << "' does not exist!" << std::endl;
        std::cerr << "Current location: " << currentDir << std::endl;
        std::cerr << "Expected path:    " << fullTargetPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder exists):  1" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
        std::cout << "\nSummary: [TARGET FOLDER MISSING]" << std::endl;
        
        resultFlags |= FLAG_FOLDER_MISSING;  // Set missing directory flag
        return resultFlags;  // Return immediately with flags
    }
    
    // Verify conn_check_files directory exists
    if (gSystem->AccessPathName(connDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'conn_check_files' does not exist!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << connDirPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder exists):  1" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
        std::cout << "\nSummary: [CONN_CHECK_FILES FOLDER MISSING]" << std::endl;
        
        resultFlags |= FLAG_CONN_FOLDER_MISSING;  // Set missing conn_check_files flag
        return resultFlags;  // Return immediately with flags
    }

    // Access conn_check_files directory contents
    TSystemDirectory connDir("conn_check_files", connDirPath);
    TList* files = connDir.GetListOfFiles();
    
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << connDirPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder access):  1" << std::endl;
        std::cout << "FLAG 1 (Electron count): 1" << std::endl;
        std::cout << "FLAG 2 (Hole count):     1" << std::endl;
        std::cout << "\nSummary: [DIRECTORY ACCESS ERROR]" << std::endl;
        
        resultFlags |= FLAG_DIR_ACCESS_ERROR;  // Set directory access flag
        return resultFlags;  // Return immediately with flags
    }

    int electronCount = 0;   // Counter for electron files
    int holeCount = 0;       // Counter for hole files
    bool openErrors = false; // Track file opening issues

    // Iterate through all files in directory
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Skip directories and special files (. and ..)
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        // Process electron files (ending with '_elect.txt')
        if (fileName.EndsWith("_elect.txt")) {
            electronCount++;
            TString filePath = TString::Format("%s/%s", connDirPath.Data(), fileName.Data());
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
        // Process hole files (ending with '_holes.txt')
        else if (fileName.EndsWith("_holes.txt")) {
            holeCount++;
            TString filePath = TString::Format("%s/%s", connDirPath.Data(), fileName.Data());
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
    }

    // Calculate validation flags based on file counts
    bool flag_electron_count = (electronCount != 8);
    bool flag_hole_count = (holeCount != 8);

    // Update result flags based on validation
    if (flag_electron_count) resultFlags |= FLAG_ELECTRON_COUNT;
    if (flag_hole_count) resultFlags |= FLAG_HOLE_COUNT;
    if (openErrors) resultFlags |= FLAG_FILE_OPEN_ERROR;

    // Generate validation report
    std::cout << "\n===== Validation Report =====" << std::endl;
    std::cout << "Current location:      " << currentDir << std::endl;
    std::cout << "Target directory:      " << targetDir << std::endl;
    std::cout << "Full target path:      " << fullTargetPath << std::endl;
    std::cout << "Conn check location:   " << connDirPath << std::endl;
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
    if (!flag_electron_count && !flag_hole_count && !openErrors) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (flag_electron_count) std::cout << "[ELECTRON COUNT] ";
        if (flag_hole_count) std::cout << "[HOLE COUNT] ";
        if (openErrors) std::cout << "[FILE ACCESS]";
    }
    std::cout << std::endl;

    return resultFlags;  // Return combined flags as bitmask
}