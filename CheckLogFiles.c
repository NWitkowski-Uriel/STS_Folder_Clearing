#include <iostream>
#include <fstream>
#include <string>
#include "TSystem.h"
#include "TRegexp.h"

// Define flag constants using bitmask
#define FLAG_DIR_MISSING         0x01  // Target directory missing
#define FLAG_LOG_MISSING         0x02  // Log file missing
#define FLAG_DATA_MISSING        0x04  // No matching data files found
#define FLAG_NO_FEB_FILE         0x08  // No tester_febs_* files found
#define FLAG_FILE_OPEN           0x10  // File opening error
#define FLAG_DATA_EMPTY          0x20  // All matching data files are empty
#define FLAG_DATA_INVALID        0x40  // Data file content is invalid

// Helper function to check data file content
static bool CheckDataFileContent(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Find the required marker
        if (line.find("LV_AFT_CONFIG_P") != std::string::npos) {
            int validLinesAfter = 0;
            
            // Check next two non-empty lines
            while (std::getline(file, line)) {
                // Skip empty lines (whitespace only)
                if (line.find_first_not_of(" \t") == std::string::npos) {
                    continue;
                }
                
                // Found a non-empty line
                validLinesAfter++;
                
                // We need at least two non-empty lines
                if (validLinesAfter >= 2) {
                    return true;
                }
            }
            break; // Stop after first marker found
        }
    }
    return false;
}

