#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "TSystem.h"
#include "TSystemDirectory.h"
#include "TFile.h"

// Define flag constants using bitmask
#define FLAG_PSCAN_FOLDER_MISSING  0x01   // pscan_files directory missing
#define FLAG_DIR_ACCESS            0x02   // Error accessing directory
#define FLAG_ELECTRON_TXT          0x04   // Incorrect electron txt file count
#define FLAG_HOLE_TXT              0x08   // Incorrect hole txt file count
#define FLAG_ELECTRON_ROOT         0x10   // Incorrect electron root file count
#define FLAG_HOLE_ROOT             0x20   // Incorrect hole root file count
#define FLAG_FILE_OPEN             0x40   // File opening error
#define FLAG_MODULE_ROOT           0x80   // Module test root file error
#define FLAG_MODULE_TXT            0x100  // Module test txt file error
#define FLAG_MODULE_PDF            0x200  // Module test pdf file error
#define FLAG_UNEXPECTED_FILES      0x400  // Unexpected files in directory

// Structure to hold detailed results
struct CheckPscanFilesResult {
    int flags = 0;                      // Bitmask of flags
    
    // File counts
    int electronTxtCount = 0;
    int holeTxtCount = 0;
    int electronRootCount = 0;
    int holeRootCount = 0;
    
    // Problematic files
    std::vector<std::string> openErrorFiles;      // Files that failed to open
    std::vector<std::string> unexpectedFiles;     // Unexpected files in directory
    std::vector<std::string> moduleErrorFiles;    // Module test files with errors
};

