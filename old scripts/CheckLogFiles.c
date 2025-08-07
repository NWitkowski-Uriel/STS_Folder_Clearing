#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "TSystem.h"
#include "TRegexp.h"
#include "TSystemDirectory.h"

// Define flag constants using bitmask
#define FLAG_DIR_MISSING         0x01   // Target directory missing
#define FLAG_LOG_MISSING         0x02   // Log file missing
#define FLAG_DATA_MISSING        0x04   // No matching data files found
#define FLAG_NO_FEB_FILE         0x08   // No tester_febs_* files found
#define FLAG_FILE_OPEN           0x10   // File opening error
#define FLAG_DATA_EMPTY          0x20   // All matching data files are empty
#define FLAG_DATA_INVALID        0x40   // Data file content is invalid
#define FLAG_UNEXPECTED_FILES    0x80   // Unexpected files in directory

// Structure to hold detailed results
struct CheckLogFilesResult {
    int flags = 0;                      // Bitmask of flags
    int dataFileCount = 0;              // Total data files found
    int nonEmptyDataCount = 0;          // Non-empty data files
    int validDataCount = 0;             // Valid data files
    bool logExists = false;             // Log file exists
    bool foundFebFile = false;          // FEB file found
    
    // Problematic files
    std::vector<std::string> openErrorFiles;     // Files that failed to open
    std::vector<std::string> unexpectedFiles;    // Unexpected files in directory
    std::vector<std::string> invalidDataFiles;   // Data files with invalid content
    std::vector<std::string> emptyDataFiles;     // Empty data files
};

