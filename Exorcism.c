#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <map>
#include <algorithm>
#include <set>

#include <TSystem.h>
#include <TString.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TLatex.h>
#include <TPie.h>
#include <TLegend.h>
#include <TObjString.h>
#include <TInterpreter.h>
#include <TPaveText.h>

// ===================================================================
// Global Constants and Structures
// ===================================================================
// Evaluation status levels
#define STATUS_PASSED           0
#define STATUS_PASSED_WITH_ISSUES 1
#define STATUS_FAILED           2

// Log files flags
#define FLAG_DIR_MISSING         0x01
#define FLAG_LOG_MISSING         0x02
#define FLAG_DATA_MISSING        0x04
#define FLAG_NO_FEB_FILE         0x08
#define FLAG_FILE_OPEN           0x10
#define FLAG_DATA_EMPTY          0x20
#define FLAG_DATA_INVALID        0x40
#define FLAG_UNEXPECTED_FILES    0x80

// Connection files flags
#define FLAG_CONN_FOLDER_MISSING 0x01
#define FLAG_DIR_ACCESS          0x02
#define FLAG_ELECTRON_COUNT      0x04
#define FLAG_HOLE_COUNT          0x08
#define FLAG_FILE_OPEN_CONN      0x10
#define FLAG_UNEXPECTED_FILES_CONN 0x20

// Trim files flags
#define FLAG_TRIM_FOLDER_MISSING 0x01
#define FLAG_DIR_ACCESS_TRIM     0x02
#define FLAG_ELECTRON_COUNT_TRIM 0x04
#define FLAG_HOLE_COUNT_TRIM     0x08
#define FLAG_FILE_OPEN_TRIM      0x10
#define FLAG_UNEXPECTED_FILES_TRIM 0x20

// Pscan files flags
#define FLAG_PSCAN_FOLDER_MISSING 0x01
#define FLAG_DIR_ACCESS_PSCAN    0x02
#define FLAG_ELECTRON_TXT        0x04
#define FLAG_HOLE_TXT            0x08
#define FLAG_ELECTRON_ROOT       0x10
#define FLAG_HOLE_ROOT           0x20
#define FLAG_FILE_OPEN_PSCAN     0x40
#define FLAG_UNEXPECTED_FILES_PSCAN 0x80
#define FLAG_MODULE_ROOT         0x100
#define FLAG_MODULE_TXT          0x200
#define FLAG_MODULE_PDF          0x400

struct ValidationResult {
    int flags = 0;
    std::vector<std::string> openErrorFiles;
    std::vector<std::string> unexpectedFiles;
    std::vector<std::string> emptyFiles;
    std::vector<std::string> invalidFiles;
    std::vector<std::string> moduleErrorFiles;
    
    // Log files specific
    int dataFileCount = 0;
    int nonEmptyDataCount = 0;
    int validDataCount = 0;
    bool foundFebFile = false;
    bool logExists = false;
    
    // Trim/Conn files specific
    int electronCount = 0;
    int holeCount = 0;
    
    // Pscan files specific
    int electronTxtCount = 0;
    int holeTxtCount = 0;
    int electronRootCount = 0;
    int holeRootCount = 0;
};

struct GlobalState {
    std::vector<std::string> reportPages;
    std::string globalSummary;
    int passedDirs = 0;
    int passedWithIssuesDirs = 0;
    int failedDirs = 0;
    std::string currentLadder;
};

GlobalState gState;

// ===================================================================
// Helper Functions
// ===================================================================
bool DirectoryExists(const TString& path) {
    return !gSystem->AccessPathName(path, kFileExists);
}

TList* GetDirectoryListing(const TString& path) {
    TSystemDirectory dir(path, path);
    return dir.GetListOfFiles();
}