CheckPscanFilesResult CheckPscanFiles(const char* targetDir) {
    CheckPscanFilesResult result;  // Initialize result structure

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Construct full path to pscan_files directory
    TString pscanDirPath = TString::Format("%s/%s/pscan_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of 'pscan_files' directory
    if (gSystem->AccessPathName(pscanDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'pscan_files' does not exist!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << pscanDirPath << std::endl;
        
        result.flags |= FLAG_PSCAN_FOLDER_MISSING;
        return result;
    }

    // ===== Check module_test files =====
    TString moduleRoot = TString::Format("%s/module_test_%s.root", pscanDirPath.Data(), targetDir);
    TString moduleTxt = TString::Format("%s/module_test_%s.txt", pscanDirPath.Data(), targetDir);
    TString modulePdf = TString::Format("%s/module_test_%s.pdf", pscanDirPath.Data(), targetDir);

    // Check module_test ROOT
    if (gSystem->AccessPathName(moduleRoot, kFileExists)) {
        result.moduleErrorFiles.push_back(moduleRoot.Data());
        std::cerr << "Error: Module test root file does not exist: " << moduleRoot << std::endl;
    } else {
        TFile* f_root = TFile::Open(moduleRoot, "READ");
        if (!f_root || f_root->IsZombie()) {
            result.moduleErrorFiles.push_back(moduleRoot.Data());
            std::cerr << "Error: Cannot open module test root file: " << moduleRoot << std::endl;
        }
        if (f_root) f_root->Close();
    }

    // Check module_test TXT
    if (gSystem->AccessPathName(moduleTxt, kFileExists)) {
        result.moduleErrorFiles.push_back(moduleTxt.Data());
        std::cerr << "Error: Module test txt file does not exist: " << moduleTxt << std::endl;
    } else {
        std::ifstream f_txt(moduleTxt.Data());
        if (!f_txt.is_open()) {
            result.moduleErrorFiles.push_back(moduleTxt.Data());
            std::cerr << "Error: Cannot open module test txt file: " << moduleTxt << std::endl;
        } else {
            f_txt.close();
        }
    }

    // Check module_test PDF (existence only)
    if (gSystem->AccessPathName(modulePdf, kFileExists)) {
        result.moduleErrorFiles.push_back(modulePdf.Data());
        std::cerr << "Error: Module test pdf file does not exist: " << modulePdf << std::endl;
    }

    // Set flags for module_test errors
    if (!result.moduleErrorFiles.empty()) {
        for (const auto& file : result.moduleErrorFiles) {
            if (file.find(".root") != std::string::npos) result.flags |= FLAG_MODULE_ROOT;
            if (file.find(".txt") != std::string::npos) result.flags |= FLAG_MODULE_TXT;
            if (file.find(".pdf") != std::string::npos) result.flags |= FLAG_MODULE_PDF;
        }
    }

    // Attempt to access directory contents
    TSystemDirectory pscanDir("pscan_files", pscanDirPath);
    TList* files = pscanDir.GetListOfFiles();
    
    // Handle directory access failure
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << pscanDirPath << std::endl;
        result.flags |= FLAG_DIR_ACCESS;
        return result;
    }

    // List of acceptable auxiliary files
    std::vector<std::string> acceptableAuxFiles = {
        "module_test_SETUP.root",
        "module_test_SETUP.txt",
        "module_test_SETUP.pdf"
    };

    // Iterate through all directory entries
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Skip directories and special files (., ..)
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        // Construct full file path
        TString filePath = TString::Format("%s/%s", pscanDirPath.Data(), fileName.Data());

        // Process electron text files
        if (fileName.EndsWith("_elect.txt")) {
            result.electronTxtCount++;
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                result.openErrorFiles.push_back(filePath.Data());
                std::cerr << "Error: Cannot open electron txt file: " << filePath << std::endl;
            } else {
                f_test.close();
            }
        }
        // Process hole text files
        else if (fileName.EndsWith("_holes.txt")) {
            result.holeTxtCount++;
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                result.openErrorFiles.push_back(filePath.Data());
                std::cerr << "Error: Cannot open hole txt file: " << filePath << std::endl;
            } else {
                f_test.close();
            }
        }
        // Process electron ROOT files
        else if (fileName.EndsWith("_elect.root")) {
            result.electronRootCount++;
            TFile* rootFile = TFile::Open(filePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                result.openErrorFiles.push_back(filePath.Data());
                std::cerr << "Error: Cannot open electron root file: " << filePath << std::endl;
            }
            if (rootFile) rootFile->Close();
        }
        // Process hole ROOT files
        else if (fileName.EndsWith("_holes.root")) {
            result.holeRootCount++;
            TFile* rootFile = TFile::Open(filePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                result.openErrorFiles.push_back(filePath.Data());
                std::cerr << "Error: Cannot open hole root file: " << filePath << std::endl;
            }
            if (rootFile) rootFile->Close();
        }
        // Handle unexpected files
        else {
            // Check if file is acceptable
            TString modulePrefix = TString::Format("module_test_%s", targetDir);
            bool isModuleFile = fileName.BeginsWith(modulePrefix) && 
                               (fileName.EndsWith(".root") || 
                                fileName.EndsWith(".txt") || 
                                fileName.EndsWith(".pdf"));
            
            bool isAcceptable = false;
            for (const auto& auxFile : acceptableAuxFiles) {
                if (fileName == auxFile) {
                    isAcceptable = true;
                    break;
                }
            }
            
            if (!isModuleFile && !isAcceptable) {
                result.unexpectedFiles.push_back(fileName.Data());
            }
        }
    }

    // Clean up directory list
    delete files;

    // Set flags based on counts
    if (result.electronTxtCount != 8) result.flags |= FLAG_ELECTRON_TXT;
    if (result.holeTxtCount != 8) result.flags |= FLAG_HOLE_TXT;
    if (result.electronRootCount != 8) result.flags |= FLAG_ELECTRON_ROOT;
    if (result.holeRootCount != 8) result.flags |= FLAG_HOLE_ROOT;
    if (!result.openErrorFiles.empty()) result.flags |= FLAG_FILE_OPEN;
    if (!result.unexpectedFiles.empty()) result.flags |= FLAG_UNEXPECTED_FILES;
    
    // Report file status
    std::cout << "\n===== Files Status =====" << std::endl;
    std::cout << "Electron text files: " << result.electronTxtCount << "/8 | "
              << "Status: " << ((result.flags & FLAG_ELECTRON_TXT) ? "FAIL" : "OK") 
              << (result.electronTxtCount < 8 ? " (UNDER)" : (result.electronTxtCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Hole text files:     " << result.holeTxtCount << "/8 | "
              << "Status: " << ((result.flags & FLAG_HOLE_TXT) ? "FAIL" : "OK")
              << (result.holeTxtCount < 8 ? " (UNDER)" : (result.holeTxtCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Electron ROOT files: " << result.electronRootCount << "/8 | "
              << "Status: " << ((result.flags & FLAG_ELECTRON_ROOT) ? "FAIL" : "OK")
              << (result.electronRootCount < 8 ? " (UNDER)" : (result.electronRootCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Hole ROOT files:     " << result.holeRootCount << "/8 | "
              << "Status: " << ((result.flags & FLAG_HOLE_ROOT) ? "FAIL" : "OK")
              << (result.holeRootCount < 8 ? " (UNDER)" : (result.holeRootCount > 8 ? " (OVER)" : "")) << std::endl;
              
    // Report module test status
    std::cout << "Module test root:  " << (result.flags & FLAG_MODULE_ROOT ? "ERROR" : "OK") << std::endl;
    std::cout << "Module test txt:   " << (result.flags & FLAG_MODULE_TXT ? "ERROR" : "OK") << std::endl;
    std::cout << "Module test pdf:   " << (result.flags & FLAG_MODULE_PDF ? "MISSING" : "OK") << std::endl;
    std::cout << "File accessibility:    " << (result.openErrorFiles.empty() ? "ALL OK" : "ERRORS DETECTED") << std::endl;
    
    // Report open errors if any
    if (!result.openErrorFiles.empty()) {
        std::cout << "\n===== FILE OPEN ERRORS =====" << std::endl;
        std::cout << "Files that could not be opened:" << std::endl;
        for (const auto& filePath : result.openErrorFiles) {
            std::cout << " - " << filePath << std::endl;
        }
    }
    
    // Report module errors if any
    if (!result.moduleErrorFiles.empty()) {
        std::cout << "\n===== MODULE TEST ERRORS =====" << std::endl;
        std::cout << "Problematic module test files:" << std::endl;
        for (const auto& filePath : result.moduleErrorFiles) {
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
        if (result.flags & FLAG_ELECTRON_TXT) std::cout << "[ELECTRON TXT COUNT] ";
        if (result.flags & FLAG_HOLE_TXT) std::cout << "[HOLE TXT COUNT] ";
        if (result.flags & FLAG_ELECTRON_ROOT) std::cout << "[ELECTRON ROOT COUNT] ";
        if (result.flags & FLAG_HOLE_ROOT) std::cout << "[HOLE ROOT COUNT] ";
        if (result.flags & FLAG_FILE_OPEN) std::cout << "[FILE ACCESS] ";
        if (result.flags & FLAG_MODULE_ROOT) std::cout << "[MODULE ROOT] ";
        if (result.flags & FLAG_MODULE_TXT) std::cout << "[MODULE TXT] ";
        if (result.flags & FLAG_MODULE_PDF) std::cout << "[MODULE PDF] ";
        if (result.flags & FLAG_UNEXPECTED_FILES) std::cout << "[UNEXPECTED FILES] ";
    }
    std::cout << std::endl;

    return result;
}