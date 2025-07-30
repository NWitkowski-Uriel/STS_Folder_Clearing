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

// Global variables to store unexpected files and invalid data files
std::vector<std::string> g_unexpectedFiles;
std::vector<std::string> g_invalidDataFiles;

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

int CheckLogFiles(const char* targetDir) {
    int resultFlags = 0;
    g_unexpectedFiles.clear();
    g_invalidDataFiles.clear();

    // Get current working directory
    TString currentDir = gSystem->pwd();
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of target directory
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory does not exist: " << fullTargetPath << std::endl;
        resultFlags |= FLAG_DIR_MISSING;
        
        // Report status
        std::cout << "\n===== Files Status =====" << std::endl;
        std::cout << "Target directory:    MISSING" << std::endl;
        
        return resultFlags;
    }

    // Initialize control variables
    bool logExists = false;
    bool foundFebFile = false;
    bool openErrors = false;
    int dataFileCount = 0;
    int nonEmptyDataCount = 0;
    int validDataCount = 0;
    bool hasUnexpectedFiles = false;

    // List of acceptable auxiliary files
    std::vector<TString> acceptableAuxFiles = {

    };

    // Construct expected log file path
    TString logFilePath = TString::Format("%s/%s_log.log", fullTargetPath.Data(), targetDir);

    // Check log file
    if (!gSystem->AccessPathName(logFilePath, kFileExists)) {
        logExists = true;
        std::ifstream f_log(logFilePath.Data());
        if (!f_log.is_open()) {
            std::cerr << "Error: Cannot open log file: " << logFilePath << std::endl;
            openErrors = true;
        } else {
            f_log.close();
        }
    } else {
        resultFlags |= FLAG_LOG_MISSING;
    }

    // Directory traversal
    TSystemDirectory targetDirObj(targetDir, fullTargetPath);
    TList* files = targetDirObj.GetListOfFiles();
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
            dataFileCount++;
            std::ifstream f_data(fullFilePath.Data(), std::ios::binary | std::ios::ate);
            if (!f_data.is_open()) {
                std::cerr << "Error: Cannot open data file: " << fullFilePath << std::endl;
                openErrors = true;
            } else {
                std::streampos size = f_data.tellg();
                f_data.close();
                
                if (size > 0) {
                    nonEmptyDataCount++;
                    if (!CheckDataFileContent(fullFilePath.Data())) {
                        g_invalidDataFiles.push_back(fileName.Data());
                    } else {
                        validDataCount++;
                    }
                }
            }
        }
        // Check for FEB files
        else if (fileName.BeginsWith("tester_febs_")) {
            isExpectedFile = true;
            foundFebFile = true;
            std::ifstream f_feb(fullFilePath.Data());
            if (!f_feb.is_open()) {
                std::cerr << "Error: Cannot open FEB file: " << fullFilePath << std::endl;
                openErrors = true;
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
            hasUnexpectedFiles = true;
            g_unexpectedFiles.push_back(fileName.Data());
        }
    }

    // Set flags based on validation
    if (dataFileCount == 0) resultFlags |= FLAG_DATA_MISSING;
    if (nonEmptyDataCount == 0 && dataFileCount > 0) resultFlags |= FLAG_DATA_EMPTY;
    if (validDataCount == 0 && nonEmptyDataCount > 0) resultFlags |= FLAG_DATA_INVALID;
    if (!foundFebFile) resultFlags |= FLAG_NO_FEB_FILE;
    if (openErrors) resultFlags |= FLAG_FILE_OPEN;
    if (hasUnexpectedFiles) resultFlags |= FLAG_UNEXPECTED_FILES;

    // Report file statuses
    std::cout << "\n===== Files Status =====" << std::endl;
    std::cout << "Log file:         " << (logExists ? (openErrors ? "EXISTS (OPEN ERROR)" : "EXISTS") : "MISSING") << std::endl;
    std::cout << "Data files:       " << dataFileCount << " found | " 
              << ((resultFlags & FLAG_DATA_MISSING) ? "NONE" : 
                 (resultFlags & FLAG_DATA_EMPTY) ? "ALL EMPTY" :
                 (resultFlags & FLAG_DATA_INVALID) ? "INVALID CONTENT" : "VALID") << std::endl;
    std::cout << "Tester FEB files: " << (foundFebFile ? "FOUND" : "NONE") << std::endl;
    std::cout << "File access:      " << (openErrors ? "ERRORS DETECTED" : "OK") << std::endl;

    // Report invalid data files if any
    if (!g_invalidDataFiles.empty()) {
        std::cout << "\n===== Invalid Data Files Found =====" << std::endl;
        std::cout << "Count: " << g_invalidDataFiles.size() << std::endl;
        for (const auto& file : g_invalidDataFiles) {
            std::cout << "  - " << file << std::endl;
        }
    }

    // Report unexpected files if any
    if (hasUnexpectedFiles) {
        std::cout << "\n===== Unexpected Files Found =====" << std::endl;
        std::cout << "Count: " << g_unexpectedFiles.size() << std::endl;
        for (const auto& file : g_unexpectedFiles) {
            std::cout << "  - " << file << std::endl;
        }
    }

    // Generate summary of issues
    std::cout << "\nSummary: ";
    if (resultFlags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (resultFlags & FLAG_DIR_MISSING) std::cout << "[DIRECTORY MISSING] ";
        if (resultFlags & FLAG_LOG_MISSING) std::cout << "[LOG MISSING] ";
        if (resultFlags & FLAG_DATA_MISSING) std::cout << "[DATA MISSING] ";
        if (resultFlags & FLAG_NO_FEB_FILE) std::cout << "[NO FEB FILES] ";
        if (resultFlags & FLAG_FILE_OPEN) std::cout << "[FILE OPEN ERROR] ";
        if (resultFlags & FLAG_DATA_EMPTY) std::cout << "[DATA EMPTY] ";
        if (resultFlags & FLAG_DATA_INVALID) std::cout << "[DATA INVALID] ";
        if (resultFlags & FLAG_UNEXPECTED_FILES) std::cout << "[UNEXPECTED FILES] ";
    }
    std::cout << std::endl;

    return resultFlags;
}