int CheckLogFiles(const char* targetDir) {
    int resultFlags = 0;  // Initialize flag container (bitmask)

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of target directory
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory does not exist: " << fullTargetPath << std::endl;
        resultFlags |= FLAG_DIR_MISSING;
        return resultFlags;
    }

    // Initialize control variables
    bool logExists = false;
    bool foundFebFile = false;
    bool openErrors = false;
    int dataFileCount = 0;
    int nonEmptyDataCount = 0;
    int validDataCount = 0;
    bool dataOpenError = false;

    // Construct expected log file path
    TString logFilePath = TString::Format("%s/%s_log.log", fullTargetPath.Data(), targetDir);

    // Check log file (.log extension)
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

    // Directory traversal for FEB files and data files
    void* dirp = gSystem->OpenDirectory(fullTargetPath);
    if (!dirp) {
        std::cerr << "Error: Could not read directory contents: " << fullTargetPath << std::endl;
        openErrors = true;
        resultFlags |= FLAG_NO_FEB_FILE | FLAG_DATA_MISSING;
    } else {
        const char* entry;
        while ((entry = gSystem->GetDirEntry(dirp))) {
            TString fileName = entry;
            if (fileName == "." || fileName == "..") continue;
            
            TString fullFilePath = TString::Format("%s/%s", fullTargetPath.Data(), entry);
            
            // Skip directories
            FileStat_t stat;
            if (gSystem->GetPathInfo(fullFilePath, stat)) continue;
            if (R_ISDIR(stat.fMode)) continue;

            // Check for data files: [targetDir]*_data.dat
            if (fileName.BeginsWith(targetDir) && fileName.EndsWith("_data.dat")) {
                dataFileCount++;
                std::ifstream f_data(fullFilePath.Data(), std::ios::binary | std::ios::ate);
                if (!f_data.is_open()) {
                    std::cerr << "Error: Cannot open data file: " << fullFilePath << std::endl;
                    openErrors = true;
                    dataOpenError = true;
                } else {
                    std::streampos size = f_data.tellg();
                    f_data.close();
                    
                    if (size > 0) {
                        nonEmptyDataCount++;
                        
                        // Check file content if not empty
                        if (CheckDataFileContent(fullFilePath.Data())) {
                            validDataCount++;
                        }
                    }
                }
            }

            // Check for FEB files (tester_febs_*)
            if (fileName.BeginsWith("tester_febs_")) {
                foundFebFile = true;
                std::ifstream f_feb(fullFilePath.Data());
                if (!f_feb.is_open()) {
                    std::cerr << "Error: Cannot open FEB file: " << fullFilePath << std::endl;
                    openErrors = true;
                } else {
                    f_feb.close();
                }
            }
        }
        gSystem->FreeDirectory(dirp);
    }

    // Set flags based on data file checks
    if (dataFileCount == 0) {
        resultFlags |= FLAG_DATA_MISSING;
    } else if (nonEmptyDataCount == 0) {
        resultFlags |= FLAG_DATA_EMPTY;
    } else if (validDataCount == 0) {
        resultFlags |= FLAG_DATA_INVALID;
    }

    // Set flags for FEB files and open errors
    if (!foundFebFile) {
        resultFlags |= FLAG_NO_FEB_FILE;
    }
    if (openErrors) {
        resultFlags |= FLAG_FILE_OPEN;
    }

    // Report file statuses
    std::cout << "\n===== Files Status =====" << std::endl;
    std::cout << "Log file:  " << logFilePath << " ... " 
              << (logExists ? (openErrors ? "exists (OPEN FAILED)" : "exists (accessible)") : "MISSING") 
              << std::endl;
              
    std::cout << "Data files: pattern '" << targetDir << "*_data.dat' ... ";
    if (dataFileCount == 0) {
        std::cout << "NONE found";
    } else if (nonEmptyDataCount == 0) {
        std::cout << "found but ALL EMPTY";
    } else if (validDataCount == 0) {
        std::cout << "found but NO VALID CONTENT";
    } else {
        std::cout << "found (" << validDataCount << "/" << nonEmptyDataCount << "/" << dataFileCount 
                  << " valid/non-empty/total)";
    }
    std::cout << (dataOpenError ? " (some OPEN FAILED)" : "") << std::endl;
              
    std::cout << "Tester FEB files: " << (foundFebFile ? "at least one found" : "NONE found") 
              << (foundFebFile && openErrors ? " (some OPEN FAILED)" : "")
              << std::endl;

    // Report final flags status
    std::cout << "\n===== Final Flags Status =====" << std::endl;
    std::cout << "FLAG_DIR_MISSING:     " << ((resultFlags & FLAG_DIR_MISSING) ? 1 : 0) << std::endl;
    std::cout << "FLAG_LOG_MISSING:     " << ((resultFlags & FLAG_LOG_MISSING) ? 1 : 0) << std::endl;
    std::cout << "FLAG_DATA_MISSING:    " << ((resultFlags & FLAG_DATA_MISSING) ? 1 : 0) << std::endl;
    std::cout << "FLAG_NO_FEB_FILE:     " << ((resultFlags & FLAG_NO_FEB_FILE) ? 1 : 0) << std::endl;
    std::cout << "FLAG_FILE_OPEN:       " << ((resultFlags & FLAG_FILE_OPEN) ? 1 : 0) << std::endl;
    std::cout << "FLAG_DATA_EMPTY:      " << ((resultFlags & FLAG_DATA_EMPTY) ? 1 : 0) << std::endl;
    std::cout << "FLAG_DATA_INVALID:    " << ((resultFlags & FLAG_DATA_INVALID) ? 1 : 0) << std::endl;

    // Generate summary of issues
    std::cout << "\nSummary: ";
    if (resultFlags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (resultFlags & FLAG_DIR_MISSING) std::cout << "[DIRECTORY MISSING] ";
        if (resultFlags & FLAG_LOG_MISSING) std::cout << "[LOG FILE MISSING] ";
        if (resultFlags & FLAG_DATA_MISSING) std::cout << "[DATA FILE MISSING] ";
        if (resultFlags & FLAG_NO_FEB_FILE) std::cout << "[NO FEB FILES] ";
        if (resultFlags & FLAG_FILE_OPEN) std::cout << "[FILE OPEN ERROR] ";
        if (resultFlags & FLAG_DATA_EMPTY) std::cout << "[DATA FILE EMPTY] ";
        if (resultFlags & FLAG_DATA_INVALID) std::cout << "[DATA CONTENT INVALID]";
    }
    std::cout << std::endl;

    return resultFlags;
}