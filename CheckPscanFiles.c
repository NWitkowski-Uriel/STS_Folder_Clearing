#include <iostream>
#include <fstream>
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
        
        // Report status flags (human-readable format)
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Pscan folder exists): 1" << std::endl;
        std::cout << "FLAG 1 (Electron txt):        1" << std::endl;
        std::cout << "FLAG 2 (Hole txt):            1" << std::endl;
        std::cout << "FLAG 3 (Electron root):       1" << std::endl;
        std::cout << "FLAG 4 (Hole root):           1" << std::endl;
        std::cout << "\nSummary: [PSCAN_FILES FOLDER MISSING]" << std::endl;
        
        // Set flag and return immediately
        resultFlags |= FLAG_PSCAN_FOLDER_MISSING;
        return resultFlags;
    }

    // Attempt to access directory contents
    TSystemDirectory pscanDir("pscan_files", pscanDirPath);
    TList* files = pscanDir.GetListOfFiles();
    
    // Handle directory access failure
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << pscanDirPath << std::endl;
        
        // Report status flags
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Directory access):  1" << std::endl;
        std::cout << "FLAG 1 (Electron txt):      1" << std::endl;
        std::cout << "FLAG 2 (Hole txt):          1" << std::endl;
        std::cout << "FLAG 3 (Electron root):     1" << std::endl;
        std::cout << "FLAG 4 (Hole root):         1" << std::endl;
        std::cout << "\nSummary: [DIRECTORY ACCESS ERROR]" << std::endl;
        
        resultFlags |= FLAG_DIR_ACCESS_ERROR;
        return resultFlags;
    }

    // Initialize counters for validation
    int electronTxtCount = 0;    // Tracks electron text files
    int holeTxtCount = 0;        // Tracks hole text files
    int electronRootCount = 0;    // Tracks electron ROOT files
    int holeRootCount = 0;        // Tracks hole ROOT files
    bool openErrors = false;      // Flags file access issues

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
    }

    // Validate file counts against expected quantity (8 each)
    bool flag_electron_txt = (electronTxtCount != 8);
    bool flag_hole_txt = (holeTxtCount != 8);
    bool flag_electron_root = (electronRootCount != 8);
    bool flag_hole_root = (holeRootCount != 8);

    // Update result flags based on validation
    if (flag_electron_txt) resultFlags |= FLAG_ELECTRON_TXT;
    if (flag_hole_txt) resultFlags |= FLAG_HOLE_TXT;
    if (flag_electron_root) resultFlags |= FLAG_ELECTRON_ROOT;
    if (flag_hole_root) resultFlags |= FLAG_HOLE_ROOT;
    if (openErrors) resultFlags |= FLAG_FILE_OPEN_ERROR;

    // Generate comprehensive validation report
    std::cout << "\n===== Validation Report =====" << std::endl;
    std::cout << "Current location:       " << currentDir << std::endl;
    std::cout << "Target directory:       " << targetDir << std::endl;
    std::cout << "Full target path:       " << fullTargetPath << std::endl;
    std::cout << "Pscan files location:   " << pscanDirPath << std::endl;
    
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
              
    std::cout << "File accessibility:    " << (openErrors ? "ERRORS DETECTED" : "ALL FILES ACCESSIBLE") << std::endl;
              
    // Report flag status (bitmask interpretation)
    std::cout << "\n===== Final Flags Status (Bitmask) =====" << std::endl;
    std::cout << "FLAG 0 (Electron txt):    " << flag_electron_txt << std::endl;
    std::cout << "FLAG 1 (Hole txt):        " << flag_hole_txt << std::endl;
    std::cout << "FLAG 2 (Electron root):   " << flag_electron_root << std::endl;
    std::cout << "FLAG 3 (Hole root):       " << flag_hole_root << std::endl;
    std::cout << "FLAG 4 (File access):     " << openErrors << std::endl;
    
    // Generate summary of issues
    std::cout << "\nSummary: ";
    if (!flag_electron_txt && !flag_hole_txt && 
        !flag_electron_root && !flag_hole_root && !openErrors) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (flag_electron_txt) std::cout << "[ELECTRON TXT COUNT] ";
        if (flag_hole_txt) std::cout << "[HOLE TXT COUNT] ";
        if (flag_electron_root) std::cout << "[ELECTRON ROOT COUNT] ";
        if (flag_hole_root) std::cout << "[HOLE ROOT COUNT] ";
        if (openErrors) std::cout << "[FILE ACCESS ISSUES]";
    }
    std::cout << std::endl;

    return resultFlags;  // Return combined flags as bitmask
}