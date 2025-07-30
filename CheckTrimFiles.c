#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <TObjString.h>  
#include <TCollection.h>
#include "TSystem.h"
#include "TSystemDirectory.h"

// Define flag constants using bitmask
#define FLAG_TRIM_FOLDER_MISSING  0x01  // trim_files directory missing
#define FLAG_DIR_ACCESS           0x02  // Error accessing directory
#define FLAG_ELECTRON_COUNT       0x04  // Incorrect electron file count
#define FLAG_HOLE_COUNT           0x08  // Incorrect hole file count
#define FLAG_FILE_OPEN            0x10  // File opening error
#define FLAG_UNEXPECTED_FILES     0x20  // Unexpected files found in directory

// Structure to hold detailed results
struct CheckTrimFilesResult {
    int flags = 0;                      // Bitmask of flags
    int electronCount = 0;              // Actual count of electron files
    int holeCount = 0;                  // Actual count of hole files
    std::vector<std::string> openErrorFiles;     // Files that failed to open
    std::vector<std::string> unexpectedFiles;    // Unexpected files in directory
};

CheckTrimFilesResult CheckTrimFiles(const char* targetDir) {
    CheckTrimFilesResult result;  // Initialize result structure

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Construct full path to trim_files directory
    TString trimDirPath = TString::Format("%s/%s/trim_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of 'trim_files' directory
    if (gSystem->AccessPathName(trimDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'trim_files' does not exist in target folder!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << trimDirPath << std::endl;
        
        // Set flag and return immediately
        result.flags |= FLAG_TRIM_FOLDER_MISSING;
        return result;
    }

    // Attempt to access directory contents
    TSystemDirectory trimDir("trim_files", trimDirPath);
    TList* files = trimDir.GetListOfFiles();
    
    // Handle directory access failure
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << trimDirPath << std::endl;
        
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
        
        // Process electron files (expected suffix: '_elect.txt')
        if (fileName.EndsWith("_elect.txt")) {
            result.electronCount++;
            TString filePath = TString::Format("%s/%s", trimDirPath.Data(), fileName.Data());
            
            // Verify file accessibility
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                result.openErrorFiles.push_back(filePath.Data());
            } else {
                f_test.close();  // Properly close accessible files
            }
        }
        // Process hole files (expected suffix: '_holes.txt')
        else if (fileName.EndsWith("_holes.txt")) {
            result.holeCount++;
            TString filePath = TString::Format("%s/%s", trimDirPath.Data(), fileName.Data());
            
            // Verify file accessibility
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                result.openErrorFiles.push_back(filePath.Data());
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

    // Set flags based on validation results
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
              
    std::cout << "File accessibility: " << (result.openErrorFiles.empty() ? "ALL OK" : "ERRORS") << std::endl;
    
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
        std::cout << "\n===== UNEXPECTED FILES FOUND =====" << std::endl;
        std::cout << "Files without '_elect.txt' or '_holes.txt' suffix:" << std::endl;
        for (const auto& fileName : result.unexpectedFiles) {
            std::cout << " - " << fileName << std::endl;
        }
    }
    
    // Generate summary of issues
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

    return result;  // Return detailed results
}