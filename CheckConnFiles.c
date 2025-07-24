#include <iostream>
#include <fstream>
#include "TSystem.h"
#include "TSystemDirectory.h"

// Define flag constants using bitmask
#define FLAG_CONN_FOLDER_MISSING     0x01  // conn_check_files directory missing
#define FLAG_DIR_ACCESS_ERROR        0x02  // Error accessing directory
#define FLAG_ELECTRON_COUNT          0x04  // Incorrect electron file count
#define FLAG_HOLE_COUNT              0x08  // Incorrect hole file count
#define FLAG_FILE_OPEN_ERROR         0x10  // File opening error

int CheckConnFiles(const char* targetDir) {
    int resultFlags = 0;  // Initialize flag container (bitmask)

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory (assumed to exist)
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Construct full path to conn_check_files directory
    TString connDirPath = TString::Format("%s/%s/conn_check_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of 'conn_check_files' directory
    if (gSystem->AccessPathName(connDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'conn_check_files' does not exist!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << connDirPath << std::endl;
        
        // Report status flags (human-readable format)
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Conn folder exists): 1" << std::endl;
        std::cout << "FLAG 1 (Electron count):     1" << std::endl;
        std::cout << "FLAG 2 (Hole count):         1" << std::endl;
        std::cout << "\nSummary: [CONN_CHECK_FILES FOLDER MISSING]" << std::endl;
        
        // Set flag and return immediately
        resultFlags |= FLAG_CONN_FOLDER_MISSING;
        return resultFlags;
    }

    // Attempt to access directory contents
    TSystemDirectory connDir("conn_check_files", connDirPath);
    TList* files = connDir.GetListOfFiles();
    
    // Handle directory access failure
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << connDirPath << std::endl;
        
        // Report status flags
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Directory access):  1" << std::endl;
        std::cout << "FLAG 1 (Electron count):    1" << std::endl;
        std::cout << "FLAG 2 (Hole count):        1" << std::endl;
        std::cout << "\nSummary: [DIRECTORY ACCESS ERROR]" << std::endl;
        
        resultFlags |= FLAG_DIR_ACCESS_ERROR;
        return resultFlags;
    }

    // Initialize counters for validation
    int electronCount = 0;   // Tracks electron-related files
    int holeCount = 0;       // Tracks hole-related files
    bool openErrors = false; // Flags file access issues

    // Iterate through all directory entries
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Skip directories and special files (., ..)
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        // Construct full file path
        TString filePath = TString::Format("%s/%s", connDirPath.Data(), fileName.Data());

        // Process electron files (expected suffix: '_elect.txt')
        if (fileName.EndsWith("_elect.txt")) {
            electronCount++;
            // Verify file accessibility
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();  // Properly close accessible files
            }
        }
        // Process hole files (expected suffix: '_holes.txt')
        else if (fileName.EndsWith("_holes.txt")) {
            holeCount++;
            // Verify file accessibility
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();  // Properly close accessible files
            }
        }
    }

    // Validate file counts against expected quantity (8 each)
    bool flag_electron_count = (electronCount != 8);
    bool flag_hole_count = (holeCount != 8);

    // Update result flags based on validation
    if (flag_electron_count) resultFlags |= FLAG_ELECTRON_COUNT;
    if (flag_hole_count) resultFlags |= FLAG_HOLE_COUNT;
    if (openErrors) resultFlags |= FLAG_FILE_OPEN_ERROR;

    // Generate comprehensive validation report
    std::cout << "\n===== Validation Report =====" << std::endl;
    std::cout << "Current location:        " << currentDir << std::endl;
    std::cout << "Target directory:        " << targetDir << std::endl;
    std::cout << "Full target path:        " << fullTargetPath << std::endl;
    std::cout << "Connection check location: " << connDirPath << std::endl;
    
    // Report file counts and validation status
    std::cout << "Electron files: " << electronCount << "/8 | "
              << "Status: " << (flag_electron_count ? "FAIL" : "OK") 
              << (electronCount < 8 ? " (UNDER)" : (electronCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Hole files:     " << holeCount << "/8 | "
              << "Status: " << (flag_hole_count ? "FAIL" : "OK")
              << (holeCount < 8 ? " (UNDER)" : (holeCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "File accessibility: " << (openErrors ? "ERRORS DETECTED" : "ALL FILES ACCESSIBLE") << std::endl;
              
    // Report flag status (bitmask interpretation)
    std::cout << "\n===== Final Flags Status (Bitmask) =====" << std::endl;
    std::cout << "FLAG 0 (Electron count): " << flag_electron_count << std::endl;
    std::cout << "FLAG 1 (Hole count):     " << flag_hole_count << std::endl;
    std::cout << "FLAG 2 (File access):    " << openErrors << std::endl;
    
    // Generate summary of issues
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