bool CheckFileAccess(const TString& filePath, std::vector<std::string>& errorList) {
    std::ifstream file(filePath.Data());
    if (!file.is_open()) {
        errorList.push_back(gSystem->BaseName(filePath.Data()));
        return false;
    }
    file.close();
    return true;
}

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
ValidationResult CheckLogFiles(const char* targetDir) {
    ValidationResult result;
    TString currentDir = gSystem->pwd();
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of target directory
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory does not exist: " << fullTargetPath << std::endl;
        result.flags |= FLAG_DIR_MISSING;
        return result;
    }

    // Construct expected log file path
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

    // Structure to store file information
    struct FileInfo {
        TString fullPath;
        TString fileName;
        TString timestamp;
        time_t fileTime;
    };

    std::vector<FileInfo> dataFiles;
    std::vector<FileInfo> testerFiles;

    // Helper function to extract timestamp and convert to time_t
    auto extractFileInfo = [](const TString& fileName) -> FileInfo {
    FileInfo info;
    info.fileName = fileName;
    
    // Extract timestamp (format for data files: M4DL0T0001610A2_240823_0940_data.dat)
    // Format for tester files: tester_febs_setup0_arr_240823_0940
    TRegexp timestampPattern("([0-9]{6}_[0-9]{4})");
    Ssiz_t pos = 0;
    Ssiz_t len = 0;
    
    if (timestampPattern.Index(fileName, &pos, len) != -1) {
        info.timestamp = fileName(pos, len);
        
        // Parse timestamp (DDMMYY_HHMM)
        TString dayStr = info.timestamp(0, 2);
        TString monthStr = info.timestamp(2, 2);
        TString yearStr = info.timestamp(4, 2);
        TString hourStr = info.timestamp(7, 2);
        TString minuteStr = info.timestamp(9, 2);
        
        struct tm timeinfo = {0};
        timeinfo.tm_year = 2000 + atoi(yearStr.Data()) - 1900;
        timeinfo.tm_mon = atoi(monthStr.Data()) - 1;
        timeinfo.tm_mday = atoi(dayStr.Data());
        timeinfo.tm_hour = atoi(hourStr.Data());
        timeinfo.tm_min = atoi(minuteStr.Data());
        timeinfo.tm_isdst = -1;
        
        info.fileTime = mktime(&timeinfo);
    }
    
    return info;
};

    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
    
        TString fullFilePath = fullTargetPath + "/" + fileName;
        bool isExpectedFile = false;

        // Skip the expected log file - it's already handled separately
        if (fileName == TString::Format("%s_log.log", targetDir)) {
            continue;
        }

        // Check for data files
        if (fileName.BeginsWith(targetDir) && fileName.EndsWith("_data.dat")) {
            isExpectedFile = true;
            result.dataFileCount++;
            
            FileInfo info = extractFileInfo(fileName);
            info.fullPath = fullFilePath;
            dataFiles.push_back(info);
            
            std::ifstream f_data(fullFilePath.Data(), std::ios::binary | std::ios::ate);
            if (!f_data.is_open()) {
                std::cerr << "Error: Cannot open data file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN;
            } else {
                std::streampos size = f_data.tellg();
                f_data.close();
                
                if (size > 0) {
                    result.nonEmptyDataCount++;
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
        // Check for FEB files
        else if (fileName.BeginsWith("tester_febs_")) {
            isExpectedFile = true;
            
            FileInfo info = extractFileInfo(fileName);
            info.fullPath = fullFilePath;
            testerFiles.push_back(info);
        }

        if (!isExpectedFile) {
            std::cerr << "Warning: Unexpected file found: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES;
        }
    }

    delete files;

    // Match data files with tester files
    std::map<TString, bool> matchedTesters; // tracks which tester files have been matched
    for (auto& tester : testerFiles) {
        matchedTesters[tester.fullPath] = false;
    }

    // First pass: try to find exact timestamp matches
    for (auto& dataFile : dataFiles) {
        bool foundMatch = false;
        for (auto& testerFile : testerFiles) {
            if (dataFile.timestamp == testerFile.timestamp && !matchedTesters[testerFile.fullPath]) {
                matchedTesters[testerFile.fullPath] = true;
                foundMatch = true;
                std::cerr << "Info: Data file " << dataFile.fileName 
                          << " matched with tester file " << testerFile.fileName << std::endl;
                break;
            }
        }
        
        if (!foundMatch) {
            std::cerr << "Warning: No exact timestamp match found for data file: " 
                      << dataFile.fileName << std::endl;
        }
    }

    // Second pass: for unmatched data files, find the best matching tester file
    for (auto& dataFile : dataFiles) {
        bool hasExactMatch = false;
        for (auto& testerFile : testerFiles) {
            if (dataFile.timestamp == testerFile.timestamp) {
                hasExactMatch = true;
                break;
            }
        }
        
        if (!hasExactMatch && dataFile.fileTime != 0) {
            // Find the closest unmatched tester file by time difference
            TString bestTester;
            time_t smallestDiff = std::numeric_limits<time_t>::max();
            
            for (auto& testerFile : testerFiles) {
                if (!matchedTesters[testerFile.fullPath] && testerFile.fileTime != 0) {
                    time_t diff = abs(dataFile.fileTime - testerFile.fileTime);
                    if (diff < smallestDiff) {
                        smallestDiff = diff;
                        bestTester = testerFile.fullPath;
                    }
                }
            }
            
            if (!bestTester.IsNull()) {
                matchedTesters[bestTester] = true;
                std::cerr << "Info: Data file " << dataFile.fileName
                          << " matched with closest tester file (time difference: " 
                          << smallestDiff << " seconds)" << std::endl;
            } else {
                std::cerr << "Error: No available tester file found for data file: "
                          << dataFile.fileName << std::endl;
                result.flags |= FLAG_NO_FEB_FILE;
            }
        }
    }

    // Check if all data files have corresponding tester files
    if (dataFiles.size() > 0 && testerFiles.size() == 0) {
        std::cerr << "Error: No FEB files found in directory" << std::endl;
        result.flags |= FLAG_NO_FEB_FILE;
    }

    // Generate detailed report (same as before)
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

    TList* files = GetDirectoryListing(trimDirPath);
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << trimDirPath << std::endl;
        result.flags |= FLAG_DIR_ACCESS_TRIM;
        return result;
    }

    // Track found indices for electron and hole files
    std::set<int> foundElectronIndices;
    std::set<int> foundHoleIndices;

    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        TString fullFilePath = trimDirPath + "/" + fileName;

        if (fileName.EndsWith("_elect.txt")) {
            // Extract HW index from filename
            Ssiz_t hwPos = fileName.Index("_HW_");
            Ssiz_t setPos = fileName.Index("_SET_", hwPos+4);
            
            if (hwPos == -1 || setPos == -1 || setPos <= hwPos+4) {
                std::cerr << "Error: Invalid electron file name format: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
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
            if (hwIndex < 0 || hwIndex > 7) {
                std::cerr << "Error: HW index out of range (0-7) in electron file: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            if (foundElectronIndices.find(hwIndex) != foundElectronIndices.end()) {
                std::cerr << "Error: Duplicate HW index " << hwIndex << " in electron files" << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            foundElectronIndices.insert(hwIndex);
            result.electronCount++;
            
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_TRIM;
            } else {
                // Check if file is empty
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        } else if (fileName.EndsWith("_holes.txt")) {
            // Extract HW index from filename
            Ssiz_t hwPos = fileName.Index("_HW_");
            Ssiz_t setPos = fileName.Index("_SET_", hwPos+4);
            
            if (hwPos == -1 || setPos == -1 || setPos <= hwPos+4) {
                std::cerr << "Error: Invalid hole file name format: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
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
            if (hwIndex < 0 || hwIndex > 7) {
                std::cerr << "Error: HW index out of range (0-7) in hole file: " << fileName << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            if (foundHoleIndices.find(hwIndex) != foundHoleIndices.end()) {
                std::cerr << "Error: Duplicate HW index " << hwIndex << " in hole files" << std::endl;
                result.invalidFiles.push_back(fileName.Data());
                result.flags |= FLAG_DATA_INVALID;
                continue;
            }
            
            foundHoleIndices.insert(hwIndex);
            result.holeCount++;
            
            std::ifstream f_test(fullFilePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_TRIM;
            } else {
                // Check if file is empty
                f_test.seekg(0, std::ios::end);
                if (f_test.tellg() == 0) {
                    result.emptyFiles.push_back(fileName.Data());
                }
                f_test.close();
            }
        } else {
            std::cerr << "Warning: Unexpected file in trim_files: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES_TRIM;
        }
    }

    delete files;

    // Set flags based on counts and indices
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

ValidationResult CheckPscanFiles(const char* targetDir) {
    ValidationResult result;
    TString currentDir = gSystem->pwd();
    TString pscanDirPath = TString::Format("%s/%s/pscan_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of 'pscan_files' directory
    if (gSystem->AccessPathName(pscanDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'pscan_files' does not exist!" << std::endl;
        std::cerr << "Expected path: " << pscanDirPath << std::endl;
        result.flags |= FLAG_PSCAN_FOLDER_MISSING;
        return result;
    }

    // Check module_test files
    TString moduleRoot = TString::Format("%s/module_test_%s.root", pscanDirPath.Data(), targetDir);
    TString moduleTxt = TString::Format("%s/module_test_%s.txt", pscanDirPath.Data(), targetDir);
    TString modulePdf = TString::Format("%s/module_test_%s.pdf", pscanDirPath.Data(), targetDir);

    // Check module ROOT file
    if (gSystem->AccessPathName(moduleRoot, kFileExists)) {
        std::cerr << "Error: Module test root file does not exist: " << moduleRoot << std::endl;
        result.moduleErrorFiles.push_back(gSystem->BaseName(moduleRoot.Data()));
        result.flags |= FLAG_MODULE_ROOT;
    } else {
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

    TList* files = GetDirectoryListing(pscanDirPath);
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << pscanDirPath << std::endl;
        result.flags |= FLAG_DIR_ACCESS_PSCAN;
        return result;
    }

    // List of acceptable auxiliary files
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

        // Process electron text files
        if (fileName.EndsWith("_elect.txt")) {
            result.electronTxtCount++;
            isExpected = true;
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
        // Process hole text files
        else if (fileName.EndsWith("_holes.txt")) {
            result.holeTxtCount++;
            isExpected = true;
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
        // Process electron ROOT files
        else if (fileName.EndsWith("_elect.root")) {
            result.electronRootCount++;
            isExpected = true;
            TFile* rootFile = TFile::Open(fullFilePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open electron root file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_PSCAN;
            }
            if (rootFile) rootFile->Close();
        }
        // Process hole ROOT files
        else if (fileName.EndsWith("_holes.root")) {
            result.holeRootCount++;
            isExpected = true;
            TFile* rootFile = TFile::Open(fullFilePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open hole root file: " << fullFilePath << std::endl;
                result.openErrorFiles.push_back(fileName.Data());
                result.flags |= FLAG_FILE_OPEN_PSCAN;
            }
            if (rootFile) rootFile->Close();
        }
        else {
            // Check if it's a module file or acceptable aux file
            TString modulePrefix = TString::Format("module_test_%s", targetDir);
            if (fileName.BeginsWith(modulePrefix) && 
                (fileName.EndsWith(".root") || fileName.EndsWith(".txt") || fileName.EndsWith(".pdf"))) {
                isExpected = true;
            } else {
                for (const auto& auxFile : acceptableAuxFiles) {
                    if (fileName == auxFile) {
                        isExpected = true;
                        break;
                    }
                }
            }
        }

        if (!isExpected) {
            std::cerr << "Warning: Unexpected file in pscan_files: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES_PSCAN;
        }
    }

    delete files;

    // Set flags based on counts
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

ValidationResult CheckConnFiles(const char* targetDir) {
    ValidationResult result;
    TString currentDir = gSystem->pwd();
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
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

    TList* files = GetDirectoryListing(connDirPath);
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << connDirPath << std::endl;
        result.flags |= FLAG_DIR_ACCESS;
        return result;
    }

    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        TString fullFilePath = connDirPath + "/" + fileName;

        if (fileName.EndsWith("_elect.txt")) {
            result.electronCount++;
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
        } else if (fileName.EndsWith("_holes.txt")) {
            result.holeCount++;
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
        } else {
            std::cerr << "Warning: Unexpected file in conn_check_files: " << fullFilePath << std::endl;
            result.unexpectedFiles.push_back(fileName.Data());
            result.flags |= FLAG_UNEXPECTED_FILES_CONN;
        }
    }

    delete files;

    // Set flags based on counts
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
void GenerateReportPage(const TString& dirName) {
    std::stringstream report;
    report << "====================================================" << std::endl;
    report << "VALIDATION REPORT FOR: " << dirName << std::endl;
    report << "====================================================" << std::endl;
    
    // Run all validations
    ValidationResult logResult = CheckLogFiles(dirName.Data());
    ValidationResult trimResult = CheckTrimFiles(dirName.Data());
    ValidationResult pscanResult = CheckPscanFiles(dirName.Data());
    ValidationResult connResult = CheckConnFiles(dirName.Data());
    
    // Determine overall status
    int dirStatus = STATUS_PASSED;
    std::string statusStr = "PASSED";
    
    // Check for serious failures first
    if (logResult.flags & (FLAG_DIR_MISSING | FLAG_LOG_MISSING | FLAG_DATA_MISSING | FLAG_NO_FEB_FILE | FLAG_FILE_OPEN | FLAG_DATA_INVALID) ||
        trimResult.flags & (FLAG_TRIM_FOLDER_MISSING | FLAG_DIR_ACCESS_TRIM | FLAG_FILE_OPEN_TRIM | FLAG_ELECTRON_COUNT_TRIM | FLAG_HOLE_COUNT_TRIM) ||
        pscanResult.flags & (FLAG_PSCAN_FOLDER_MISSING | FLAG_DIR_ACCESS_PSCAN | FLAG_FILE_OPEN_PSCAN | FLAG_ELECTRON_TXT | FLAG_HOLE_TXT | FLAG_ELECTRON_ROOT | FLAG_HOLE_ROOT | FLAG_MODULE_ROOT | FLAG_MODULE_TXT | FLAG_MODULE_PDF) ||
        connResult.flags & (FLAG_CONN_FOLDER_MISSING | FLAG_DIR_ACCESS | FLAG_FILE_OPEN_CONN | FLAG_ELECTRON_COUNT | FLAG_HOLE_COUNT)) {
        dirStatus = STATUS_FAILED;
        statusStr = "FAILED";
    }
    // Check for minor issues (only empty files or unexpected files)
    else if (logResult.flags & (FLAG_DATA_EMPTY | FLAG_UNEXPECTED_FILES) ||
             pscanResult.flags & (FLAG_UNEXPECTED_FILES_PSCAN)) {
        dirStatus = STATUS_PASSED_WITH_ISSUES;
        statusStr = "PASSED WITH ISSUES";
    }
    
    // Update counters based on status
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
    
    report << "STATUS: " << statusStr << std::endl;
    
    // Add detailed results
    report << "\n[LOG FILES]" << std::endl;
    report << "Data files: " << logResult.dataFileCount << " found | " 
           << (logResult.flags & FLAG_DATA_MISSING ? "NONE" : 
              (logResult.flags & FLAG_DATA_EMPTY) ? "SOME EMPTY" :
              (logResult.flags & FLAG_DATA_INVALID) ? "SOME INVALID" : "VALID") << std::endl;
    report << "Non-empty files: " << logResult.nonEmptyDataCount << "/" << logResult.dataFileCount << std::endl;
    report << "Valid files: " << logResult.validDataCount << "/" << logResult.dataFileCount << std::endl;
    report << "Tester FEB files: " << (logResult.foundFebFile ? "FOUND" : "NONE") << std::endl;
    report << "Log file: " << (logResult.logExists ? "FOUND" : "MISSING") << std::endl;
    
    if (!logResult.emptyFiles.empty()) {
        report << "\nEmpty log data files:" << std::endl;
        for (const auto& file : logResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    if (!logResult.invalidFiles.empty()) {
        report << "\nInvalid log data files:" << std::endl;
        for (const auto& file : logResult.invalidFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    report << "\n[TRIM FILES]" << std::endl;
    report << "Electron files: " << trimResult.electronCount << "/8" << std::endl;
    report << "Hole files: " << trimResult.holeCount << "/8" << std::endl;
    
    if (!trimResult.emptyFiles.empty()) {
        report << "\nEmpty trim files:" << std::endl;
        for (const auto& file : trimResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    report << "\n[PSCAN FILES]" << std::endl;
    report << "Electron text: " << pscanResult.electronTxtCount << "/8" << std::endl;
    report << "Hole text: " << pscanResult.holeTxtCount << "/8" << std::endl;
    report << "Electron root: " << pscanResult.electronRootCount << "/8" << std::endl;
    report << "Hole root: " << pscanResult.holeRootCount << "/8" << std::endl;
    report << "Module files: " << (pscanResult.flags & (FLAG_MODULE_ROOT|FLAG_MODULE_TXT|FLAG_MODULE_PDF) ? "ERROR" : "OK") << std::endl;
    
    if (!pscanResult.emptyFiles.empty()) {
        report << "\nEmpty pscan files:" << std::endl;
        for (const auto& file : pscanResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    if (!pscanResult.moduleErrorFiles.empty()) {
        report << "\nModule test file errors:" << std::endl;
        for (const auto& file : pscanResult.moduleErrorFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    report << "\n[CONNECTION FILES]" << std::endl;
    report << "Electron files: " << connResult.electronCount << "/8" << std::endl;
    report << "Hole files: " << connResult.holeCount << "/8" << std::endl;
    
    if (!connResult.emptyFiles.empty()) {
        report << "\nEmpty connection files:" << std::endl;
        for (const auto& file : connResult.emptyFiles) {
            report << " - " << file << std::endl;
        }
    }
    
    // Add error details if any
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
    
    // Add unexpected files if any
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
    
    gState.reportPages.push_back(report.str());
}

void SaveTxtReport(const TString& filename) {
    std::ofstream out(filename.Data());
    
    out << "EXORCISM VALIDATION REPORT\n";
    out << "Ladder: " << gState.currentLadder << "\n";
    out << "Generated: " << __DATE__ << " " << __TIME__ << "\n";
    out << "====================================================\n\n";
    
    for (const auto& page : gState.reportPages) {
        out << page << "\n\n";
    }
    
    out << gState.globalSummary << std::endl;
    out.close();
    std::cout << "Text report saved to: " << filename << std::endl;
}

void SaveRootReport(const TString& filename) {
    TFile file(filename, "RECREATE");
    
    for (size_t i = 0; i < gState.reportPages.size(); i++) {
        TString name = TString::Format("Directory_%zu", i);
        TObjString obj(gState.reportPages[i].c_str());
        obj.Write(name);
    }
    
    TObjString summary(gState.globalSummary.c_str());
    summary.Write("GlobalSummary");
    file.Close();
    std::cout << "ROOT report saved to: " << filename << std::endl;
}

void SavePdfReport(const TString& filename) {
    TCanvas canvas("canvas", "Validation Report", 1200, 1600);
    canvas.Print(filename + "[");
    
    // First page - Global Summary
    canvas.Clear();
    canvas.Divide(1, 2);
    
    // Top part for text summary
    canvas.cd(1);
    TPaveText summaryBox(0.05, 0.05, 0.95, 0.95);
    summaryBox.AddText("EXORCISM VALIDATION REPORT - GLOBAL SUMMARY");
    summaryBox.AddText("");
    summaryBox.AddText(TString::Format("Ladder: %s", TString(gState.currentLadder).Data()));
    summaryBox.AddText(TString::Format("Report generated: %s %s", __DATE__, __TIME__));
    summaryBox.AddText("");
    summaryBox.AddText(TString::Format("Total directories: %d", gState.passedDirs + gState.passedWithIssuesDirs + gState.failedDirs));
    summaryBox.AddText(TString::Format("Passed: %d", gState.passedDirs));
    summaryBox.AddText(TString::Format("Passed with issues: %d", gState.passedWithIssuesDirs));
    summaryBox.AddText(TString::Format("Failed: %d", gState.failedDirs));
    summaryBox.AddText(TString::Format("Success rate: %.1f%%", 
        (gState.passedDirs + gState.passedWithIssuesDirs + gState.failedDirs > 0 ? 
         100.0 * (gState.passedDirs + gState.passedWithIssuesDirs) / (gState.passedDirs + gState.passedWithIssuesDirs + gState.failedDirs) : 0)));
    summaryBox.Draw();
    
    // Bottom part for pie chart
    canvas.cd(2);
    if (gState.passedDirs > 0 || gState.passedWithIssuesDirs > 0 || gState.failedDirs > 0) {
        double total = gState.passedDirs + gState.passedWithIssuesDirs + gState.failedDirs;
        
        TPie* pie = new TPie("pie", "", 3);
        pie->SetCircle(0.3, 0.5, 0.2);
        pie->SetEntryVal(0, gState.passedDirs);
        pie->SetEntryLabel(0, "");
        pie->SetEntryFillColor(0, kGreen);

        pie->SetEntryVal(1, gState.passedWithIssuesDirs);
        pie->SetEntryLabel(1, "");
        pie->SetEntryFillColor(1, kOrange);

        pie->SetEntryVal(2, gState.failedDirs);
        pie->SetEntryLabel(2, "");
        pie->SetEntryFillColor(2, kRed);

        pie->Draw("rsc");

        TLegend* legend = new TLegend(0.6, 0.5, 0.95, 0.85);
        legend->SetHeader("Validation Results", "C");
        legend->SetTextSize(0.03);
        legend->SetBorderSize(1);
        legend->SetFillColor(0);

        legend->AddEntry("", TString::Format("Passed: %d (%.1f%%)", 
                     gState.passedDirs, 100.0*gState.passedDirs/total), "");
        legend->AddEntry("", TString::Format("Passed with issues: %d (%.1f%%)", 
                     gState.passedWithIssuesDirs, 100.0*gState.passedWithIssuesDirs/total), "");
        legend->AddEntry("", TString::Format("Failed: %d (%.1f%%)", 
                     gState.failedDirs, 100.0*gState.failedDirs/total), "");

        legend->Draw();
    }
    
    canvas.Print(filename);
    
    // Directory reports
    for (const auto& report : gState.reportPages) {
        canvas.Clear();
        canvas.cd();
        
        TPaveText textBox(0.05, 0.05, 0.95, 0.95);
        textBox.SetTextAlign(12);
        textBox.SetTextSize(0.025);
        textBox.SetFillColor(0);
        textBox.SetBorderSize(1);
        
        std::istringstream stream(report);
        std::string line;
        bool isFailedFolder = false;
        
        while (std::getline(stream, line)) {
            // Status line
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
            
            // Log file status - modified to match "Tester FEB files" format
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

            // Module files status - modified to match "Tester FEB files" format
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
            
            // Invalid files section
            if (line.find("Invalid log data files:") != std::string::npos) {
                textBox.AddText(line.c_str())->SetTextColor(kRed);
                continue;
            }
            
            // Module test errors section
            if (line.find("Module test file errors:") != std::string::npos) {
                textBox.AddText(line.c_str())->SetTextColor(kRed);
                continue;
            }
            
            // Individual file errors (lines starting with " - ")
            if (line.find(" - ") == 0) {
                textBox.AddText(line.c_str())->SetTextColor(kRed);
                continue;
            }
            
            // Count lines - color numbers if incorrect
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
                    
                    // Check if count is incorrect (8 expected)
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
            
            // Error messages
            if (line.find("Error:") != std::string::npos || line.find("Warning:") != std::string::npos) {
                textBox.AddText(line.c_str())->SetTextColor(kOrange+7);
                continue;
            }
            
            // Default case
            textBox.AddText(line.c_str());
        }
        
        textBox.Draw();
        canvas.Print(filename);
    }
    
    canvas.Print(filename + "]");
    std::cout << "PDF report saved to: " << filename << std::endl;
}

void GenerateGlobalSummary(int totalDirs) {
    std::stringstream summary;
    summary << "\n\n====================================================" << std::endl;
    summary << "EXORCISM VALIDATION SUMMARY" << std::endl;
    summary << "====================================================" << std::endl;
    summary << "Ladder:          " << gState.currentLadder << std::endl;
    summary << "Total directories: " << totalDirs << std::endl;
    summary << "Passed:          " << gState.passedDirs << std::endl;
    summary << "Passed with issues: " << gState.passedWithIssuesDirs << std::endl;
    summary << "Failed:          " << gState.failedDirs << std::endl;
    summary << "Success rate:    " << std::fixed << std::setprecision(1) 
            << (totalDirs > 0 ? (100.0 * (gState.passedDirs + gState.passedWithIssuesDirs) / totalDirs) : 0) 
            << "%" << std::endl;
    summary << "====================================================" << std::endl;
    
    gState.globalSummary = summary.str();
    std::cout << gState.globalSummary << std::endl;
}

// ===================================================================
// Directory Processing
// ===================================================================
std::vector<TString> FindValidationDirectories() {
    std::vector<TString> directories;
    TSystemDirectory rootDir(".", ".");
    TList* dirContents = rootDir.GetListOfFiles();
    
    if (dirContents) {
        TIter next(dirContents);
        TSystemFile* file;
        while ((file = (TSystemFile*)next())) {
            TString fileName = file->GetName();
            if (file->IsDirectory() && fileName != "." && fileName != ".." && !fileName.BeginsWith(".")) {
                directories.push_back(fileName);
            }
        }
        delete dirContents;
    }
    
    return directories;
}

// ===================================================================
// File Cleanup Function
// ===================================================================
void Extra_Omnes() {
    std::cout << "\n===== FILE CLEANUP PROCEDURE =====" << std::endl;
    std::cout << "This will remove problematic files after confirmation." << std::endl;
    
    std::vector<std::string> deletedFiles;
    std::vector<std::string> failedDeletions;
    
    // Process all directories
    std::vector<TString> directories = FindValidationDirectories();
    for (const auto& dir : directories) {
        // Run all validations to get complete file lists
        ValidationResult logResult = CheckLogFiles(dir.Data());
        ValidationResult trimResult = CheckTrimFiles(dir.Data());
        ValidationResult pscanResult = CheckPscanFiles(dir.Data());
        ValidationResult connResult = CheckConnFiles(dir.Data());

        // ===== 1. FIRST PROCESS DATA-TESTER PAIRS =====
        if (logResult.dataFileCount > 0) {
            TSystemDirectory dirObj(dir, dir);
            TList* files = dirObj.GetListOfFiles();
            if (files) {
                std::vector<std::pair<TString, TString>> dataTesterPairs;
                std::map<TString, bool> testerFileMap;

                // Collect all tester files (excluding logs)
                TSystemFile* file;
                TIter next(files);
                while ((file = (TSystemFile*)next())) {
                    TString fileName = file->GetName();
                    if (fileName.BeginsWith("tester_febs_") && !fileName.EndsWith(".log")) {
                        testerFileMap[fileName] = false;
                    }
                }

                // Match data files with testers (excluding logs)
                TIter next2(files);
                while ((file = (TSystemFile*)next2())) {
                    TString fileName = file->GetName();
                    if (fileName.BeginsWith(dir) && fileName.EndsWith("_data.dat") && !fileName.EndsWith(".log")) {
                        // Extract timestamp and match with tester
                        Ssiz_t lastUnderscore = fileName.Last('_');
                        if (lastUnderscore != kNPOS) {
                            TString baseName = fileName(0, lastUnderscore);
                            Ssiz_t timestampPos = baseName.Last('_');
                            if (timestampPos != kNPOS) {
                                TString timestamp = baseName(timestampPos+1, baseName.Length()-timestampPos-1);
                                TString matchedTester;
                                for (auto& tester : testerFileMap) {
                                    if (tester.first.Contains(timestamp) && !tester.second) {
                                        matchedTester = tester.first;
                                        tester.second = true;
                                        break;
                                    }
                                }
                                dataTesterPairs.emplace_back(fileName, matchedTester);
                            }
                        }
                    }
                }

                // Process invalid data files and their testers
                for (const auto& invalidFile : logResult.invalidFiles) {
                    // Skip log files
                    if (TString(invalidFile.c_str()).EndsWith(".log")) {
                        continue;
                    }

                    for (const auto& pair : dataTesterPairs) {
                        if (pair.first == invalidFile) {
                            std::cout << "\n===== INVALID DATA-TESTER PAIR =====" << std::endl;
                            std::cout << "Data file: " << pair.first << std::endl;
                            if (!pair.second.IsNull()) {
                                std::cout << "Matched tester file: " << pair.second << std::endl;
                            } else {
                                std::cout << "No matching tester file found" << std::endl;
                            }

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

        // ===== 2. PROCESS OTHER PROBLEMATIC FILES =====
        struct FileCategory {
            std::string name;
            std::vector<std::string> files;
            TString subdir;
            bool checkFormat;
        };

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

        // ===== 3. SPECIAL HANDLING FOR TRIM AND CONN FILES =====
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

                // Check if file name matches expected format
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
    
    // Generate cleanup report
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
void Exorcism() {
    // Initialize global state
    gState.currentLadder = gSystem->BaseName(gSystem->WorkingDirectory());
    
    std::cout << "Starting EXORCISM validation for ladder: " << gState.currentLadder << std::endl;
    std::cout << "====================================================" << std::endl;
    
    // Find directories to validate
    std::vector<TString> directories = FindValidationDirectories();
    if (directories.empty()) {
        std::cout << "No validation directories found!" << std::endl;
        return;
    }
    
    std::cout << "Found " << directories.size() << " directories to validate" << std::endl;
    std::cout << "====================================================\n" << std::endl;
    
    // First validation pass (before cleanup)
    std::cout << "\n===== FIRST VALIDATION PASS (BEFORE CLEANUP) =====" << std::endl;
    for (const auto& dir : directories) {
        GenerateReportPage(dir);
    }
    GenerateGlobalSummary(directories.size());
    
    // Save pre-cleanup reports with "_before" suffix
    TString timestamp = TString::Format("_%s_%s", __DATE__, __TIME__);
    timestamp.ReplaceAll(" ", "_");
    timestamp.ReplaceAll(":", "-");
    
    // Convert std::string to TString if needed
    TString ladderName(gState.currentLadder.c_str());
    
    // Create filenames using TString::Format
    TString beforeTxt = TString::Format("ExorcismReport_%s%s_before.txt", 
                                      ladderName.Data(), timestamp.Data());
    TString beforeRoot = TString::Format("ExorcismReport_%s%s_before.root", 
                                       ladderName.Data(), timestamp.Data());
    TString beforePdf = TString::Format("ExorcismReport_%s%s_before.pdf", 
                                      ladderName.Data(), timestamp.Data());
    
    std::cout << "\nSaving pre-cleanup reports..." << std::endl;
    SaveTxtReport(beforeTxt);
    SaveRootReport(beforeRoot);
    SavePdfReport(beforePdf);
    
    // Perform cleanup
    Extra_Omnes();
    
    // Clear the global state for second validation pass
    gState.reportPages.clear();
    gState.passedDirs = 0;
    gState.passedWithIssuesDirs = 0;
    gState.failedDirs = 0;
    gState.globalSummary.clear();
    
    // Second validation pass (after cleanup)
    std::cout << "\n===== SECOND VALIDATION PASS (AFTER CLEANUP) =====" << std::endl;
    for (const auto& dir : directories) {
        GenerateReportPage(dir);
    }
    GenerateGlobalSummary(directories.size());
    
    // Save post-cleanup reports with "_after" suffix
    TString afterTxt = TString::Format("ExorcismReport_%s%s_after.txt", 
                                     ladderName.Data(), timestamp.Data());
    TString afterRoot = TString::Format("ExorcismReport_%s%s_after.root", 
                                      ladderName.Data(), timestamp.Data());
    TString afterPdf = TString::Format("ExorcismReport_%s%s_after.pdf", 
                                     ladderName.Data(), timestamp.Data());
    
    std::cout << "\nSaving post-cleanup reports..." << std::endl;
    SaveTxtReport(afterTxt);
    SaveRootReport(afterRoot);
    SavePdfReport(afterPdf);
    
    std::cout << "\nValidation complete! Two sets of reports generated for ladder: "
              << gState.currentLadder << std::endl;
    std::cout << "Pre-cleanup reports: " << beforeTxt << ", " << beforeRoot << ", " << beforePdf << std::endl;
    std::cout << "Post-cleanup reports: " << afterTxt << ", " << afterRoot << ", " << afterPdf << std::endl;
}

int main() {
    Exorcism();
    return 0;
}