/*
 *
 * EXORCISM - Ladder Test Data Validation System
 * Copyright (c) 2025 Nikodem Witkowski
 * Licensed under the MIT License
 * 
 * This program performs comprehensive validation of ladder test data
 * directory structures and files. It checks for required files, naming conventions,
 * content validity, and generates detailed reports before/after cleanup operations.
 */

// Standard C++ Library Headers
#include <iostream>   // For console input/output operations
#include <fstream>    // For file stream operations
#include <vector>     // For dynamic array functionality
#include <string>     // For string manipulation
#include <sstream>    // For string stream processing
#include <iomanip>    // For output formatting
#include <ctime>      // For date/time functions
#include <map>        // For key-value pair storage
#include <algorithm>  // For sorting/searching algorithms
#include <set>        // For unique element storage

// ROOT Framework Headers (Data Analysis)
#include <TSystem.h>              // System interface utilities
#include <TString.h>              // ROOT string implementation
#include <TSystemDirectory.h>     // Directory handling
#include <TSystemFile.h>          // File handling
#include <TList.h>                // Linked list container
#include <TFile.h>                // ROOT file I/O operations
#include <TCanvas.h>              // Drawing canvas
#include <TLatex.h>               // LaTeX text rendering
#include <TPie.h>                 // Pie chart visualization
#include <TLegend.h>              // Chart legend
#include <TObjString.h>           // String object wrapper
#include <TInterpreter.h>         // C++ interpreter
#include <TPaveText.h>            // Text box widget

// ===================================================================
// Global Constants and Structures
// ===================================================================

/*
 * Evaluation Status Levels:
 * These constants define the possible validation outcomes
 */
#define STATUS_PASSED           0       // All checks successful
#define STATUS_PASSED_WITH_ISSUES 1     // Minor non-critical issues
#define STATUS_FAILED           2       // Critical validation failures

/*
 * Log Files Validation Flags (Bitmask):
 * Each flag represents a specific validation failure condition
 */
#define FLAG_DIR_MISSING         0x01   // Target directory not found
#define FLAG_LOG_MISSING         0x02   // Main log file missing
#define FLAG_DATA_MISSING        0x04   // Required data files missing
#define FLAG_NO_FEB_FILE         0x08   // Matching tester FEB file missing
#define FLAG_FILE_OPEN           0x10   // File access error
#define FLAG_DATA_EMPTY          0x20   // Empty data file
#define FLAG_DATA_INVALID        0x40   // Invalid file content
#define FLAG_UNEXPECTED_FILES    0x80   // Unexpected files in directory

/*
 * Connection Files Validation Flags:
 * Specific flags for connection test files
 */
#define FLAG_CONN_FOLDER_MISSING 0x01   // conn_check_files directory missing
#define FLAG_DIR_ACCESS          0x02   // Directory access error
#define FLAG_ELECTRON_COUNT      0x04   // Incorrect electron file count
#define FLAG_HOLE_COUNT          0x08   // Incorrect hole file count
#define FLAG_FILE_OPEN_CONN      0x10   // Connection file access error
#define FLAG_UNEXPECTED_FILES_CONN 0x20 // Unexpected files in connection dir

/*
 * Trim Files Validation Flags:
 * Specific flags for trim adjustment files
 */
#define FLAG_TRIM_FOLDER_MISSING 0x01   // trim_files directory missing
#define FLAG_DIR_ACCESS_TRIM     0x02   // Directory access error
#define FLAG_ELECTRON_COUNT_TRIM 0x04   // Incorrect electron file count
#define FLAG_HOLE_COUNT_TRIM     0x08   // Incorrect hole file count
#define FLAG_FILE_OPEN_TRIM      0x10   // Trim file access error
#define FLAG_UNEXPECTED_FILES_TRIM 0x20 // Unexpected files in trim dir

/*
 * Pscan Files Validation Flags:
 * Specific flags for parameter scan files
 */
#define FLAG_PSCAN_FOLDER_MISSING 0x01  // pscan_files directory missing
#define FLAG_DIR_ACCESS_PSCAN    0x02   // Directory access error
#define FLAG_ELECTRON_TXT        0x04   // Incorrect electron txt count
#define FLAG_HOLE_TXT            0x08   // Incorrect hole txt count
#define FLAG_ELECTRON_ROOT       0x10   // Incorrect electron root count
#define FLAG_HOLE_ROOT           0x20   // Incorrect hole root count
#define FLAG_FILE_OPEN_PSCAN     0x40   // Pscan file access error
#define FLAG_UNEXPECTED_FILES_PSCAN 0x80 // Unexpected files in pscan dir
#define FLAG_MODULE_ROOT         0x100  // Module root file error
#define FLAG_MODULE_TXT          0x200  // Module text file error
#define FLAG_MODULE_PDF          0x400  // Module PDF file missing

/*
 * ValidationResult Structure:
 * Contains all validation results for a single test directory
 */
struct ValidationResult {
    int flags = 0;  // Bitmask of encountered issues
    
    // Error file collections
    std::vector<std::string> openErrorFiles;    // Files that couldn't be opened
    std::vector<std::string> unexpectedFiles;   // Unexpected files found
    std::vector<std::string> emptyFiles;        // Empty files found
    std::vector<std::string> invalidFiles;      // Files with invalid content
    std::vector<std::string> moduleErrorFiles;  // Module test file errors
    
    // Log files specific counters
    int dataFileCount = 0;      // Total data files found
    int nonEmptyDataCount = 0;  // Non-empty data files
    int validDataCount = 0;     // Data files with valid content
    bool foundFebFile = false;  // Found matching tester FEB file
    bool logExists = false;     // Main log file exists
    
    // Trim/Conn files specific counters
    int electronCount = 0;      // Electron files found
    int holeCount = 0;          // Hole files found
    
    // Pscan files specific counters
    int electronTxtCount = 0;   // Electron text files
    int holeTxtCount = 0;       // Hole text files
    int electronRootCount = 0;  // Electron root files
    int holeRootCount = 0;      // Hole root files
};

/*
 * GlobalState Structure:
 * Tracks overall validation state across all directories
 */
struct GlobalState {
    std::vector<std::string> reportPages;  // Individual directory reports
    std::string globalSummary;             // Consolidated summary
    int passedDirs = 0;                    // Count of passed directories
    int passedWithIssuesDirs = 0;          // Count of passed with issues
    int failedDirs = 0;                    // Count of failed directories
    std::string currentLadder;             // Current working directory name
};

GlobalState gState;  // Global state instance

// ===================================================================
// Helper Functions
// ===================================================================

/*
 * DirectoryExists:
 * Checks if a directory exists at the given path
 */
bool DirectoryExists(const TString& path) {
    return !gSystem->AccessPathName(path, kFileExists);
}

/*
 * GetDirectoryListing:
 * Returns a list of files in the specified directory
 */
TList* GetDirectoryListing(const TString& path) {
    TSystemDirectory dir(path, path);
    return dir.GetListOfFiles();
}

/*
 * CheckFileAccess:
 * Verifies if a file can be opened and tracks failures
 */
bool CheckFileAccess(const TString& filePath, std::vector<std::string>& errorList) {
    std::ifstream file(filePath.Data());
    if (!file.is_open()) {
        errorList.push_back(gSystem->BaseName(filePath.Data()));
        return false;
    }
    file.close();
    return true;
}

/*
 * CheckRootFile:
 * Verifies if a ROOT file can be opened properly
 */
bool CheckRootFile(const TString& filePath, std::vector<std::string>& errorList) {
    TFile* file = TFile::Open(filePath, "READ");
    if (!file || file->IsZombie()) {
        errorList.push_back(gSystem->BaseName(filePath.Data()));
        if (file) delete file;
        return false;
    }
    delete file;
    return true;
}

/*
 * CheckDataFileContent:
 * Verifies if the content of a .dat file meets expected patterns
 */
