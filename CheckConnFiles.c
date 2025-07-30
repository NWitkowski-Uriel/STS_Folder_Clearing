#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <TObjString.h>  
#include <TCollection.h>
#include "TSystem.h"
#include "TSystemDirectory.h"

// Define flag constants using bitmask
#define FLAG_CONN_FOLDER_MISSING     0x01  // conn_check_files directory missing
#define FLAG_DIR_ACCESS              0x02  // Error accessing directory
#define FLAG_ELECTRON_COUNT          0x04  // Incorrect electron file count
#define FLAG_HOLE_COUNT              0x08  // Incorrect hole file count
#define FLAG_FILE_OPEN               0x10  // File opening error
#define FLAG_UNEXPECTED_FILES        0x20  // Unexpected files found in directory

// Structure to hold detailed results
struct CheckConnFilesResult {
    int flags = 0;                      // Bitmask of flags
    int electronCount = 0;              // Count of electron files
    int holeCount = 0;                  // Count of hole files
    std::vector<std::string> openErrorFiles;     // Files that failed to open
    std::vector<std::string> unexpectedFiles;    // Unexpected files in directory
};

CheckConnFilesResult CheckConnFiles(const char* targetDir) {
    CheckConnFilesResult result;  // Initialize result structure

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Construct full path to conn_check_files directory
    TString connDirPath = TString::Format("%s/%s/conn_check_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of 'conn_check_files' directory
    if (gSystem->AccessPathName(connDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'conn_check_files' does not exist!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << connDirPath << std::endl;
        
        result.flags |= FLAG_CONN_FOLDER_MISSING;
        return result;
    }

    // Attempt to access directory contents
    TSystemDirectory connDir("conn_check_files", connDirPath);
    TList* files = connDir.GetListOfFiles();
    
    // Handle directory access failure
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << connDirPath << std::endl;
        
        result.flags |= FLAG_DIR_ACCESS;
        return result;
    }

    // Iterate through all directory entries
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Skip directories and special files (., ..)
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        // Construct full file path
        TString filePath = TString::Format("%s/%s", connDirPath.Data(), fileName.Data());

        // Process electron files
        if (fileName.EndsWith("_elect.txt")) {
            result.electronCount++;
            // Verify file accessibility
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                result.openErrorFiles.push_back(filePath.Data());
                std::cerr << "Error: Cannot open electron file: " << filePath << std::endl;
            } else {
                f_test.close();  // Properly close accessible files
            }
        }
        // Process hole files
        else if (fileName.EndsWith("_holes.txt")) {
            result.holeCount++;
            // Verify file accessibility
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                result.openErrorFiles.push_back(filePath.Data());
                std::cerr << "Error: Cannot open hole file: " << filePath << std::endl;
            } else {
                f_test.close();  // Properly close accessible files
            }
        }
        // Handle unexpected files
        else {
            result.unexpectedFiles.push_back(fileName.Data());
        }
    }

    // Clean up directory list
    delete files;

    // Set flags based on validation
    if (result.electronCount != 8) result.flags |= FLAG_ELECTRON_COUNT;
    if (result.holeCount != 8) result.flags |= FLAG_HOLE_COUNT;
    if (!result.openErrorFiles.empty()) result.flags |= FLAG_FILE_OPEN;
    if (!result.unexpectedFiles.empty()) result.flags |= FLAG_UNEXPECTED_FILES;
    
    // Report file counts and validation status
    std::cout << "\n===== Files Status =====" << std::endl;
    std::cout << "Electron files: " << result.electronCount << "/8 | "
              << "Status: " << ((result.flags & FLAG_ELECTRON_COUNT) ? "FAIL" : "OK") 
              << (result.electronCount < 8 ? " (UNDER)" : (result.electronCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Hole files:     " << result.holeCount << "/8 | "
              << "Status: " << ((result.flags & FLAG_HOLE_COUNT) ? "FAIL" : "OK")
              << (result.holeCount < 8 ? " (UNDER)" : (result.holeCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "File accessibility: " << (result.openErrorFiles.empty() ? "ALL OK" : "ERRORS DETECTED") << std::endl;
    
    // Report open errors if any
    if (!result.openErrorFiles.empty()) {
        std::cout << "\n===== FILE OPEN ERRORS =====" << std::endl;
        std::cout << "Files that could not be opened:" << std::endl;
        for (const auto& filePath : result.openErrorFiles) {
            std::cout << " - " << filePath << std::endl;
        }
    }
    
    // Report unexpected files if any
    if (!result.unexpectedFiles.empty()) {
        std::cout << "\n===== UNEXPECTED FILES =====" << std::endl;
        std::cout << "Unexpected files in directory:" << std::endl;
        for (const auto& fileName : result.unexpectedFiles) {
            std::cout << " - " << fileName << std::endl;
        }
    }
    
    // Generate summary
    std::cout << "\nSummary: ";
    if (result.flags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (result.flags & FLAG_ELECTRON_COUNT) std::cout << "[ELECTRON COUNT] ";
        if (result.flags & FLAG_HOLE_COUNT) std::cout << "[HOLE COUNT] ";
        if (result.flags & FLAG_FILE_OPEN) std::cout << "[FILE ACCESS] ";
        if (result.flags & FLAG_UNEXPECTED_FILES) std::cout << "[UNEXPECTED FILES]";
    }
    std::cout << std::endl;

    return result;
}