// Helper function to check data file content
static bool CheckDataFileContent(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("LV_AFT_CONFIG_P") != std::string::npos) {
            int validLinesAfter = 0;
            
            while (std::getline(file, line)) {
                if (line.find_first_not_of(" \t") == std::string::npos) {
                    continue;
                }
                
                validLinesAfter++;
                if (validLinesAfter >= 2) {
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

CheckLogFilesResult CheckLogFiles(const char* targetDir) {
    CheckLogFilesResult result;

    // Get current working directory
    TString currentDir = gSystem->pwd();
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of target directory
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory does not exist: " << fullTargetPath << std::endl;
        result.flags |= FLAG_DIR_MISSING;
        
        // Report status
        std::cout << "\n===== Files Status =====" << std::endl;
        std::cout << "Target directory:    MISSING" << std::endl;
        
        return result;
    }

    // List of acceptable auxiliary files
    std::vector<TString> acceptableAuxFiles = {
        // Add any acceptable auxiliary files here if needed
    };

    // Construct expected log file path
    TString logFilePath = TString::Format("%s/%s_log.log", fullTargetPath.Data(), targetDir);

    // Check log file
    if (!gSystem->AccessPathName(logFilePath, kFileExists)) {
        result.logExists = true;
        std::ifstream f_log(logFilePath.Data());
        if (!f_log.is_open()) {
            std::cerr << "Error: Cannot open log file: " << logFilePath << std::endl;
            result.openErrorFiles.push_back(logFilePath.Data());
        } else {
            f_log.close();
        }
    } else {
        result.flags |= FLAG_LOG_MISSING;
    }

    // Directory traversal
    TSystemDirectory targetDirObj(targetDir, fullTargetPath);
    TList* files = targetDirObj.GetListOfFiles();
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << fullTargetPath << std::endl;
        result.flags |= FLAG_FILE_OPEN;
        return result;
    }

    TSystemFile* file;
    TIter next(files);

    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Skip directories and special files
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        TString fullFilePath = TString::Format("%s/%s", fullTargetPath.Data(), fileName.Data());

        bool isExpectedFile = false;

        // Check for data files
        if (fileName.BeginsWith(targetDir) && fileName.EndsWith("_data.dat")) {
            isExpectedFile = true;
            result.dataFileCount++;
            std::ifstream f_data(fullFilePath.Data(), std::ios::binary | std::ios::ate);
            if (!f_data.is_open()) {
                std::cerr << "Error: Cannot open data file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fullFilePath.Data());
            } else {
                std::streampos size = f_data.tellg();
                f_data.close();
                
                if (size > 0) {
                    result.nonEmptyDataCount++;
                    if (CheckDataFileContent(fullFilePath.Data())) {
                        result.validDataCount++;
                    } else {
                        result.invalidDataFiles.push_back(fullFilePath.Data());
                    }
                } else {
                    result.emptyDataFiles.push_back(fullFilePath.Data());
                }
            }
        }
        // Check for FEB files
        else if (fileName.BeginsWith("tester_febs_")) {
            isExpectedFile = true;
            result.foundFebFile = true;
            std::ifstream f_feb(fullFilePath.Data());
            if (!f_feb.is_open()) {
                std::cerr << "Error: Cannot open FEB file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fullFilePath.Data());
            } else {
                f_feb.close();
            }
        }
        // Check for log file
        else if (fileName == TString::Format("%s_log.log", targetDir)) {
            isExpectedFile = true;
        }
        // Check against acceptable auxiliary files
        else {
            for (const auto& auxFile : acceptableAuxFiles) {
                if (fileName == auxFile) {
                    isExpectedFile = true;
                    break;
                }
            }
        }

        // Collect unexpected files
        if (!isExpectedFile) {
            result.unexpectedFiles.push_back(fileName.Data());
        }
    }

    // Clean up directory list
    delete files;

    // Set flags based on validation
    if (result.dataFileCount == 0) result.flags |= FLAG_DATA_MISSING;
    if (result.nonEmptyDataCount == 0 && result.dataFileCount > 0) result.flags |= FLAG_DATA_EMPTY;
    if (result.validDataCount == 0 && result.nonEmptyDataCount > 0) result.flags |= FLAG_DATA_INVALID;
    if (!result.foundFebFile) result.flags |= FLAG_NO_FEB_FILE;
    if (!result.openErrorFiles.empty()) result.flags |= FLAG_FILE_OPEN;
    if (!result.unexpectedFiles.empty()) result.flags |= FLAG_UNEXPECTED_FILES;

    // Report file statuses
    std::cout << "\n===== Files Status =====" << std::endl;
    std::cout << "Log file:         " << (result.logExists ? 
              (std::find(result.openErrorFiles.begin(), result.openErrorFiles.end(), logFilePath.Data()) != result.openErrorFiles.end() ? 
              "EXISTS (OPEN ERROR)" : "EXISTS") : "MISSING") << std::endl;
              
    std::cout << "Data files:       " << result.dataFileCount << " found | " 
              << ((result.flags & FLAG_DATA_MISSING) ? "NONE" : 
                 (result.flags & FLAG_DATA_EMPTY) ? "ALL EMPTY" :
                 (result.flags & FLAG_DATA_INVALID) ? "INVALID CONTENT" : "VALID") << std::endl;
                 
    std::cout << "Non-empty files:  " << result.nonEmptyDataCount << "/" << result.dataFileCount << std::endl;
    std::cout << "Valid files:      " << result.validDataCount << "/" << result.dataFileCount << std::endl;
    std::cout << "Tester FEB files: " << (result.foundFebFile ? "FOUND" : "NONE") << std::endl;
    std::cout << "File access:      " << (result.openErrorFiles.empty() ? "OK" : "ERRORS DETECTED") << std::endl;

    // Report empty data files
    if (!result.emptyDataFiles.empty()) {
        std::cout << "\n===== Empty Data Files =====" << std::endl;
        std::cout << "Count: " << result.emptyDataFiles.size() << std::endl;
        for (const auto& file : result.emptyDataFiles) {
            std::cout << "  - " << file << std::endl;
        }
    }

    // Report invalid data files
    if (!result.invalidDataFiles.empty()) {
        std::cout << "\n===== Invalid Data Files =====" << std::endl;
        std::cout << "Files with invalid content: " << result.invalidDataFiles.size() << std::endl;
        for (const auto& file : result.invalidDataFiles) {
            std::cout << "  - " << file << std::endl;
        }
    }

    // Report open errors
    if (!result.openErrorFiles.empty()) {
        std::cout << "\n===== File Open Errors =====" << std::endl;
        std::cout << "Files that could not be opened: " << result.openErrorFiles.size() << std::endl;
        for (const auto& file : result.openErrorFiles) {
            std::cout << "  - " << file << std::endl;
        }
    }

    // Report unexpected files
    if (!result.unexpectedFiles.empty()) {
        std::cout << "\n===== Unexpected Files =====" << std::endl;
        std::cout << "Count: " << result.unexpectedFiles.size() << std::endl;
        for (const auto& file : result.unexpectedFiles) {
            std::cout << "  - " << file << std::endl;
        }
    }

    // Generate summary
    std::cout << "\nSummary: ";
    if (result.flags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (result.flags & FLAG_DIR_MISSING) std::cout << "[DIRECTORY MISSING] ";
        if (result.flags & FLAG_LOG_MISSING) std::cout << "[LOG MISSING] ";
        if (result.flags & FLAG_DATA_MISSING) std::cout << "[DATA MISSING] ";
        if (result.flags & FLAG_NO_FEB_FILE) std::cout << "[NO FEB FILES] ";
        if (result.flags & FLAG_FILE_OPEN) std::cout << "[FILE OPEN ERROR] ";
        if (result.flags & FLAG_DATA_EMPTY) std::cout << "[DATA EMPTY] ";
        if (result.flags & FLAG_DATA_INVALID) std::cout << "[DATA INVALID] ";
        if (result.flags & FLAG_UNEXPECTED_FILES) std::cout << "[UNEXPECTED FILES] ";
    }
    std::cout << std::endl;

    return result;
}