bool CheckDataFileContent(const char* filePath) {
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

// ===================================================================
// Validation Functions
// ===================================================================

/*
 * CheckLogFiles:
 * Validates the log files and data files in a test directory
 * 
 * Parameters:
 *   targetDir - Name of the directory to validate
 * 
 * Returns:
 *   ValidationResult containing all findings
 * 
 * Checks:
 * 1. Directory existence
 * 2. Presence of main log file (<dir>_log.log)
 * 3. Data files (*_data.dat) existence and content
 * 4. Matching tester FEB files (tester_febs_*)
 * 5. File naming conventions with timestamps
 * 6. File accessibility and content validity
 */
ValidationResult CheckLogFiles(const char* targetDir) {
    ValidationResult result;
    TString currentDir = gSystem->pwd();
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of target directory
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory does not exist: " << fullTargetPath << std::endl;
        result.flags |= FLAG_DIR_MISSING;
        return result; // Can't proceed if directory is missing
    }

    /* Check for existence and accessibility of the primary log file */
    TString logFilePath = TString::Format("%s/%s_log.log", fullTargetPath.Data(), targetDir);
    if (!gSystem->AccessPathName(logFilePath, kFileExists)) {
        result.logExists = true;
        if (!CheckFileAccess(logFilePath, result.openErrorFiles)) {
            std::cerr << "Error: Cannot open log file: " << logFilePath << std::endl;
            result.flags |= FLAG_FILE_OPEN;
        }
    } else {
        std::cerr << "Error: Log file does not exist: " << logFilePath << std::endl;
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

    // Structure to store discovered files with their metadata
    struct FileInfo {
        TString fullPath;         // Complete file path
        TString fileName;         // Just the filename
        TString dateTimePattern;  // Extracted timestamp (YYMMDD_HHMM)
        bool isSpecialCase;       // Flag for files without timestamp
    };

    std::vector<FileInfo> dataFiles;    // Stores all found data files
    std::vector<FileInfo> testerFiles;  // Stores all found tester FEB files

    // ===================================================================
    // FILE PROCESSING LOOP
    // ===================================================================
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();

        // Skip directories and special files
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
    
        TString fullFilePath = fullTargetPath + "/" + fileName;
        bool isExpectedFile = false;

        // Skip the log file we already processed
        if (fileName == TString::Format("%s_log.log", targetDir)) {
            continue;
        }

        // Check for data files (two possible formats)
        if (fileName.BeginsWith(targetDir) && fileName.EndsWith("_data.dat")) {
            isExpectedFile = true;
            result.dataFileCount++;
            
            FileInfo info;
            info.fullPath = fullFilePath;
            info.fileName = fileName;
            info.isSpecialCase = false;

            // Check for standard format: "folder_YYMMDD_HHMM_data.dat"
            if (fileName.Length() == (15 + 1 + 6 + 1 + 4 + 9)) { // 15 (folder) + _ + 6 (YYMMDD) + _ + 4 (HHMM) + _data.dat
                info.dateTimePattern = fileName(16, 11); // Extract YYMMDD_HHMM
            } 
            // Check for special case: "folder_data.dat"
            else if (fileName == TString::Format("%s_data.dat", targetDir)) {
                info.isSpecialCase = true;
            } else {
                std::cerr << "Warning: Unexpected data file format: " << fileName << std::endl;
                result.unexpectedFiles.push_back(fileName.Data());
                result.flags |= FLAG_UNEXPECTED_FILES;
                continue; // Skip further processing for malformed names
            }
            
            dataFiles.push_back(info);
            
            // DATA FILE CONTENT VALIDATION
            std::ifstream f_data(fullFilePath.Data(), std::ios::binary | std::ios::ate);
            if (!f_data.is_open()) {
                std::cerr << "Error: Cannot open data file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN;
            } else {
                // Check file size first (quick check)
                std::streampos size = f_data.tellg();
                f_data.close();
                
                if (size > 0) {
                    result.nonEmptyDataCount++;
                    // Perform detailed content validation
                    if (CheckDataFileContent(fullFilePath.Data())) {
                        result.validDataCount++;
                    } else {
                        std::cerr << "Error: Invalid content in data file: " << fullFilePath << std::endl;
                        result.invalidFiles.push_back(fileName.Data());
                        result.flags |= FLAG_DATA_INVALID;
                    }
                } else {
                    std::cerr << "Warning: Empty data file: " << fullFilePath << std::endl;
                    result.emptyFiles.push_back(fileName.Data());
                    result.flags |= FLAG_DATA_EMPTY;
                }
            }
        }
        // TESTER FEB FILE PROCESSING
        else if (fileName.BeginsWith("tester_febs_") && fileName.Contains("_arr_")) {
            isExpectedFile = true;
            
            FileInfo info;
            info.fullPath = fullFilePath;
            info.fileName = fileName;
            info.isSpecialCase = false;
            
            // Extract timestamp from FEB filename (format: tester_febs_*_arr_YYMMDD_HHMM*)
            Ssiz_t arrPos = fileName.Index("_arr_");
            if (arrPos != kNPOS && fileName.Length() >= arrPos + 5 + 11) {
                info.dateTimePattern = fileName(arrPos + 5, 11); // Extract YYMMDD_HHMM
            } else {
                std::cerr << "Warning: Invalid FEB file format: " << fileName << std::endl;
                result.unexpectedFiles.push_back(fileName.Data());
                result.flags |= FLAG_UNEXPECTED_FILES;
                continue;
            }
            
            testerFiles.push_back(info);
        }

        // UNEXPECTED FILE HANDLING
        if (!isExpectedFile) {
            std::cerr << "Warning: Unexpected file found: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES;
        }
    }

    delete files; // Clean up directory listing

    // ===================================================================
    // DATA-TESTER FILE MATCHING
    // ===================================================================
    /* This complex matching handles two scenarios:
     * 1. Normal case: Data file with timestamp matches tester file with same timestamp
     * 2. Special case: Untimestamped data file matches with oldest available tester file
     */
    
    // Sort tester files chronologically (oldest first)
    std::sort(testerFiles.begin(), testerFiles.end(), [](const FileInfo& a, const FileInfo& b) {
        return a.dateTimePattern < b.dateTimePattern;
    });

    // Track which tester files have been matched
    std::vector<bool> matchedTesters(testerFiles.size(), false);
    int specialCaseMatchIndex = -1;

    for (auto& dataFile : dataFiles) {
        bool foundMatch = false;
        
        // Special case handling (data file without timestamp)
        if (dataFile.isSpecialCase) {
            // Find the oldest unmatched tester file
            for (size_t i = 0; i < testerFiles.size(); i++) {
                if (!matchedTesters[i]) {
                    matchedTesters[i] = true;
                    foundMatch = true;
                    specialCaseMatchIndex = i;
                    std::cerr << "Info: Special case data file " << dataFile.fileName 
                              << " matched with oldest available tester file " << testerFiles[i].fileName 
                              << " (pattern: " << testerFiles[i].dateTimePattern << ")" << std::endl;
                    break;
                }
            }
        } else {
            // Normal case - match by exact timestamp
            for (size_t i = 0; i < testerFiles.size(); i++) {
                if (!matchedTesters[i] && dataFile.dateTimePattern == testerFiles[i].dateTimePattern) {
                    matchedTesters[i] = true;
                    foundMatch = true;
                    std::cerr << "Info: Data file " << dataFile.fileName 
                              << " matched with tester file " << testerFiles[i].fileName 
                              << " (pattern: " << dataFile.dateTimePattern << ")" << std::endl;
                    break;
                }
            }
        }
        
        // No match found for this data file
        if (!foundMatch) {
            std::cerr << "Error: No matching tester file found for data file: " 
                      << dataFile.fileName;
            if (!dataFile.isSpecialCase) {
                std::cerr << " (pattern: " << dataFile.dateTimePattern << ")";
            }
            std::cerr << std::endl;
            result.flags |= FLAG_NO_FEB_FILE;
        }
    }

    // Final check if we have data files but no FEB files at all
    if (dataFiles.size() > 0 && testerFiles.size() == 0) {
        std::cerr << "Error: No FEB files found in directory" << std::endl;
        result.flags |= FLAG_NO_FEB_FILE;
    }

    // ===================================================================
    // REPORT GENERATION
    // ===================================================================
    std::cout << "\n===== Log Files Status =====" << std::endl;
    std::cout << "Log file:         " << (result.logExists ? "FOUND" : "MISSING") 
              << (result.flags & FLAG_FILE_OPEN ? " (OPEN ERROR)" : "") << std::endl;
    std::cout << "Data files:       " << result.dataFileCount << " found | "
              << (result.flags & FLAG_DATA_MISSING ? "NONE" : 
                 (result.flags & FLAG_DATA_EMPTY) ? "SOME EMPTY" :
                 (result.flags & FLAG_DATA_INVALID) ? "SOME INVALID" : "VALID") << std::endl;
    std::cout << "Non-empty files:  " << result.nonEmptyDataCount << "/" << result.dataFileCount << std::endl;
    std::cout << "Valid files:      " << result.validDataCount << "/" << result.dataFileCount << std::endl;
    std::cout << "Tester FEB files: " << testerFiles.size() << " found | "
              << (result.flags & FLAG_NO_FEB_FILE ? "MISSING MATCHES" : "ALL MATCHED") << std::endl;

    if (!result.emptyFiles.empty()) {
        std::cout << "\n===== Empty Data Files =====" << std::endl;
        for (const auto& file : result.emptyFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.invalidFiles.empty()) {
        std::cout << "\n===== Invalid Data Files =====" << std::endl;
        for (const auto& file : result.invalidFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.openErrorFiles.empty()) {
        std::cout << "\n===== File Open Errors =====" << std::endl;
        for (const auto& file : result.openErrorFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.unexpectedFiles.empty()) {
        std::cout << "\n===== Unexpected Files =====" << std::endl;
        for (const auto& file : result.unexpectedFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    std::cout << "\nSummary: ";
    if (result.flags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (result.flags & FLAG_DIR_MISSING) std::cout << "[DIR MISSING] ";
        if (result.flags & FLAG_LOG_MISSING) std::cout << "[LOG MISSING] ";
        if (result.flags & FLAG_DATA_MISSING) std::cout << "[DATA MISSING] ";
        if (result.flags & FLAG_NO_FEB_FILE) std::cout << "[NO FEB FILES] ";
        if (result.flags & FLAG_FILE_OPEN) std::cout << "[FILE OPEN ERROR] ";
        if (result.flags & FLAG_DATA_EMPTY) std::cout << "[DATA EMPTY] ";
        if (result.flags & FLAG_DATA_INVALID) std::cout << "[DATA INVALID] ";
        if (result.flags & FLAG_UNEXPECTED_FILES) std::cout << "[UNEXPECTED FILES]";
    }
    std::cout << std::endl;

    return result;
}

/*
 * CheckTrimFiles:
 * Validates the trim adjustment files in the 'trim_files' subdirectory.
 * Ensures proper count and naming convention of electron and hole trim files.
 * 
 * Parameters:
 *   targetDir - Parent directory containing trim_files
 * 
 * Returns:
 *   ValidationResult with trim file findings
 * 
 * Checks:
 * 1. trim_files subdirectory existence
 * 2. Exactly 8 electron files (*_elect.txt)
 * 3. Exactly 8 hole files (*_holes.txt)
 * 4. Proper HW index format (0-7) in filenames
 * 5. No duplicate HW indices
 * 6. File accessibility
 * 7. No unexpected files in directory
 */
ValidationResult CheckTrimFiles(const char* targetDir) {
    ValidationResult result;
    TString currentDir = gSystem->pwd();
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    TString trimDirPath = TString::Format("%s/%s/trim_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of 'trim_files' directory
    if (gSystem->AccessPathName(trimDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'trim_files' does not exist in target folder!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << trimDirPath << std::endl;
        result.flags |= FLAG_TRIM_FOLDER_MISSING;
        return result;
    }

    // DIRECTORY SCANNING INITIALIZATION
    TList* files = GetDirectoryListing(trimDirPath);
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << trimDirPath << std::endl;
        result.flags |= FLAG_DIR_ACCESS_TRIM;
        return result;
    }

    // ===================================================================
    // HW INDEX TRACKING SETUP
    // ===================================================================
    /* We need exactly 8 files each for electrons and holes (HW indices 0-7) */
    std::set<int> foundElectronIndices;  // Tracks found electron HW indices
    std::set<int> foundHoleIndices;      // Tracks found hole HW indices

    // ===================================================================
    // FILE PROCESSING LOOP
    // ===================================================================
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();

        // Skip directories and special files
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        TString fullFilePath = trimDirPath + "/" + fileName;

        // =================================================================
        // ELECTRON FILE PROCESSING
        // =================================================================
        if (fileName.EndsWith("_elect.txt")) {
            // Extract HW index from filename (format: *_HW_X_SET_*_elect.txt)
            Ssiz_t hwPos = fileName.Index("_HW_");
            Ssiz_t setPos = fileName.Index("_SET_", hwPos+4);
            
            // Validate filename format
            if (hwPos == -1 || setPos == -1 || setPos <= hwPos+4) {
                std::cerr << "Error: Invalid electron file name format: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            // Extract and validate HW index
            TString indexStr = fileName(hwPos+4, setPos-(hwPos+4));
            bool isNumber = true;
            for (int i = 0; i < indexStr.Length(); i++) {
                if (!isdigit(indexStr[i])) {
                    isNumber = false;
                    break;
                }
            }
            
            if (!isNumber) {
                std::cerr << "Error: Invalid HW index in electron file: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            int hwIndex = atoi(indexStr.Data());
            // Validate HW index range (0-7)
            if (hwIndex < 0 || hwIndex > 7) {
                std::cerr << "Error: HW index out of range (0-7) in electron file: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            // Check for duplicate indices
            if (foundElectronIndices.find(hwIndex) != foundElectronIndices.end()) {
                std::cerr << "Error: Duplicate HW index " << hwIndex << " in electron files" << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            // Record valid file
            foundElectronIndices.insert(hwIndex);
            result.electronCount++;
            
            // Check file accessibility and content
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_TRIM;
            } else {
                // Quick empty file check
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        }
        // =================================================================
        // HOLE FILE PROCESSING (similar to electron files)
        // =================================================================}
         else if (fileName.EndsWith("_holes.txt")) {
            // Extract HW index from filename (format: *_HW_X_SET_*_hole.txt)
            Ssiz_t hwPos = fileName.Index("_HW_");
            Ssiz_t setPos = fileName.Index("_SET_", hwPos+4);
            
            if (hwPos == -1 || setPos == -1 || setPos <= hwPos+4) {
                std::cerr << "Error: Invalid hole file name format: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue; // Skip malformed filenames
            }
            
            // Extract and validate HW index
            TString indexStr = fileName(hwPos+4, setPos-(hwPos+4));
            bool isNumber = true;
            for (int i = 0; i < indexStr.Length(); i++) {
                if (!isdigit(indexStr[i])) {
                    isNumber = false;
                    break;
                }
            }
            
            if (!isNumber) {
                std::cerr << "Error: Invalid HW index in hole file: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            int hwIndex = atoi(indexStr.Data());
            // Validate HW index range (0-7)
            if (hwIndex < 0 || hwIndex > 7) {
                std::cerr << "Error: HW index out of range (0-7) in hole file: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            // Check for duplicate indices
            if (foundHoleIndices.find(hwIndex) != foundHoleIndices.end()) {
                std::cerr << "Error: Duplicate HW index " << hwIndex << " in hole files" << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            // Record valid file
            foundHoleIndices.insert(hwIndex);
            result.holeCount++;
            
            // Check file accessibility and content
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_TRIM;
            } else {
                // Quick empty file check
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        }
        // =================================================================
        // UNEXPECTED FILE HANDLING
        // =================================================================
        else {
            std::cerr << "Warning: Unexpected file in trim_files: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES_TRIM;
        }
    }

    delete files; // Clean up directory listing

    // ===================================================================
    // FINAL COUNT VALIDATION
    // ===================================================================
    /* Verify we have exactly 8 files for each type with unique indices */
    if (result.electronCount != 8) {
        std::cerr << "Error: Incorrect number of electron files: " << result.electronCount << "/8" << std::endl;
        result.flags |= FLAG_ELECTRON_COUNT_TRIM;
    } else if (foundElectronIndices.size() != 8) {
        std::cerr << "Error: Missing or duplicate HW indices in electron files" << std::endl;
        result.flags |= FLAG_ELECTRON_COUNT_TRIM;
    }
    
    if (result.holeCount != 8) {
        std::cerr << "Error: Incorrect number of hole files: " << result.holeCount << "/8" << std::endl;
        result.flags |= FLAG_HOLE_COUNT_TRIM;
    } else if (foundHoleIndices.size() != 8) {
        std::cerr << "Error: Missing or duplicate HW indices in hole files" << std::endl;
        result.flags |= FLAG_HOLE_COUNT_TRIM;
    }

    // Generate detailed report
    std::cout << "\n===== Trim Files Status =====" << std::endl;
    std::cout << "Electron files: " << result.electronCount << "/8 | "
              << ((result.flags & FLAG_ELECTRON_COUNT_TRIM) ? "FAIL" : "OK")
              << (result.electronCount < 8 ? " (UNDER)" : (result.electronCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "Hole files:     " << result.holeCount << "/8 | "
              << ((result.flags & FLAG_HOLE_COUNT_TRIM) ? "FAIL" : "OK")
              << (result.holeCount < 8 ? " (UNDER)" : (result.holeCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "File name format: " << (result.invalidFiles.empty() ? "ALL VALID" : "ERRORS DETECTED") << std::endl;
    std::cout << "File accessibility: " << (result.openErrorFiles.empty() ? "ALL OK" : "ERRORS") << std::endl;

    if (!result.emptyFiles.empty()) {
        std::cout << "\n===== Empty Files =====" << std::endl;
        for (const auto& file : result.emptyFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.invalidFiles.empty()) {
        std::cout << "\n===== Invalid Files (Bad Name Format) =====" << std::endl;
        for (const auto& file : result.invalidFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.openErrorFiles.empty()) {
        std::cout << "\n===== File Open Errors =====" << std::endl;
        for (const auto& file : result.openErrorFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.unexpectedFiles.empty()) {
        std::cout << "\n===== Unexpected Files =====" << std::endl;
        for (const auto& file : result.unexpectedFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    std::cout << "\nSummary: ";
    if (result.flags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (result.flags & FLAG_TRIM_FOLDER_MISSING) std::cout << "[FOLDER MISSING] ";
        if (result.flags & FLAG_DIR_ACCESS_TRIM) std::cout << "[DIR ACCESS ERROR] ";
        if (result.flags & FLAG_ELECTRON_COUNT_TRIM) std::cout << "[ELECTRON COUNT] ";
        if (result.flags & FLAG_HOLE_COUNT_TRIM) std::cout << "[HOLE COUNT] ";
        if (result.flags & FLAG_FILE_OPEN_TRIM) std::cout << "[FILE OPEN ERROR] ";
        if (result.flags & FLAG_DATA_INVALID) std::cout << "[INVALID FILENAME] ";
        if (result.flags & FLAG_UNEXPECTED_FILES_TRIM) std::cout << "[UNEXPECTED FILES]";
    }
    std::cout << std::endl;

    return result;
}

/*
 * CheckPscanFiles:
 * Validates the parameter scan (pscan) files in the 'pscan_files' subdirectory.
 * This includes checking for required module files and per-HW electron/hole files.
 * 
 * Parameters:
 *   targetDir - Parent directory containing pscan_files
 * 
 * Returns:
 *   ValidationResult with pscan file findings
 * 
 * Checks:
 * 1. pscan_files subdirectory existence
 * 2. Module test files (root/txt/pdf)
 * 3. Exactly 8 electron text files (*_elect.txt)
 * 4. Exactly 8 hole text files (*_holes.txt) 
 * 5. Exactly 8 electron root files (*_elect.root)
 * 6. Exactly 8 hole root files (*_holes.root)
 * 7. File accessibility and validity
 * 8. No unexpected files in directory
 */
ValidationResult CheckPscanFiles(const char* targetDir) {
    ValidationResult result;
    TString currentDir = gSystem->pwd();
    TString pscanDirPath = TString::Format("%s/%s/pscan_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of the required 'pscan_files' subdirectory */
    if (gSystem->AccessPathName(pscanDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'pscan_files' does not exist!" << std::endl;
        std::cerr << "Expected path: " << pscanDirPath << std::endl;
        result.flags |= FLAG_PSCAN_FOLDER_MISSING;
        return result; // Cannot proceed without this directory
    }

    // ===================================================================
    // MODULE TEST FILES VALIDATION
    // ===================================================================
    /* Check required module-level files:
     * 1. module_test_<dir>.root - ROOT format results
     * 2. module_test_<dir>.txt  - Text summary
     * 3. module_test_<dir>.pdf  - Report PDF
     */
    TString moduleRoot = TString::Format("%s/module_test_%s.root", pscanDirPath.Data(), targetDir);
    TString moduleTxt = TString::Format("%s/module_test_%s.txt", pscanDirPath.Data(), targetDir);
    TString modulePdf = TString::Format("%s/module_test_%s.pdf", pscanDirPath.Data(), targetDir);

    // Check module ROOT file
    if (gSystem->AccessPathName(moduleRoot, kFileExists)) {
        std::cerr << "Error: Module test root file does not exist: " << moduleRoot << std::endl;
        result.moduleErrorFiles.push_back(gSystem->BaseName(moduleRoot.Data()));
        result.flags |= FLAG_MODULE_ROOT;
    } else {
        // Verify ROOT file can be opened and is valid
        TFile* f_root = TFile::Open(moduleRoot, "READ");
        if (!f_root || f_root->IsZombie()) {
            std::cerr << "Error: Cannot open module test root file: " << moduleRoot << std::endl;
            result.moduleErrorFiles.push_back(gSystem->BaseName(moduleRoot.Data()));
            result.flags |= FLAG_MODULE_ROOT;
        }
        if (f_root) f_root->Close();
    }

    // Check module TXT file
    if (gSystem->AccessPathName(moduleTxt, kFileExists)) {
        std::cerr << "Error: Module test txt file does not exist: " << moduleTxt << std::endl;
        result.moduleErrorFiles.push_back(gSystem->BaseName(moduleTxt.Data()));
        result.flags |= FLAG_MODULE_TXT;
    } else {
        std::ifstream f_txt(moduleTxt.Data());
        if (!f_txt.is_open()) {
            std::cerr << "Error: Cannot open module test txt file: " << moduleTxt << std::endl;
            result.moduleErrorFiles.push_back(gSystem->BaseName(moduleTxt.Data()));
            result.flags |= FLAG_MODULE_TXT;
        } else {
            // Check if file is empty
            f_txt.seekg(0, std::ios::end);
            if (f_txt.tellg() == 0) {
                result.emptyFiles.push_back(gSystem->BaseName(moduleTxt.Data()));
            }
            f_txt.close();
        }
    }

    // Check module PDF file (existence only)
    if (gSystem->AccessPathName(modulePdf, kFileExists)) {
        std::cerr << "Error: Module test pdf file does not exist: " << modulePdf << std::endl;
        result.moduleErrorFiles.push_back(gSystem->BaseName(modulePdf.Data()));
        result.flags |= FLAG_MODULE_PDF;
    }

    // ===================================================================
    // PER-HW FILES VALIDATION
    // ===================================================================
    TList* files = GetDirectoryListing(pscanDirPath);
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << pscanDirPath << std::endl;
        result.flags |= FLAG_DIR_ACCESS_PSCAN;
        return result;
    }

    // List of acceptable auxiliary files that aren't per-HW files
    std::vector<std::string> acceptableAuxFiles = {
        "module_test_SETUP.root",
        "module_test_SETUP.txt",
        "module_test_SETUP.pdf"
    };

    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        TString fullFilePath = pscanDirPath + "/" + fileName;
        bool isExpected = false;

        // =================================================================
        // ELECTRON TEXT FILES PROCESSING
        // =================================================================
        if (fileName.EndsWith("_elect.txt")) {
            result.electronTxtCount++;
            isExpected = true;

            // Check file accessibility and content
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron txt file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_PSCAN;
            } else {
                // Check if file is empty
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        }

        // =================================================================
        // HOLE TEXT FILES PROCESSING
        // =================================================================
        else if (fileName.EndsWith("_holes.txt")) {
            result.holeTxtCount++;
            isExpected = true;

            // Check file accessibility and content
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole txt file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_PSCAN;
            } else {
                // Check if file is empty
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        }

        // =================================================================
        // ELECTRON ROOT FILES PROCESSING
        // =================================================================
        else if (fileName.EndsWith("_elect.root")) {
            result.electronRootCount++;
            isExpected = true;

            // Validate ROOT file can be opened
            TFile* rootFile = TFile::Open(fullFilePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open electron root file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_PSCAN;
            }
            if (rootFile) rootFile->Close();
        }
        
        // =================================================================
        // HOLE ROOT FILES PROCESSING
        // =================================================================
        else if (fileName.EndsWith("_holes.root")) {
            result.holeRootCount++;
            isExpected = true;

            // Validate ROOT file can be opened
            TFile* rootFile = TFile::Open(fullFilePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open hole root file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_PSCAN;
            }
            if (rootFile) rootFile->Close();
        }

        // =================================================================
        // MODULE/ACCEPTABLE AUX FILES CHECK
        // =================================================================
        else {
            // Check if it's a module file or acceptable aux file
            TString modulePrefix = TString::Format("module_test_%s", targetDir);
            if (fileName.BeginsWith(modulePrefix) && 
                (fileName.EndsWith(".root") || fileName.EndsWith(".txt") || fileName.EndsWith(".pdf"))) {
                isExpected = true;
            } else {
                // Check against acceptable auxiliary files
                for (const auto& auxFile : acceptableAuxFiles) {
                    if (fileName == auxFile) {
                        isExpected = true;
                        break;
                    }
                }
            }
        }

        // =================================================================
        // UNEXPECTED FILES HANDLING
        // =================================================================
        if (!isExpected) {
            std::cerr << "Warning: Unexpected file in pscan_files: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES_PSCAN;
        }
    }

    delete files; // Clean up directory listing

    // ===================================================================
    // FINAL COUNT VALIDATION
    // ===================================================================
    /* Verify we have exactly 8 files of each required type */
    if (result.electronTxtCount != 8) {
        std::cerr << "Error: Incorrect number of electron txt files: " << result.electronTxtCount << "/8" << std::endl;
        result.flags |= FLAG_ELECTRON_TXT;
    }
    if (result.holeTxtCount != 8) {
        std::cerr << "Error: Incorrect number of hole txt files: " << result.holeTxtCount << "/8" << std::endl;
        result.flags |= FLAG_HOLE_TXT;
    }
    if (result.electronRootCount != 8) {
        std::cerr << "Error: Incorrect number of electron root files: " << result.electronRootCount << "/8" << std::endl;
        result.flags |= FLAG_ELECTRON_ROOT;
    }
    if (result.holeRootCount != 8) {
        std::cerr << "Error: Incorrect number of hole root files: " << result.holeRootCount << "/8" << std::endl;
        result.flags |= FLAG_HOLE_ROOT;
    }

    // Generate detailed report
    std::cout << "\n===== Pscan Files Status =====" << std::endl;
    std::cout << "Electron text files: " << result.electronTxtCount << "/8 | "
              << ((result.flags & FLAG_ELECTRON_TXT) ? "FAIL" : "OK") 
              << (result.electronTxtCount < 8 ? " (UNDER)" : (result.electronTxtCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "Hole text files:     " << result.holeTxtCount << "/8 | "
              << ((result.flags & FLAG_HOLE_TXT) ? "FAIL" : "OK")
              << (result.holeTxtCount < 8 ? " (UNDER)" : (result.holeTxtCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "Electron ROOT files: " << result.electronRootCount << "/8 | "
              << ((result.flags & FLAG_ELECTRON_ROOT) ? "FAIL" : "OK")
              << (result.electronRootCount < 8 ? " (UNDER)" : (result.electronRootCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "Hole ROOT files:     " << result.holeRootCount << "/8 | "
              << ((result.flags & FLAG_HOLE_ROOT) ? "FAIL" : "OK")
              << (result.holeRootCount < 8 ? " (UNDER)" : (result.holeRootCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "Module test root:  " << (result.flags & FLAG_MODULE_ROOT ? "ERROR" : "OK") << std::endl;
    std::cout << "Module test txt:   " << (result.flags & FLAG_MODULE_TXT ? "ERROR" : "OK") << std::endl;
    std::cout << "Module test pdf:   " << (result.flags & FLAG_MODULE_PDF ? "MISSING" : "OK") << std::endl;
    std::cout << "File accessibility:    " << (result.openErrorFiles.empty() ? "ALL OK" : "ERRORS DETECTED") << std::endl;

    if (!result.emptyFiles.empty()) {
        std::cout << "\n===== Empty Files =====" << std::endl;
        for (const auto& file : result.emptyFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.moduleErrorFiles.empty()) {
        std::cout << "\n===== Module Test Errors =====" << std::endl;
        for (const auto& file : result.moduleErrorFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.openErrorFiles.empty()) {
        std::cout << "\n===== File Open Errors =====" << std::endl;
        for (const auto& file : result.openErrorFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.unexpectedFiles.empty()) {
        std::cout << "\n===== Unexpected Files =====" << std::endl;
        for (const auto& file : result.unexpectedFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    std::cout << "\nSummary: ";
    if (result.flags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (result.flags & FLAG_PSCAN_FOLDER_MISSING) std::cout << "[FOLDER MISSING] ";
        if (result.flags & FLAG_DIR_ACCESS_PSCAN) std::cout << "[DIR ACCESS ERROR] ";
        if (result.flags & FLAG_ELECTRON_TXT) std::cout << "[ELECTRON TXT COUNT] ";
        if (result.flags & FLAG_HOLE_TXT) std::cout << "[HOLE TXT COUNT] ";
        if (result.flags & FLAG_ELECTRON_ROOT) std::cout << "[ELECTRON ROOT COUNT] ";
        if (result.flags & FLAG_HOLE_ROOT) std::cout << "[HOLE ROOT COUNT] ";
        if (result.flags & FLAG_FILE_OPEN_PSCAN) std::cout << "[FILE OPEN ERROR] ";
        if (result.flags & FLAG_MODULE_ROOT) std::cout << "[MODULE ROOT ERROR] ";
        if (result.flags & FLAG_MODULE_TXT) std::cout << "[MODULE TXT ERROR] ";
        if (result.flags & FLAG_MODULE_PDF) std::cout << "[MODULE PDF MISSING] ";
        if (result.flags & FLAG_UNEXPECTED_FILES_PSCAN) std::cout << "[UNEXPECTED FILES]";
    }
    std::cout << std::endl;

    return result;
}

/*
 * CheckConnFiles:
 * Validates connection test files in the 'conn_check_files' subdirectory.
 * Ensures proper count and accessibility of electron and hole connection files.
 * 
 * Parameters:
 *   targetDir - Parent directory containing conn_check_files
 * 
 * Returns:
 *   ValidationResult with connection file findings
 * 
 * Checks:
 * 1. conn_check_files subdirectory existence
 * 2. Exactly 8 electron files (*_elect.txt)
 * 3. Exactly 8 hole files (*_holes.txt)
 * 4. File accessibility
 * 5. No unexpected files in directory
 */
ValidationResult CheckConnFiles(const char* targetDir) {
    ValidationResult result;
    TString currentDir = gSystem->pwd();
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    TString connDirPath = TString::Format("%s/%s/conn_check_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of the required 'conn_check_files' subdirectory */
    if (gSystem->AccessPathName(connDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'conn_check_files' does not exist!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << connDirPath << std::endl;
        result.flags |= FLAG_CONN_FOLDER_MISSING;
        return result; // Cannot proceed without this directory
    }

    // ===================================================================
    // DIRECTORY SCANNING INITIALIZATION
    // ===================================================================
    TList* files = GetDirectoryListing(connDirPath);
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << connDirPath << std::endl;
        result.flags |= FLAG_DIR_ACCESS;
        return result;
    }

    // ===================================================================
    // FILE PROCESSING LOOP
    // ===================================================================
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();

        // Skip directories and special files
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        TString fullFilePath = connDirPath + "/" + fileName;

        // =================================================================
        // ELECTRON FILE PROCESSING
        // =================================================================
        if (fileName.EndsWith("_elect.txt")) {
            result.electronCount++;

            // Check file accessibility
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_CONN;
            } else {
                // Check if file is empty
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        }

        // =================================================================
        // HOLE FILE PROCESSING
        // =================================================================
        else if (fileName.EndsWith("_holes.txt")) {
            result.holeCount++;

            // Check file accessibility
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_CONN;
            } else {
                // Check if file is empty
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        }

        // =================================================================
        // UNEXPECTED FILE HANDLING
        // =================================================================
        else {
            std::cerr << "Warning: Unexpected file in conn_check_files: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES_CONN;
        }
    }

    delete files; // Clean up directory listing

    // ===================================================================
    // FINAL COUNT VALIDATION
    // ===================================================================
    /* Verify we have exactly 8 files for each type */
    if (result.electronCount != 8) {
        std::cerr << "Error: Incorrect number of electron files: " << result.electronCount << "/8" << std::endl;
        result.flags |= FLAG_ELECTRON_COUNT;
    }
    if (result.holeCount != 8) {
        std::cerr << "Error: Incorrect number of hole files: " << result.holeCount << "/8" << std::endl;
        result.flags |= FLAG_HOLE_COUNT;
    }

    // Generate detailed report
    std::cout << "\n===== Connection Files Status =====" << std::endl;
    std::cout << "Electron files: " << result.electronCount << "/8 | "
              << ((result.flags & FLAG_ELECTRON_COUNT) ? "FAIL" : "OK") 
              << (result.electronCount < 8 ? " (UNDER)" : (result.electronCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "Hole files:     " << result.holeCount << "/8 | "
              << ((result.flags & FLAG_HOLE_COUNT) ? "FAIL" : "OK")
              << (result.holeCount < 8 ? " (UNDER)" : (result.holeCount > 8 ? " (OVER)" : "")) << std::endl;
    std::cout << "File accessibility: " << (result.openErrorFiles.empty() ? "ALL OK" : "ERRORS DETECTED") << std::endl;

    if (!result.emptyFiles.empty()) {
        std::cout << "\n===== Empty Files =====" << std::endl;
        for (const auto& file : result.emptyFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.openErrorFiles.empty()) {
        std::cout << "\n===== File Open Errors =====" << std::endl;
        for (const auto& file : result.openErrorFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    if (!result.unexpectedFiles.empty()) {
        std::cout << "\n===== Unexpected Files =====" << std::endl;
        for (const auto& file : result.unexpectedFiles) {
            std::cout << " - " << file << std::endl;
        }
    }

    std::cout << "\nSummary: ";
    if (result.flags == 0) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (result.flags & FLAG_CONN_FOLDER_MISSING) std::cout << "[FOLDER MISSING] ";
        if (result.flags & FLAG_DIR_ACCESS) std::cout << "[DIR ACCESS ERROR] ";
        if (result.flags & FLAG_ELECTRON_COUNT) std::cout << "[ELECTRON COUNT] ";
        if (result.flags & FLAG_HOLE_COUNT) std::cout << "[HOLE COUNT] ";
        if (result.flags & FLAG_FILE_OPEN_CONN) std::cout << "[FILE OPEN ERROR] ";
        if (result.flags & FLAG_UNEXPECTED_FILES_CONN) std::cout << "[UNEXPECTED FILES]";
    }
    std::cout << std::endl;

    return result;
}

// ===================================================================
// Reporting Functions
// ===================================================================

/*
 * GenerateReportPage:
 * Creates a detailed validation report for a single test directory by combining results
 * from all validation checks (log, trim, pscan, and connection files).
 * 
 * Parameters:
 *   dirName - Name of the directory being validated
 * 
 * Effects:
 * - Runs all four validation functions
 * - Determines overall directory status
 * - Updates global counters
 * - Adds formatted report to gState.reportPages
 */
void GenerateReportPage(const TString& dirName) {
    std::stringstream report; // String stream to build the report
    
    // ===================================================================
    // REPORT HEADER
    // ===================================================================
    report << "====================================================" << std::endl;
    report << "VALIDATION REPORT FOR: " << dirName << std::endl;
    report << "====================================================" << std::endl;
    
    // ===================================================================
    // RUN ALL VALIDATIONS
    // ===================================================================
    /* Execute all four validation functions for this directory */
    ValidationResult logResult = CheckLogFiles(dirName.Data());
    ValidationResult trimResult = CheckTrimFiles(dirName.Data());
    ValidationResult pscanResult = CheckPscanFiles(dirName.Data());
    ValidationResult connResult = CheckConnFiles(dirName.Data());
    
    // ===================================================================
    // DETERMINE OVERALL STATUS
    // ===================================================================
    /* Status hierarchy: FAILED > PASSED_WITH_ISSUES > PASSED */
    int dirStatus = STATUS_PASSED;
    std::string statusStr = "PASSED";
    
    // Check for critical failures first
    if (logResult.flags & (FLAG_DIR_MISSING | FLAG_LOG_MISSING | FLAG_DATA_MISSING | FLAG_NO_FEB_FILE | FLAG_FILE_OPEN | FLAG_DATA_INVALID) ||
        trimResult.flags & (FLAG_TRIM_FOLDER_MISSING | FLAG_DIR_ACCESS_TRIM | FLAG_FILE_OPEN_TRIM | FLAG_ELECTRON_COUNT_TRIM | FLAG_HOLE_COUNT_TRIM) ||
        pscanResult.flags & (FLAG_PSCAN_FOLDER_MISSING | FLAG_DIR_ACCESS_PSCAN | FLAG_FILE_OPEN_PSCAN | FLAG_ELECTRON_TXT | FLAG_HOLE_TXT | FLAG_ELECTRON_ROOT | FLAG_HOLE_ROOT | FLAG_MODULE_ROOT | FLAG_MODULE_TXT | FLAG_MODULE_PDF) ||
        connResult.flags & (FLAG_CONN_FOLDER_MISSING | FLAG_DIR_ACCESS | FLAG_FILE_OPEN_CONN | FLAG_ELECTRON_COUNT | FLAG_HOLE_COUNT)) {
        dirStatus = STATUS_FAILED;
        statusStr = "FAILED";
    }
    // Check for non-critical issues (only empty/unexpected files)
    else if (logResult.flags & (FLAG_DATA_EMPTY | FLAG_UNEXPECTED_FILES) ||
             pscanResult.flags & (FLAG_UNEXPECTED_FILES_PSCAN)) {
        dirStatus = STATUS_PASSED_WITH_ISSUES;
        statusStr = "PASSED WITH ISSUES";
    }
    
    // ===================================================================
    // UPDATE GLOBAL COUNTERS
    // ===================================================================
    switch (dirStatus) {
        case STATUS_PASSED:
            gState.passedDirs++;
            break;
        case STATUS_PASSED_WITH_ISSUES:
            gState.passedWithIssuesDirs++;
            break;
        case STATUS_FAILED:
            gState.failedDirs++;
            break;
    }
    
    // ===================================================================
    // REPORT GENERATION
    // ===================================================================
    
    // 1. STATUS HEADER
    report << "STATUS: " << statusStr << std::endl;
    
    // 2. LOG FILES SECTION
    report << "\n[LOG FILES]" << std::endl;
    report << "Data files: " << logResult.dataFileCount << " found | " 
           << (logResult.flags & FLAG_DATA_MISSING ? "NONE" : 
              (logResult.flags & FLAG_DATA_EMPTY) ? "SOME EMPTY" :
              (logResult.flags & FLAG_DATA_INVALID) ? "SOME INVALID" : "VALID") << std::endl;
    report << "Non-empty files: " << logResult.nonEmptyDataCount << "/" << logResult.dataFileCount << std::endl;
    report << "Valid files: " << logResult.validDataCount << "/" << logResult.dataFileCount << std::endl;
    report << "Tester FEB files: " << (logResult.foundFebFile ? "FOUND" : "NONE") << std::endl;
    report << "Log file: " << (logResult.logExists ? "FOUND" : "MISSING") << std::endl;
    
    // 2a. Empty log files
    if (!logResult.emptyFiles.empty()) {
        report << "\nEmpty log data files:" << std::endl;
        for (const auto& file : logResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    // 2b. Invalid log files
    if (!logResult.invalidFiles.empty()) {
        report << "\nInvalid log data files:" << std::endl;
        for (const auto& file : logResult.invalidFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    // 3. TRIM FILES SECTION
    report << "\n[TRIM FILES]" << std::endl;
    report << "Electron files: " << trimResult.electronCount << "/8" << std::endl;
    report << "Hole files: " << trimResult.holeCount << "/8" << std::endl;
    
    // 3a. Empty trim files
    if (!trimResult.emptyFiles.empty()) {
        report << "\nEmpty trim files:" << std::endl;
        for (const auto& file : trimResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    // 4. PSCAN FILES SECTION
    report << "\n[PSCAN FILES]" << std::endl;
    report << "Electron text: " << pscanResult.electronTxtCount << "/8" << std::endl;
    report << "Hole text: " << pscanResult.holeTxtCount << "/8" << std::endl;
    report << "Electron root: " << pscanResult.electronRootCount << "/8" << std::endl;
    report << "Hole root: " << pscanResult.holeRootCount << "/8" << std::endl;
    report << "Module files: " << (pscanResult.flags & (FLAG_MODULE_ROOT|FLAG_MODULE_TXT|FLAG_MODULE_PDF) ? "ERROR" : "OK") << std::endl;
    
    // 4a. Empty pscan files
    if (!pscanResult.emptyFiles.empty()) {
        report << "\nEmpty pscan files:" << std::endl;
        for (const auto& file : pscanResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    // 4b. Module file errors
    if (!pscanResult.moduleErrorFiles.empty()) {
        report << "\nModule test file errors:" << std::endl;
        for (const auto& file : pscanResult.moduleErrorFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    // 5. CONNECTION FILES SECTION
    report << "\n[CONNECTION FILES]" << std::endl;
    report << "Electron files: " << connResult.electronCount << "/8" << std::endl;
    report << "Hole files: " << connResult.holeCount << "/8" << std::endl;
    
    if (!connResult.emptyFiles.empty()) {
        report << "\nEmpty connection files:" << std::endl;
        for (const auto& file : connResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    // ===================================================================
    // ERROR SUMMARY SECTION
    // ===================================================================
    
    // 6. FILE OPEN ERRORS (combined from all validations)
    if (!logResult.openErrorFiles.empty() || !trimResult.openErrorFiles.empty() ||
        !pscanResult.openErrorFiles.empty() || !connResult.openErrorFiles.empty()) {
        report << "\n[FILE OPEN ERRORS]" << std::endl;
        
        if (!logResult.openErrorFiles.empty()) {
            report << "Log file errors:" << std::endl;
            for (const auto& file : logResult.openErrorFiles) report << "  - " << file << std::endl;
        }
        if (!trimResult.openErrorFiles.empty()) {
            report << "Trim file errors:" << std::endl;
            for (const auto& file : trimResult.openErrorFiles) report << "  - " << file << std::endl;
        }
        if (!pscanResult.openErrorFiles.empty()) {
            report << "Pscan file errors:" << std::endl;
            for (const auto& file : pscanResult.openErrorFiles) report << "  - " << file << std::endl;
        }
        if (!connResult.openErrorFiles.empty()) {
            report << "Connection file errors:" << std::endl;
            for (const auto& file : connResult.openErrorFiles) report << "  - " << file << std::endl;
        }
    }
    
    // 7. UNEXPECTED FILES (combined from all validations)
    if (!logResult.unexpectedFiles.empty() || !trimResult.unexpectedFiles.empty() ||
        !pscanResult.unexpectedFiles.empty() || !connResult.unexpectedFiles.empty()) {
        report << "\n[UNEXPECTED FILES]" << std::endl;
        
        if (!logResult.unexpectedFiles.empty()) {
            report << "Log directory unexpected files:" << std::endl;
            for (const auto& file : logResult.unexpectedFiles) report << "  - " << file << std::endl;
        }
        if (!trimResult.unexpectedFiles.empty()) {
            report << "Trim directory unexpected files:" << std::endl;
            for (const auto& file : trimResult.unexpectedFiles) report << "  - " << file << std::endl;
        }
        if (!pscanResult.unexpectedFiles.empty()) {
            report << "Pscan directory unexpected files:" << std::endl;
            for (const auto& file : pscanResult.unexpectedFiles) report << "  - " << file << std::endl;
        }
        if (!connResult.unexpectedFiles.empty()) {
            report << "Connection directory unexpected files:" << std::endl;
            for (const auto& file : connResult.unexpectedFiles) report << "  - " << file << std::endl;
        }
    }
    
    // ===================================================================
    // STORE FINAL REPORT
    // ===================================================================
    gState.reportPages.push_back(report.str());
}

/*
 * SaveTxtReport:
 * Generates and saves a complete text report containing all validation results
 * to a specified file. Includes both individual directory reports and global summary.
 *
 * Parameters:
 *   filename - Full path of the output text file to create/overwrite
 *
 * Operation:
 * 1. Creates/overwrites the specified text file
 * 2. Writes a standardized report header with metadata
 * 3. Outputs all individual directory reports
 * 4. Appends the global summary statistics
 * 5. Closes the file with proper error handling
 *
 * Output Format:
 * - Plain ASCII text with clear section headers
 * - Fixed-width formatting for alignment
 * - Human-readable status indicators
 */
void SaveTxtReport(const TString& filename) {
    // Attempt to open the output file
    std::ofstream out(filename.Data());
    
    // ===================================================================
    // FILE OPENING VALIDATION
    // ===================================================================
    if (!out.is_open()) {
        std::cerr << "Error: Could not open report file for writing: " << filename << std::endl;
        return;
    }

    // ===================================================================
    // REPORT HEADER SECTION
    // ===================================================================
    /* Standardized header with metadata */
    out << "=======================================================\n";
    out << "      EXORCISM VALIDATION REPORT - TEXT VERSION\n";
    out << "=======================================================\n\n";
    
    // System and timing information
    out << "Ladder: " << gState.currentLadder << "\n";
    out << "Report generated: " << __DATE__ << " " << __TIME__ << "\n";
    out << "Total directories processed: " 
        << (gState.passedDirs + gState.passedWithIssuesDirs + gState.failedDirs) << "\n";
    out << "-------------------------------------------------------\n\n";

    // ===================================================================
    // INDIVIDUAL DIRECTORY REPORTS
    // ===================================================================
    /* Write each directory's report in sequence */
    for (size_t i = 0; i < gState.reportPages.size(); i++) {
        // Add separator between reports
        if (i > 0) {
            out << "\n\n";
            out << "=======================================================\n";
            out << "              NEXT DIRECTORY REPORT\n";
            out << "=======================================================\n\n";
        }
        
        // Write the actual report content
        out << gState.reportPages[i];
    }

    // ===================================================================
    // GLOBAL SUMMARY SECTION
    // ===================================================================
    /* Final consolidated statistics */
    out << "\n\n";
    out << "=======================================================\n";
    out << "                 VALIDATION SUMMARY\n";
    out << "=======================================================\n\n";
    
    // Calculate success rate (handling division by zero)
    int totalDirs = gState.passedDirs + gState.passedWithIssuesDirs + gState.failedDirs;
    float successRate = (totalDirs > 0) ? 
        (100.0f * (gState.passedDirs + gState.passedWithIssuesDirs) / totalDirs) : 0.0f;

    // Summary statistics
    out << "Directories passed completely: " << gState.passedDirs << "\n";
    out << "Directories passed with issues: " << gState.passedWithIssuesDirs << "\n";
    out << "Directories failed: " << gState.failedDirs << "\n";
    out << "Overall success rate: " << std::fixed << std::setprecision(1) 
        << successRate << "%\n\n";
    
    // Add any additional global summary content
    if (!gState.globalSummary.empty()) {
        out << gState.globalSummary << "\n";
    }

    // ===================================================================
    // REPORT FOOTER
    // ===================================================================
    out << "=======================================================\n";
    out << "                 END OF REPORT\n";
    out << "=======================================================\n";

    // ===================================================================
    // FILE CLOSING AND CONFIRMATION
    // ===================================================================
    out.close();
    
    // Verify successful write operation
    if (out.fail()) {
        std::cerr << "Warning: Potential write error during report generation: " 
                  << filename << std::endl;
    } else {
        std::cout << "Text report successfully saved to: " << filename << std::endl;
    }
}

/*
 * SaveRootReport:
 * Saves all validation reports to a ROOT file format for programmatic analysis.
 * Stores each directory report as a separate TObjString and includes summary statistics.
 *
 * Parameters:
 *   filename - Full path of the output ROOT file to create/overwrite
 *
 * Operation:
 * 1. Creates a new ROOT file (overwrites existing)
 * 2. Saves each directory report as a named TObjString
 * 3. Stores global summary as a separate object
 * 4. Ensures proper file closure and error handling
 *
 * ROOT File Structure:
 * - Contains TObjString objects for each directory report
 * - Includes "GlobalSummary" TObjString
 * - Objects named systematically (Directory_0, Directory_1, etc.)
 */
void SaveRootReport(const TString& filename) {
    // ===================================================================
    // FILE CREATION AND VALIDATION
    // ===================================================================
    /* Create the ROOT file (RECREATE mode overwrites existing) */
    TFile file(filename, "RECREATE");
    
    // Verify file was created successfully
    if (file.IsZombie()) {
        std::cerr << "Error: Could not create ROOT report file: " << filename << std::endl;
        return;
    }

    // ===================================================================
    // STORE INDIVIDUAL DIRECTORY REPORTS
    // ===================================================================
    /* Save each directory report as a separate named object */
    for (size_t i = 0; i < gState.reportPages.size(); i++) {
        // Create object name with index (Directory_0, Directory_1, etc.)
        TString name = TString::Format("Directory_%zu", i);
        
        // Create a ROOT string object containing the report
        TObjString obj(gState.reportPages[i].c_str());
        
        // Write to file and check for errors
        if (obj.Write(name) == 0) {
            std::cerr << "Warning: Failed to write directory report " << i 
                      << " to ROOT file" << std::endl;
        }
    }

    // ===================================================================
    // STORE GLOBAL SUMMARY
    // ===================================================================
    /* Save the consolidated summary as a special object */
    TObjString summary(gState.globalSummary.c_str());
    if (summary.Write("GlobalSummary") == 0) {
        std::cerr << "Warning: Failed to write global summary to ROOT file" << std::endl;
    }

    // ===================================================================
    // FILE FINALIZATION
    // ===================================================================
    /* Properly close the file and handle any errors */
    file.Close();
    
    // Verify write operation was successful
    if (file.TestBit(TFile::kWriteError)) {
        std::cerr << "Warning: Potential write error during ROOT file creation: " 
                  << filename << std::endl;
    } else {
        std::cout << "ROOT report successfully saved to: " << filename << std::endl;
        
        // Optional: Print file size for verification
        Long64_t size = file.GetSize();
        std::cout << "File size: " << size << " bytes" << std::endl;
    }
}

/*
 * SavePdfReport:
 * Generates a comprehensive PDF report with graphical elements including:
 * - Summary page with pie chart visualization
 * - Detailed directory reports with color-coded status
 * - Professional formatting and visual hierarchy
 *
 * Parameters:
 *   filename - Full path of the output PDF file
 *
 * Operation:
 * 1. Creates a multi-page PDF document using ROOT's TCanvas
 * 2. First page shows global statistics and pie chart
 * 3. Subsequent pages show individual directory reports
 * 4. Uses color coding to highlight status and issues
 * 5. Closes PDF document properly
 */
void SavePdfReport(const TString& filename) {
    // Create a canvas for PDF output (1200x1600 pixels)
    TCanvas canvas("canvas", "Validation Report", 1200, 1600);

    // ===================================================================
    // PDF DOCUMENT INITIALIZATION
    // ===================================================================
    /* Open PDF document - use [ to start multi-page document */
    canvas.Print(filename + "[");
    
    // ===================================================================
    // PAGE 1: GLOBAL SUMMARY
    // ===================================================================
    canvas.Clear();
    canvas.Divide(1, 2); // Split canvas into top and bottom sections
    
    // -------------------------------------------------------------------
    // TOP SECTION: TEXT SUMMARY
    // -------------------------------------------------------------------
    canvas.cd(1); // Activate top section
    
    // Create text box for summary information
    TPaveText summaryBox(0.05, 0.05, 0.95, 0.95);
    summaryBox.AddText("EXORCISM VALIDATION REPORT - GLOBAL SUMMARY");
    summaryBox.AddText("");
    summaryBox.AddText(TString::Format("Ladder: %s", TString(gState.currentLadder).Data()));
    summaryBox.AddText(TString::Format("Report generated: %s %s", __DATE__, __TIME__));
    summaryBox.AddText("");

    // Calculate totals and success rate
    int totalDirs = gState.passedDirs + gState.passedWithIssuesDirs + gState.failedDirs;
    summaryBox.AddText(TString::Format("Total directories: %d", totalDirs));
    summaryBox.AddText(TString::Format("Passed: %d", gState.passedDirs));
    summaryBox.AddText(TString::Format("Passed with issues: %d", gState.passedWithIssuesDirs));
    summaryBox.AddText(TString::Format("Failed: %d", gState.failedDirs));
    summaryBox.AddText(TString::Format("Success rate: %.1f%%", (totalDirs > 0 ? 100.0 * (gState.passedDirs + gState.passedWithIssuesDirs) / totalDirs : 0)));

    // Style the summary box
    summaryBox.SetTextAlign(12);  // Center alignment
    summaryBox.SetTextSize(0.03);
    summaryBox.SetFillColor(0);   // Transparent background
    summaryBox.SetBorderSize(1);
    summaryBox.Draw();
    
    // -------------------------------------------------------------------
    // BOTTOM SECTION: PIE CHART VISUALIZATION
    // -------------------------------------------------------------------
    canvas.cd(2); // Activate bottom section

    if (totalDirs > 0) {
        // Create pie chart with three segments
        TPie* pie = new TPie("pie", "", 3);
        
        // Position and size the pie chart
        pie->SetCircle(0.3, 0.5, 0.2);
        
        // Add data segments with colors
        pie->SetEntryVal(0, gState.passedDirs);
        pie->SetEntryLabel(0, "");
        pie->SetEntryFillColor(0, kGreen);

        pie->SetEntryVal(1, gState.passedWithIssuesDirs);
        pie->SetEntryLabel(1, "");
        pie->SetEntryFillColor(1, kOrange);

        pie->SetEntryVal(2, gState.failedDirs);
        pie->SetEntryLabel(2, "");
        pie->SetEntryFillColor(2, kRed);

        // Draw the pie chart
        pie->Draw("rsc");

        // Create and position the legend
        TLegend* legend = new TLegend(0.6, 0.5, 0.95, 0.85);
        legend->SetHeader("Validation Results", "C");
        legend->SetTextSize(0.03);
        legend->SetBorderSize(1);
        legend->SetFillColor(0);

        // Add legend entries with percentages
        legend->AddEntry("", TString::Format("Passed: %d (%.1f%%)", 
            gState.passedDirs, 100.0*gState.passedDirs/totalDirs), "");
        legend->AddEntry("", TString::Format("Passed with issues: %d (%.1f%%)", 
            gState.passedWithIssuesDirs, 100.0*gState.passedWithIssuesDirs/totalDirs), "");
        legend->AddEntry("", TString::Format("Failed: %d (%.1f%%)", 
            gState.failedDirs, 100.0*gState.failedDirs/totalDirs), "");

        legend->Draw();
    } else {
        // Handle case with no directories
        TPaveText noData(0.1, 0.1, 0.9, 0.9);
        noData.AddText("No validation data available")->SetTextColor(kRed);
        noData.Draw();
    }
    
    // Output the summary page to PDF
    canvas.Print(filename);
    
    // ===================================================================
    // FOLLOWING PAGES: DETAILED DIRECTORY REPORTS
    // ===================================================================
    for (const auto& report : gState.reportPages) {
        canvas.Clear();
        canvas.cd(); // Use full canvas for directory reports
        
        // Create text box for report content      
        TPaveText textBox(0.05, 0.05, 0.95, 0.95);
        textBox.SetTextAlign(12);   // Left alignment
        textBox.SetTextSize(0.025);
        textBox.SetFillColor(0);    // Transparent background
        textBox.SetBorderSize(1);
        
        // Parse the report text line by line
        std::istringstream stream(report);
        std::string line;
        bool isFailedFolder = false;
        
        while (std::getline(stream, line)) {
            // Skip empty lines
            if (line.empty()) continue;
            
            // Handle status line with color coding
            if (line.find("STATUS:") != std::string::npos) {
                if (line.find("FAILED") != std::string::npos) {
                    textBox.AddText(line.c_str())->SetTextColor(kRed);
                    isFailedFolder = true;
                } 
                else if (line.find("PASSED WITH ISSUES") != std::string::npos) {
                    textBox.AddText(line.c_str())->SetTextColor(kOrange);
                    isFailedFolder = true;
                }
                else if (line.find("PASSED") != std::string::npos) {
                    textBox.AddText(line.c_str())->SetTextColor(kGreen+2);
                    isFailedFolder = false;
                }
                continue;
            }
            
            // Handle log file status line
            if (line.find("Log file:") != std::string::npos) {
                size_t colonPos = line.find(":");
                if (colonPos != std::string::npos) {
                    std::string prefix = line.substr(0, colonPos + 1);
                    std::string rest = line.substr(colonPos + 1);
        
                    textBox.AddText(prefix.c_str());
                     if (rest.find("FOUND") != std::string::npos) {
                        textBox.AddText(rest.c_str())->SetTextColor(kGreen+2);
                    } else if (rest.find("MISSING") != std::string::npos) {
                        textBox.AddText(rest.c_str())->SetTextColor(kRed);
                    } else {
                        textBox.AddText(rest.c_str());
                    }
                } else {
                     textBox.AddText(line.c_str());
                }
                continue;
            }

            // Handle module files status line
            if (line.find("Module files:") != std::string::npos) {
                size_t colonPos = line.find(":");
                if (colonPos != std::string::npos) {
                    std::string prefix = line.substr(0, colonPos + 1);
                    std::string rest = line.substr(colonPos + 1);
        
                    textBox.AddText(prefix.c_str());
                    if (rest.find("ERROR") != std::string::npos) {
                        textBox.AddText(rest.c_str())->SetTextColor(kRed);
                    } else if (rest.find("OK") != std::string::npos) {
                        textBox.AddText(rest.c_str())->SetTextColor(kGreen+2);
                    } else {
                        textBox.AddText(rest.c_str());
                    }
                    } else {
                        textBox.AddText(line.c_str());
                    }
                continue;
            }
            
            // Highlight invalid files sections
            if (line.find("Invalid log data files:") != std::string::npos ||
                line.find("Module test file errors:") != std::string::npos) {
                textBox.AddText(line.c_str())->SetTextColor(kRed);
                continue;
            }
            
            // Highlight individual file errors (lines starting with " - ")
            if (line.find(" - ") == 0) {
                textBox.AddText(line.c_str())->SetTextColor(kRed);
                continue;
            }
            
            // Individual file errors (lines starting with " - ")
            if (line.find(" - ") == 0) {
                textBox.AddText(line.c_str())->SetTextColor(kRed);
                continue;
            }
            
            // Color code count lines (X/8) based on correctness
            if (line.find("files:") != std::string::npos || line.find("found:") != std::string::npos) {
                size_t colonPos = line.find(":");
                if (colonPos != std::string::npos) {
                    std::string prefix = line.substr(0, colonPos + 1);
                    std::string rest = line.substr(colonPos + 1);
                    
                    // Check if count is incorrect (looking for patterns like "X/8")
                    bool isCountIncorrect = false;
                    size_t slashPos = rest.find("/");
                    if (slashPos != std::string::npos) {
                        std::string countStr = rest.substr(0, slashPos);
                        std::string expectedStr = rest.substr(slashPos + 1);
    
                        try {
                            int count = std::stoi(countStr);
                            int expected = std::stoi(expectedStr);
                            if (count != expected) {
                                isCountIncorrect = true;
                            }
                        } catch (const std::invalid_argument& e) {
                            isCountIncorrect = true;
                        } catch (const std::out_of_range& e) {
                            isCountIncorrect = true;
                        } 
                    }                   
                    // Add colored text
                    textBox.AddText(prefix.c_str());
                    if (isCountIncorrect) {
                        textBox.AddText(rest.c_str())->SetTextColor(kRed);
                    } else {
                        textBox.AddText(rest.c_str())->SetTextColor(kGreen+2);
                    }
                } else {
                    textBox.AddText(line.c_str());
                }
                continue;
            }

            // Special handling for Pscan section counts
            if (line.find("Electron text:") != std::string::npos || 
                line.find("Hole text:") != std::string::npos ||
                line.find("Electron root:") != std::string::npos ||
                line.find("Hole root:") != std::string::npos) {
                
                size_t colonPos = line.find(":");
                if (colonPos != std::string::npos) {
                    std::string prefix = line.substr(0, colonPos + 1);
                    std::string rest = line.substr(colonPos + 1);
                    
                    // Color code count lines (X/8) based on correctness
                    bool isCountCorrect = false;
                    size_t slashPos = rest.find("/");
                    if (slashPos != std::string::npos) {
                        std::string countStr = rest.substr(0, slashPos);
                        
                        try {
                            int count = std::stoi(countStr);
                            if (count == 8) {
                                isCountCorrect = true;
                            }
                        } catch (...) {
                            // Ignore conversion errors
                        }
                    }
                    
                    // Add colored text
                    textBox.AddText(prefix.c_str());
                    if (isCountCorrect) {
                        textBox.AddText(rest.c_str())->SetTextColor(kGreen+2);
                    } else {
                        textBox.AddText(rest.c_str())->SetTextColor(kRed);
                    }
                } else {
                    textBox.AddText(line.c_str());
                }
                continue;
            }
            
            // Highlight error and warning messages
            if (line.find("Error:") != std::string::npos || 
                line.find("Warning:") != std::string::npos) {
                textBox.AddText(line.c_str())->SetTextColor(kOrange+7);
                continue;
            }
            
            // Default case - normal text
            textBox.AddText(line.c_str());
        }
        
        // Draw the text box and add to PDF
        textBox.Draw();
        canvas.Print(filename);
    }
    
    // ===================================================================
    // FINALIZE PDF DOCUMENT
    // ===================================================================
    /* Close the PDF document properly using ] */
    canvas.Print(filename + "]");
    std::cout << "PDF report saved to: " << filename << std::endl;
}

/*
 * GenerateGlobalSummary:
 * Creates a consolidated summary of all validation results with statistics and metrics.
 * This global summary appears at the end of reports and provides high-level insights.
 *
 * Parameters:
 *   totalDirs - Total number of directories processed (for percentage calculations)
 *
 * Operation:
 * 1. Calculates success rate and other metrics
 * 2. Formats results into a standardized summary block
 * 3. Stores the summary in gState.globalSummary for inclusion in reports
 * 4. Outputs the summary to console for immediate feedback
 *
 * Output Includes:
 * - Total directories processed
 * - Breakdown by status (passed/warning/failed)
 * - Success rate percentage
 * - Visual separators for readability
 */
void GenerateGlobalSummary(int totalDirs) {
    // Create a string stream to build the summary content
    std::stringstream summary;

    // ===================================================================
    // SUMMARY HEADER
    // ===================================================================
    summary << "\n\n====================================================\n";
    summary << "EXORCISM VALIDATION SUMMARY\n";
    summary << "====================================================\n";

    // ===================================================================
    // CORE STATISTICS
    // ===================================================================
    /* Calculate success rate with protection against division by zero */
    float successRate = totalDirs > 0 ? 
        (100.0f * (gState.passedDirs + gState.passedWithIssuesDirs) / totalDirs) : 0.0f;

    // Basic counts
    summary << "Ladder:          " << gState.currentLadder << "\n";
    summary << "Total directories: " << totalDirs << "\n";
    summary << "Passed:          " << gState.passedDirs << "\n";
    summary << "Passed with issues: " << gState.passedWithIssuesDirs << "\n";
    summary << "Failed:          " << gState.failedDirs << "\n";

    // Success rate with fixed decimal precision
    summary << "Success rate:    " << std::fixed << std::setprecision(1) 
            << successRate << "%\n";

    // ===================================================================
    // ADDITIONAL METRICS (when applicable)
    // ===================================================================
    /* Include warning ratios if there are passed-with-issues cases */
    if (gState.passedWithIssuesDirs > 0) {
        float warningRate = 100.0f * gState.passedWithIssuesDirs / 
                          (gState.passedDirs + gState.passedWithIssuesDirs);
        summary << "Warning rate among passed: " << std::setprecision(1) 
                << warningRate << "%\n";
    }

    /* Critical failure analysis */
    if (gState.failedDirs > 0) {
        float failureRate = 100.0f * gState.failedDirs / totalDirs;
        summary << "Critical failure rate: " << std::setprecision(1) 
                << failureRate << "%\n";
    }

    // ===================================================================
    // QUALITATIVE ASSESSMENT
    // ===================================================================
    summary << "\nOverall Status: ";
    if (successRate >= 95.0f) {
        summary << "EXCELLENT (≥95% success)";
    } 
    else if (successRate >= 80.0f) {
        summary << "GOOD (≥80% success)";
    }
    else if (successRate >= 60.0f) {
        summary << "FAIR (≥60% success)";
    }
    else {
        summary << "POOR (<60% success)";
    }

    // ===================================================================
    // FOOTER
    // ===================================================================
    summary << "\n====================================================\n";
    summary << "End of Summary\n";
    summary << "====================================================\n";

    // ===================================================================
    // STORE AND OUTPUT RESULTS
    // ===================================================================
    // Save to global state for inclusion in reports
    gState.globalSummary = summary.str();

    // Also print to console for immediate visibility
    std::cout << gState.globalSummary << std::endl;
}

// ===================================================================
// Directory Processing
// ===================================================================
/*
 * FindValidationDirectories:
 * Scans the current working directory to identify all potential validation directories.
 * 
 * Returns:
 *   std::vector<TString> - List of directory names that should be validated
 *
 * Operation:
 * 1. Gets list of all files/directories in current working directory
 * 2. Filters for valid directory names (excluding system directories)
 * 3. Returns names of candidate directories for validation
 *
 * Directory Selection Criteria:
 * - Must be a directory (not a file)
 * - Must not be "." or ".." (current/parent directory)
 * - Must not be hidden (names starting with '.')
 * - Must not be a known system directory
 */
std::vector<TString> FindValidationDirectories() {
    std::vector<TString> directories;  // Stores found directories
    
    // ===================================================================
    // INITIALIZE DIRECTORY SCANNING
    // ===================================================================
    /* Create directory object for current working directory */
    TSystemDirectory rootDir(".", ".");
    
    /* Get list of all entries in the directory */
    TList* dirContents = rootDir.GetListOfFiles();
    
    // ===================================================================
    // VALIDATION CHECKS
    // ===================================================================
    if (!dirContents) {
        std::cerr << "Error: Could not read directory contents from current path." << std::endl;
        return directories;  // Return empty vector on error
    }

    // ===================================================================
    // PROCESS DIRECTORY ENTRIES
    // ===================================================================
    TIter next(dirContents);  // Create iterator for directory contents
    TSystemFile* file;        // Pointer to current file/directory entry
    
    while ((file = (TSystemFile*)next())) {  // Iterate through all entries
        TString fileName = file->GetName();  // Get entry name
        
        // ---------------------------------------------------------------
        // FILTER OUT NON-DIRECTORIES
        // ---------------------------------------------------------------
        /* Skip if not a directory or special system entries */
        if (!file->IsDirectory() || fileName == "." || fileName == "..") {
            continue;
        }
        
        // ---------------------------------------------------------------
        // FILTER OUT HIDDEN DIRECTORIES
        // ---------------------------------------------------------------
        /* Skip hidden directories (common on Unix systems) */
        if (fileName.BeginsWith(".")) {
            continue;
        }
        
        // ---------------------------------------------------------------
        // FILTER OUT KNOWN SYSTEM DIRECTORIES
        // ---------------------------------------------------------------
        /* Skip common ROOT and system directories */
        if (fileName == "root" || fileName == "sys" || fileName == "etc") {
            continue;
        }
        
        // ---------------------------------------------------------------
        // ADD VALID DIRECTORY TO RESULTS
        // ---------------------------------------------------------------
        /* If all checks passed, add to our list */
        directories.push_back(fileName);
    }

    // ===================================================================
    // CLEANUP AND RETURN RESULTS
    // ===================================================================
    delete dirContents;  // Free the directory listing
    
    // Log findings to console
    std::cout << "Found " << directories.size() 
              << " potential validation directories." << std::endl;
    
    return directories;
}

// ===================================================================
// File Cleanup Function
// ===================================================================
/*
 * Extra_Omnes:
 * Performs interactive cleanup of problematic files identified during validation.
 * Latin for "all others out", this function handles all remaining file issues.
 *
 * Operation:
 * 1. Identifies problematic files from validation results
 * 2. Groups files by error type (invalid, empty, unexpected)
 * 3. Prompts user for confirmation before deletion
 * 4. Tracks successful/failed deletions
 * 5. Generates cleanup report
 *
 * Safety Features:
 * - Interactive confirmation for each deletion group
 * - Preserves original files if deletion fails
 * - Comprehensive logging of all actions
 */
void Extra_Omnes() {
    std::cout << "\n===== FILE CLEANUP PROCEDURE =====" << std::endl;
    std::cout << "This will remove problematic files after confirmation." << std::endl;
    
    // ===================================================================
    // TRACKING VARIABLES
    // ===================================================================
    std::vector<std::string> deletedFiles;      // Successfully deleted files
    std::vector<std::string> failedDeletions;   // Files that couldn't be deleted
    
    // ===================================================================
    // PROCESS ALL DIRECTORIES
    // ===================================================================
    std::vector<TString> directories = FindValidationDirectories();
    for (const auto& dir : directories) {
        // Run validators to get complete error lists
        ValidationResult logResult = CheckLogFiles(dir.Data());
        ValidationResult trimResult = CheckTrimFiles(dir.Data());
        ValidationResult pscanResult = CheckPscanFiles(dir.Data());
        ValidationResult connResult = CheckConnFiles(dir.Data());

        // ===============================================================
        // 1. PROCESS DATA-TESTER PAIRS
        // ===============================================================
        if (logResult.dataFileCount > 0) {
            TSystemDirectory dirObj(dir, dir);
            TList* files = dirObj.GetListOfFiles();
            if (files) {
                std::vector<std::pair<TString, TString>> dataTesterPairs;
                std::map<TString, bool> testerFileMap;

                // Collect all tester files first
                TSystemFile* file;
                TIter next(files);
                while ((file = (TSystemFile*)next())) {
                    TString fileName = file->GetName();
                    if (fileName.BeginsWith("tester_febs_") && fileName.Contains("_arr_")) {
                        testerFileMap[fileName] = false; // Mark as unmatched
                    }
                }

                // Match data files with testers
                TIter next2(files);
                while ((file = (TSystemFile*)next2())) {
                    TString fileName = file->GetName();
                    if (fileName.BeginsWith(dir) && fileName.EndsWith("_data.dat")) {
                        TString dataPattern;
                        bool isSpecialCase = false;

                        // Check for standard format: "folder_YYMMDD_HHMM_data.dat"
                        if (fileName.Length() == (15 + 1 + 6 + 1 + 4 + 9)) {
                            dataPattern = fileName(16, 11); // Extract YYMMDD_HHMM
                        } 
                        // Check for special case: "folder_data.dat"
                        else if (fileName == TString::Format("%s_data.dat", dir.Data())) {
                            isSpecialCase = true;
                        } else {
                            continue; // Skip unexpected formats
                        }

                        // Find matching tester file
                        TString matchedTester;
                        if (isSpecialCase) {
                            // Special case: match with oldest available tester
                            for (auto& tester : testerFileMap) {
                                if (!tester.second) {
                                    matchedTester = tester.first;
                                    tester.second = true;
                                    break;
                                }
                            }
                        } else {
                            // Normal case: match by exact pattern
                            for (auto& tester : testerFileMap) {
                                Ssiz_t arrPos = tester.first.Index("_arr_");
                                if (arrPos != kNPOS && !tester.second) {
                                    TString testerPattern = tester.first(arrPos + 5, 11);
                                    if (dataPattern == testerPattern) {
                                        matchedTester = tester.first;
                                        tester.second = true;
                                        break;
                                    }
                                }
                            }
                        }

                        dataTesterPairs.emplace_back(fileName, matchedTester);
                    }
                }

                // Process invalid data files and their testers
                for (const auto& invalidFile : logResult.invalidFiles) {
                    // Skip log files from deletion
                    if (TString(invalidFile.c_str()).EndsWith(".log")) {
                        continue;
                    }

                    // Find matching pair
                    for (const auto& pair : dataTesterPairs) {
                        if (pair.first == invalidFile) {
                            std::cout << "\n===== INVALID DATA-TESTER PAIR =====" << std::endl;
                            std::cout << "Data file: " << pair.first << std::endl;
                            if (!pair.second.IsNull()) {
                                std::cout << "Matched tester file: " << pair.second << std::endl;
                            } else {
                                std::cout << "No matching tester file found" << std::endl;
                            }

                            // Interactive confirmation
                            std::cout << "Delete this file pair? (y/n): ";
                            std::string response;
                            std::getline(std::cin, response);
                            
                            if (response == "y" || response == "Y") {
                                // Delete data file
                                TString dataPath = dir + "/" + pair.first;
                                if (gSystem->Unlink(dataPath.Data())) {
                                    deletedFiles.push_back(pair.first.Data());
                                    std::cout << "Deleted data file: " << pair.first << std::endl;
                                } else {
                                    failedDeletions.push_back(pair.first.Data());
                                    std::cout << "Failed to delete data file: " << pair.first << std::endl;
                                }

                                // Delete tester file if exists
                                if (!pair.second.IsNull()) {
                                    TString testerPath = dir + "/" + pair.second;
                                    if (gSystem->Unlink(testerPath.Data())) {
                                        deletedFiles.push_back(pair.second.Data());
                                        std::cout << "Deleted tester file: " << pair.second << std::endl;
                                    } else {
                                        failedDeletions.push_back(pair.second.Data());
                                        std::cout << "Failed to delete tester file: " << pair.second << std::endl;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                delete files;
            }
        }

        // ===============================================================
        // 2. PROCESS OTHER PROBLEMATIC FILES
        // ===============================================================
        struct FileCategory {
            std::string name;
            std::vector<std::string> files;
            TString subdir;
            bool checkFormat;
        };

        // Define cleanup categories
        std::vector<FileCategory> categories = {
            {"Empty log data files", logResult.emptyFiles, "", false},
            {"Unexpected files in log directory", logResult.unexpectedFiles, "", false},
            {"Empty pscan files", pscanResult.emptyFiles, "pscan_files", false},
            {"Module test file errors", pscanResult.moduleErrorFiles, "pscan_files", false},
            {"Unexpected files in pscan directory", pscanResult.unexpectedFiles, "pscan_files", false}
        };

        // Filter out log files from all categories
        for (auto& category : categories) {
            category.files.erase(
                std::remove_if(
                    category.files.begin(),
                    category.files.end(),
                    [](const std::string& file) {
                        return TString(file.c_str()).EndsWith(".log");
                    }
                ),
                category.files.end()
            );
        }

        for (auto& category : categories) {
            if (category.files.empty()) continue;
            
            std::cout << "\n===== " << category.name << " =====" << std::endl;
            std::cout << "Found " << category.files.size() << " files:" << std::endl;
            for (const auto& file : category.files) {
                std::cout << " - " << file << std::endl;
            }
            
            // Interactive confirmation
            std::cout << "\nDelete these " << category.files.size() << " files? (y/n): ";
            std::string response;
            std::getline(std::cin, response);
            
            if (response == "y" || response == "Y") {
                for (const auto& file : category.files) {
                    TString fullPath = dir + "/";
                    if (!category.subdir.IsNull()) {
                        fullPath += category.subdir + "/";
                    }
                    fullPath += file.c_str();
                    
                    if (gSystem->Unlink(fullPath.Data())) {
                        deletedFiles.push_back(file);
                    } else {
                        failedDeletions.push_back(file);
                    }
                }
                std::cout << "Deleted " << category.files.size() << " files." << std::endl;
            }
        }

        // ===============================================================
        // 3. SPECIAL HANDLING FOR TRIM AND CONN FILES
        // ===============================================================
        auto processProtectedFiles = [&](const std::vector<std::string>& files, 
                                        const TString& subdir, 
                                        const std::string& categoryName) {
            if (files.empty()) return;

            std::vector<std::string> invalidFormatFiles;
            for (const auto& file : files) {
                TString fileName(file.c_str());
                
                // Skip log files
                if (fileName.EndsWith(".log")) {
                    continue;
                }

                // Check file name format
                bool validFormat = false;
                if (subdir == "trim_files") {
                    validFormat = fileName.EndsWith("_elect.txt") || fileName.EndsWith("_holes.txt");
                } else if (subdir == "conn_check_files") {
                    validFormat = fileName.EndsWith("_elect.txt") || fileName.EndsWith("_holes.txt");
                }

                if (!validFormat) {
                    invalidFormatFiles.push_back(file);
                }
            }

            if (!invalidFormatFiles.empty()) {
                std::cout << "\n===== INVALID FORMAT FILES IN " << subdir << " =====" << std::endl;
                std::cout << "Found " << invalidFormatFiles.size() << " files with wrong format:" << std::endl;
                for (const auto& file : invalidFormatFiles) {
                    std::cout << " - " << file << std::endl;
                }
                
                // Interactive confirmation
                std::cout << "\nDelete these " << invalidFormatFiles.size() 
                          << " invalid format files? (y/n): ";
                std::string response;
                std::getline(std::cin, response);
                
                if (response == "y" || response == "Y") {
                    for (const auto& file : invalidFormatFiles) {
                        TString fullPath = dir + "/" + subdir + "/" + file.c_str();
                        if (gSystem->Unlink(fullPath.Data())) {
                            deletedFiles.push_back(file);
                        } else {
                            failedDeletions.push_back(file);
                        }
                    }
                    std::cout << "Deleted " << invalidFormatFiles.size() << " files." << std::endl;
                }
            }
        };

        // Process trim and conn files with special rules
        processProtectedFiles(trimResult.unexpectedFiles, "trim_files", "Unexpected files in trim directory");
        processProtectedFiles(connResult.unexpectedFiles, "conn_check_files", "Unexpected files in connection directory");
    }
    
    // ===================================================================
    // GENERATE CLEANUP REPORT
    // ===================================================================
    std::stringstream cleanupReport;
    cleanupReport << "\n===== FILE CLEANUP REPORT =====" << std::endl;
    cleanupReport << "Total deleted files: " << deletedFiles.size() << std::endl;
    cleanupReport << "Total failed deletions: " << failedDeletions.size() << std::endl;
    
    if (!deletedFiles.empty()) {
        cleanupReport << "\nSuccessfully deleted files:" << std::endl;
        for (const auto& file : deletedFiles) {
            cleanupReport << " - " << file << std::endl;
        }
    }
    
    if (!failedDeletions.empty()) {
        cleanupReport << "\nFailed to delete:" << std::endl;
        for (const auto& file : failedDeletions) {
            cleanupReport << " - " << file << std::endl;
        }
    }
    
    // Add to global report
    gState.globalSummary += cleanupReport.str();
    
    std::cout << cleanupReport.str() << std::endl;
}
// ===================================================================
// Main Function - Exorcism
// ===================================================================
/*
 * Exorcism:
 * Main driver function that orchestrates the entire validation process.
 * Performs complete validation of ladder test data directories including:
 * - Directory discovery
 * - Two-pass validation (before/after cleanup)
 * - Comprehensive report generation
 * - Interactive cleanup of problematic files
 *
 * Operation Flow:
 * 1. Initializes global state
 * 2. Discovers validation directories
 * 3. First validation pass (pre-cleanup)
 * 4. Generates initial reports
 * 5. Performs interactive cleanup
 * 6. Second validation pass (post-cleanup)
 * 7. Generates final reports
 * 8. Provides completion summary
 */
void Exorcism() {
    // ===================================================================
    // INITIALIZATION
    // ===================================================================
    /* Set current ladder name from working directory */
    gState.currentLadder = gSystem->BaseName(gSystem->WorkingDirectory());
    
    /* Print startup banner */
    std::cout << "Starting EXORCISM validation for ladder: " 
              << gState.currentLadder << std::endl;
    std::cout << "====================================================" << std::endl;

    // ===================================================================
    // DIRECTORY DISCOVERY
    // ===================================================================
    /* Find all candidate directories for validation */
    std::vector<TString> directories = FindValidationDirectories();
    if (directories.empty()) {
        std::cout << "No validation directories found!" << std::endl;
        return;  // Exit if nothing to validate
    }
    
    std::cout << "Found " << directories.size() 
              << " directories to validate" << std::endl;
    std::cout << "====================================================\n" << std::endl;

    // ===================================================================
    // FIRST VALIDATION PASS (BEFORE CLEANUP)
    // ===================================================================
    std::cout << "\n===== FIRST VALIDATION PASS (BEFORE CLEANUP) =====" << std::endl;
    
    /* Process each directory and generate reports */
    for (const auto& dir : directories) {
        GenerateReportPage(dir);
    }
    
    /* Generate initial summary statistics */
    GenerateGlobalSummary(directories.size());
    
    // ===================================================================
    // SAVE PRE-CLEANUP REPORTS
    // ===================================================================
    /* Create timestamp for report filenames */
    TString timestamp = TString::Format("_%s_%s", __DATE__, __TIME__);
    timestamp.ReplaceAll(" ", "_");  // Fix spaces in date
    timestamp.ReplaceAll(":", "-");  // Fix colons in time
    
    /* Create report filenames with ladder name and timestamp */
    TString beforeTxt = TString::Format("ExorcismReport_%s%s_before.txt", 
                                      gState.currentLadder.c_str(), timestamp.Data());
    TString beforeRoot = TString::Format("ExorcismReport_%s%s_before.root", 
                                       gState.currentLadder.c_str(), timestamp.Data());
    TString beforePdf = TString::Format("ExorcismReport_%s%s_before.pdf", 
                                      gState.currentLadder.c_str(), timestamp.Data());
    
    /* Save reports in all formats */
    std::cout << "\nSaving pre-cleanup reports..." << std::endl;
    SaveTxtReport(beforeTxt);
    SaveRootReport(beforeRoot);
    SavePdfReport(beforePdf);

    // ===================================================================
    // INTERACTIVE CLEANUP
    // ===================================================================
    /* Perform guided cleanup of problematic files */
    Extra_Omnes();
    
    // ===================================================================
    // PREPARE FOR SECOND VALIDATION PASS
    // ===================================================================
    /* Reset global state while preserving ladder name */
    std::string ladderName = gState.currentLadder;  // Save
    gState = GlobalState();  // Reset all counters and reports
    gState.currentLadder = ladderName;  // Restore
    
    // ===================================================================
    // SECOND VALIDATION PASS (AFTER CLEANUP)
    // ===================================================================
    std::cout << "\n===== SECOND VALIDATION PASS (AFTER CLEANUP) =====" << std::endl;
    
    /* Re-validate all directories */
    for (const auto& dir : directories) {
        GenerateReportPage(dir);
    }
    
    /* Generate post-cleanup summary */
    GenerateGlobalSummary(directories.size());
    
    // ===================================================================
    // SAVE POST-CLEANUP REPORTS
    // ===================================================================
    /* Create report filenames for post-cleanup */
    TString afterTxt = TString::Format("ExorcismReport_%s%s_after.txt", 
                                     gState.currentLadder.c_str(), timestamp.Data());
    TString afterRoot = TString::Format("ExorcismReport_%s%s_after.root", 
                                      gState.currentLadder.c_str(), timestamp.Data());
    TString afterPdf = TString::Format("ExorcismReport_%s%s_after.pdf", 
                                     gState.currentLadder.c_str(), timestamp.Data());
    
    /* Save final reports */
    std::cout << "\nSaving post-cleanup reports..." << std::endl;
    SaveTxtReport(afterTxt);
    SaveRootReport(afterRoot);
    SavePdfReport(afterPdf);

    // ===================================================================
    // COMPLETION SUMMARY
    // ===================================================================
    std::cout << "\nValidation complete! Two sets of reports generated:" << std::endl;
    std::cout << "Pre-cleanup reports:" << std::endl;
    std::cout << " - Text: " << beforeTxt << std::endl;
    std::cout << " - ROOT: " << beforeRoot << std::endl;
    std::cout << " - PDF:  " << beforePdf << std::endl;
    
    std::cout << "\nPost-cleanup reports:" << std::endl;
    std::cout << " - Text: " << afterTxt << std::endl;
    std::cout << " - ROOT: " << afterRoot << std::endl;
    std::cout << " - PDF:  " << afterPdf << std::endl;
    
}

int main() {
    Exorcism();
    return 0;
}


