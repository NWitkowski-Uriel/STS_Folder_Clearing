#include <iostream>
#include <fstream>
#include <vector>
#include "TSystem.h"
#include "TSystemDirectory.h"
#include "TFile.h"

// Define flag constants using bitmask
#define FLAG_PSCAN_FOLDER_MISSING  0x01   // pscan_files directory missing
#define FLAG_DIR_ACCESS_ERROR      0x02   // Error accessing directory
#define FLAG_ELECTRON_TXT          0x04   // Incorrect electron txt file count
#define FLAG_HOLE_TXT              0x08   // Incorrect hole txt file count
#define FLAG_ELECTRON_ROOT         0x10   // Incorrect electron root file count
#define FLAG_HOLE_ROOT             0x20   // Incorrect hole root file count
#define FLAG_FILE_OPEN_ERROR       0x40   // File opening error
#define FLAG_MODULE_ROOT           0x80   // Module test root file error
#define FLAG_MODULE_TXT            0x100  // Module test txt file error
#define FLAG_MODULE_PDF            0x200  // Module test pdf file error
#define FLAG_UNEXPECTED_FILES      0x400  // Unexpected files in directory

int CheckPscanFiles(const char* targetDir) {
    int resultFlags = 0;  // Initialize flag container (bitmask)

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory (assumed to exist)
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Construct full path to pscan_files directory
    TString pscanDirPath = TString::Format("%s/%s/pscan_files", currentDir.Data(), targetDir);

    // PRIMARY CHECK: Verify existence of 'pscan_files' directory
    if (gSystem->AccessPathName(pscanDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'pscan_files' does not exist!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << pscanDirPath << std::endl;
        
        // Report status flags (including module_test results)
        std::cout << "\n===== Module Test Files Status =====" << std::endl;
        std::cout << "Module test root:  MISSING (pscan_files folder missing)" << std::endl;
        std::cout << "Module test txt:   MISSING (pscan_files folder missing)" << std::endl;
        std::cout << "Module test pdf:   MISSING (pscan_files folder missing)" << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Pscan folder exists): 1" << std::endl;
        std::cout << "FLAG 1 (Electron txt):        1" << std::endl;
        std::cout << "FLAG 2 (Hole txt):            1" << std::endl;
        std::cout << "FLAG 3 (Electron root):       1" << std::endl;
        std::cout << "FLAG 4 (Hole root):           1" << std::endl;
        std::cout << "FLAG 5 (Module root):         1" << std::endl;
        std::cout << "FLAG 6 (Module txt):          1" << std::endl;
        std::cout << "FLAG 7 (Module pdf):          1" << std::endl;
        std::cout << "\nSummary: [PSCAN_FILES FOLDER MISSING] [MODULE ROOT] [MODULE TXT] [MODULE PDF]" << std::endl;
        
        // Set flag and return
        resultFlags |= FLAG_PSCAN_FOLDER_MISSING;
        resultFlags |= FLAG_MODULE_ROOT;
        resultFlags |= FLAG_MODULE_TXT;
        resultFlags |= FLAG_MODULE_PDF;
        return resultFlags;
    }

    // ===== Check module_test files in pscan_files directory =====
    TString moduleRoot = TString::Format("%s/module_test_%s.root", pscanDirPath.Data(), targetDir);
    TString moduleTxt = TString::Format("%s/module_test_%s.txt", pscanDirPath.Data(), targetDir);
    TString modulePdf = TString::Format("%s/module_test_%s.pdf", pscanDirPath.Data(), targetDir);

    bool moduleRootError = false;
    bool moduleTxtError = false;
    bool modulePdfError = false;

    // Check module_test ROOT
    if (gSystem->AccessPathName(moduleRoot, kFileExists)) {
        moduleRootError = true;
        std::cerr << "Error: Module test root file does not exist: " << moduleRoot << std::endl;
    } else {
        TFile* f_root = TFile::Open(moduleRoot, "READ");
        if (!f_root || f_root->IsZombie()) {
            moduleRootError = true;
            std::cerr << "Error: Cannot open module test root file: " << moduleRoot << std::endl;
        }
        if (f_root) f_root->Close();
    }

    // Check module_test TXT
    if (gSystem->AccessPathName(moduleTxt, kFileExists)) {
        moduleTxtError = true;
        std::cerr << "Error: Module test txt file does not exist: " << moduleTxt << std::endl;
    } else {
        std::ifstream f_txt(moduleTxt.Data());
        if (!f_txt.is_open()) {
            moduleTxtError = true;
            std::cerr << "Error: Cannot open module test txt file: " << moduleTxt << std::endl;
        } else {
            f_txt.close();
        }
    }

    // Check module_test PDF (existence only)
    if (gSystem->AccessPathName(modulePdf, kFileExists)) {
        modulePdfError = true;
        std::cerr << "Error: Module test pdf file does not exist: " << modulePdf << std::endl;
    }

    // Set flags for module_test errors
    if (moduleRootError) resultFlags |= FLAG_MODULE_ROOT;
    if (moduleTxtError) resultFlags |= FLAG_MODULE_TXT;
    if (modulePdfError) resultFlags |= FLAG_MODULE_PDF;

    // Attempt to access directory contents
    TSystemDirectory pscanDir("pscan_files", pscanDirPath);
    TList* files = pscanDir.GetListOfFiles();

    // Initialize counters for validation
    int electronTxtCount = 0;    // Tracks electron text files
    int holeTxtCount = 0;        // Tracks hole text files
    int electronRootCount = 0;    // Tracks electron ROOT files
    int holeRootCount = 0;        // Tracks hole ROOT files
    bool openErrors = false;      // Flags file access issues
    
    // List of acceptable auxiliary files
    std::vector<TString> acceptableAuxFiles = {
        "a.txt",
        "find_ASICs.sh",
        "count_select_sort_files.sh",
        "count_select_sort_root_files.sh",     
        "execution.C",
        "plot.txt",
        "plot_1024.C",
        "plot_1024_Maria.C",
        "trim_adc.cxx",
        "trim_adc.hxx",
        "module_test_SETUP.root",
        "module_test_SETUP.txt",
        "module_test_SETUP.pdf"
    };
    
    // List to collect unexpected files
    std::vector<TString> unexpectedFiles;

    // Iterate through all directory entries
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Skip directories and special files (., ..)
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        // Construct full file path
        TString filePath = TString::Format("%s/%s", pscanDirPath.Data(), fileName.Data());

        // Process electron text files (expected suffix: '_elect.txt')
        if (fileName.EndsWith("_elect.txt")) {
            electronTxtCount++;
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron txt file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();  // Properly close accessible files
            }
        }
        // Process hole text files (expected suffix: '_holes.txt')
        else if (fileName.EndsWith("_holes.txt")) {
            holeTxtCount++;
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole txt file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();  // Properly close accessible files
            }
        }
        // Process electron ROOT files (expected suffix: '_elect.root')
        else if (fileName.EndsWith("_elect.root")) {
            electronRootCount++;
            // Use ROOT's TFile for proper ROOT file validation
            TFile* rootFile = TFile::Open(filePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open electron root file: " << filePath << std::endl;
                openErrors = true;
            }
            // Clean up resources if file was opened
            if (rootFile) rootFile->Close();
        }
        // Process hole ROOT files (expected suffix: '_holes.root')
        else if (fileName.EndsWith("_holes.root")) {
            holeRootCount++;
            // Use ROOT's TFile for proper ROOT file validation
            TFile* rootFile = TFile::Open(filePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open hole root file: " << filePath << std::endl;
                openErrors = true;
            }
            // Clean up resources if file was opened
            if (rootFile) rootFile->Close();
        }
        // Check for unexpected files
        else {
            // Check against module test files
            TString modulePrefix = TString::Format("module_test_%s", targetDir);
            bool isModuleFile = fileName.BeginsWith(modulePrefix) && 
                               (fileName.EndsWith(".root") || 
                                fileName.EndsWith(".txt") || 
                                fileName.EndsWith(".pdf"));
            
            // Check against acceptable auxiliary files
            bool isAcceptable = false;
            for (const auto& auxFile : acceptableAuxFiles) {
                if (fileName == auxFile) {
                    isAcceptable = true;
                    break;
                }
            }
            
            // Collect unexpected files
            if (!isModuleFile && !isAcceptable) {
                unexpectedFiles.push_back(fileName);
            }
        }
    }

    // Validate file counts against expected quantity (8 each)
    bool flag_electron_txt = (electronTxtCount != 8);
    bool flag_hole_txt = (holeTxtCount != 8);
    bool flag_electron_root = (electronRootCount != 8);
    bool flag_hole_root = (holeRootCount != 8);
    
    // Set flag if unexpected files found
    bool flag_unexpected_files = !unexpectedFiles.empty();

    // Update result flags based on validation
    if (flag_electron_txt) resultFlags |= FLAG_ELECTRON_TXT;
    if (flag_hole_txt) resultFlags |= FLAG_HOLE_TXT;
    if (flag_electron_root) resultFlags |= FLAG_ELECTRON_ROOT;
    if (flag_hole_root) resultFlags |= FLAG_HOLE_ROOT;
    if (openErrors) resultFlags |= FLAG_FILE_OPEN_ERROR;
    if (flag_unexpected_files) resultFlags |= FLAG_UNEXPECTED_FILES;

    // Report module_test status
    std::cout << "\n===== Files Status =====" << std::endl;

    // Report file counts and validation status
    std::cout << "Electron text files: " << electronTxtCount << "/8 | "
              << "Status: " << (flag_electron_txt ? "FAIL" : "OK") 
              << (electronTxtCount < 8 ? " (UNDER)" : (electronTxtCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Hole text files:     " << holeTxtCount << "/8 | "
              << "Status: " << (flag_hole_txt ? "FAIL" : "OK")
              << (holeTxtCount < 8 ? " (UNDER)" : (holeTxtCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Electron ROOT files: " << electronRootCount << "/8 | "
              << "Status: " << (flag_electron_root ? "FAIL" : "OK")
              << (electronRootCount < 8 ? " (UNDER)" : (electronRootCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Hole ROOT files:     " << holeRootCount << "/8 | "
              << "Status: " << (flag_hole_root ? "FAIL" : "OK")
              << (holeRootCount < 8 ? " (UNDER)" : (holeRootCount > 8 ? " (OVER)" : "")) << std::endl;
              
    std::cout << "Module test root:  " << (moduleRootError ? "MISSING/ERROR" : "OK") << std::endl;
    std::cout << "Module test txt:   " << (moduleTxtError ? "MISSING/ERROR" : "OK") << std::endl;
    std::cout << "Module test pdf:   " << (modulePdfError ? "MISSING" : "OK") << std::endl;
    std::cout << "File accessibility:    " << (openErrors ? "ERRORS DETECTED" : "ALL FILES ACCESSIBLE") << std::endl;
    
    // Report unexpected files if any
    if (!unexpectedFiles.empty()) {
        std::cout << "\n===== Unexpected Files Found =====" << std::endl;
        std::cout << "Number of unexpected files: " << unexpectedFiles.size() << std::endl;
        for (const auto& file : unexpectedFiles) {
            std::cout << "  - " << file << std::endl;
        }
    }
              
    // Report flag status (bitmask interpretation)
    std::cout << "\n===== Final Flags Status (Bitmask) =====" << std::endl;
    std::cout << "FLAG 0 (Pscan folder exists): 0" << std::endl;
    std::cout << "FLAG 1 (Electron txt):        " << flag_electron_txt << std::endl;
    std::cout << "FLAG 2 (Hole txt):            " << flag_hole_txt << std::endl;
    std::cout << "FLAG 3 (Electron root):       " << flag_electron_root << std::endl;
    std::cout << "FLAG 4 (Hole root):           " << flag_hole_root << std::endl;
    std::cout << "FLAG 5 (File access):         " << openErrors << std::endl;
    std::cout << "FLAG 6 (Module root):         " << moduleRootError << std::endl;
    std::cout << "FLAG 7 (Module txt):          " << moduleTxtError << std::endl;
    std::cout << "FLAG 8 (Module pdf):          " << modulePdfError << std::endl;
    std::cout << "FLAG 9 (Unexpected files):    " << flag_unexpected_files << std::endl;
    
    // Generate summary of issues
    std::cout << "\nSummary: ";
    if (!flag_electron_txt && !flag_hole_txt && !flag_electron_root && 
        !flag_hole_root && !openErrors && !moduleTxtError && 
        !moduleRootError && !modulePdfError && !flag_unexpected_files) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (flag_electron_txt) std::cout << "[ELECTRON TXT COUNT] ";
        if (flag_hole_txt) std::cout << "[HOLE TXT COUNT] ";
        if (flag_electron_root) std::cout << "[ELECTRON ROOT COUNT] ";
        if (flag_hole_root) std::cout << "[HOLE ROOT COUNT] ";
        if (openErrors) std::cout << "[FILE ACCESS ISSUES] ";
        if (moduleTxtError) std::cout << "[MODULE TXT] ";
        if (moduleRootError) std::cout << "[MODULE ROOT] ";
        if (modulePdfError) std::cout << "[MODULE PDF] ";
        if (flag_unexpected_files) std::cout << "[UNEXPECTED FILES] ";
    }
    std::cout << std::endl;

    return resultFlags;  // Return combined flags as bitmask
}