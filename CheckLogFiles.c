#include <iostream>
#include <fstream>
#include "TSystem.h"
#include "TRegexp.h"

// Define flag constants using bitmask
#define FLAG_DIR_MISSING         0x01  // Target directory missing
#define FLAG_LOG_MISSING         0x02  // Log file missing
#define FLAG_DATA_MISSING        0x04  // Fixed data file missing
#define FLAG_NO_FEB_FILE         0x08  // No tester_febs_* files found
#define FLAG_FILE_OPEN_ERROR     0x10  // File opening error
#define FLAG_DATA_EMPTY          0x20  // Data file empty

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
    bool dataValid = false;
    bool foundFebFile = false;
    bool openErrors = false;
    bool dataEmpty = false;

    // Construct expected file paths
    TString logFilePath = TString::Format("%s/%s_log.log", fullTargetPath.Data(), targetDir);
    TString dataFilePath = TString::Format("%s/%s_data.dat", fullTargetPath.Data(), targetDir);

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

    // Check fixed data file - must exist and be valid
    if (!gSystem->AccessPathName(dataFilePath, kFileExists)) {
        std::ifstream f_data(dataFilePath.Data(), std::ios::binary | std::ios::ate);
        if (!f_data.is_open()) {
            std::cerr << "Error: Cannot open data file: " << dataFilePath << std::endl;
            openErrors = true;
            resultFlags |= FLAG_DATA_MISSING;
        } else {
            // Validate content size
            std::streampos size = f_data.tellg();
            if (size == 0) {
                std::cerr << "Error: Data file is empty: " << dataFilePath << std::endl;
                dataEmpty = true;
                resultFlags |= FLAG_DATA_EMPTY;
            } else {
                dataValid = true;
            }
            f_data.close();
        }
    } else {
        resultFlags |= FLAG_DATA_MISSING;
    }

    // Directory traversal for FEB files (always) and dated files (if needed)
    void* dirp = gSystem->OpenDirectory(fullTargetPath);
    if (!dirp) {
        std::cerr << "Error: Could not read directory contents: " << fullTargetPath << std::endl;
        openErrors = true;
        resultFlags |= FLAG_NO_FEB_FILE;
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

            // Check for FEB files
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

    // Set flags based on checks
    if (!foundFebFile) {
        resultFlags |= FLAG_NO_FEB_FILE;
    }
    if (openErrors) {
        resultFlags |= FLAG_FILE_OPEN_ERROR;
    }

    // Report file statuses
    std::cout << "\n===== Files Status =====" << std::endl;
    std::cout << "Log file:  " << logFilePath << " ... " 
              << (logExists ? (openErrors ? "exists (OPEN FAILED)" : "exists (accessible)") : "MISSING") 
              << std::endl;
              
    std::cout << "Data file: " << dataFilePath << " ... " 
              << (dataValid ? "exists (valid)" : 
                  (resultFlags & FLAG_DATA_MISSING ? "MISSING" : 
                   (dataEmpty ? "EMPTY" : "INVALID")))
              << std::endl;
              
    std::cout << "Tester FEB files: " << (foundFebFile ? "at least one found" : "NONE found") 
              << (foundFebFile && openErrors ? " (some OPEN FAILED)" : "")
              << std::endl;

    // Report final flags status
    std::cout << "\n===== Final Flags Status =====" << std::endl;
    std::cout << "FLAG_DIR_MISSING:     " << ((resultFlags & FLAG_DIR_MISSING) ? 1 : 0) << std::endl;
    std::cout << "FLAG_LOG_MISSING:     " << ((resultFlags & FLAG_LOG_MISSING) ? 1 : 0) << std::endl;
    std::cout << "FLAG_DATA_MISSING:    " << ((resultFlags & FLAG_DATA_MISSING) ? 1 : 0) << std::endl;
    std::cout << "FLAG_NO_FEB_FILE:     " << ((resultFlags & FLAG_NO_FEB_FILE) ? 1 : 0) << std::endl;
    std::cout << "FLAG_FILE_OPEN_ERROR: " << ((resultFlags & FLAG_FILE_OPEN_ERROR) ? 1 : 0) << std::endl;
    std::cout << "FLAG_DATA_EMPTY:      " << ((resultFlags & FLAG_DATA_EMPTY) ? 1 : 0) << std::endl;

    // Generate summary of issues
    std::cout << "\nSummary: ";
    if (resultFlags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (resultFlags & FLAG_DIR_MISSING) std::cout << "[DIRECTORY MISSING] ";
        if (resultFlags & FLAG_LOG_MISSING) std::cout << "[LOG FILE MISSING] ";
        if (resultFlags & FLAG_DATA_MISSING) std::cout << "[DATA FILE MISSING] ";
        if (resultFlags & FLAG_NO_FEB_FILE) std::cout << "[NO FEB FILES] ";
        if (resultFlags & FLAG_FILE_OPEN_ERROR) std::cout << "[FILE OPEN ERROR] ";
        if (resultFlags & FLAG_DATA_EMPTY) std::cout << "[DATA FILE EMPTY]";
    }
    std::cout << std::endl;

    return resultFlags;
}