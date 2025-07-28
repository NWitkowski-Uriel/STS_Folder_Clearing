#include <TSystem.h>
#include <TString.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TInterpreter.h>
#include <iostream>
#include <vector>
#include <map>
#include <iomanip> // For std::setprecision

// ===================================================================
// Flag definitions for CheckTrimFiles (must match CheckTrimFiles.c)
// ===================================================================
#define TRIM_FLAG_TRIM_FOLDER_MISSING  0x01
#define TRIM_FLAG_DIR_ACCESS_ERROR     0x02
#define TRIM_FLAG_ELECTRON_COUNT       0x04
#define TRIM_FLAG_HOLE_COUNT           0x08
#define TRIM_FLAG_FILE_OPEN_ERROR      0x10

// ===================================================================
// Flag definitions for CheckPscanFiles (must match CheckPscanFiles.c)
// ===================================================================
#define PSCAN_FLAG_PSCAN_FOLDER_MISSING  0x01
#define PSCAN_FLAG_DIR_ACCESS_ERROR      0x02
#define PSCAN_FLAG_ELECTRON_TXT          0x04
#define PSCAN_FLAG_HOLE_TXT              0x08
#define PSCAN_FLAG_ELECTRON_ROOT         0x10
#define PSCAN_FLAG_HOLE_ROOT             0x20
#define PSCAN_FLAG_FILE_OPEN_ERROR       0x40
#define PSCAN_FLAG_MODULE_ROOT           0x80
#define PSCAN_FLAG_MODULE_TXT            0x100
#define PSCAN_FLAG_MODULE_PDF            0x200

// ===================================================================
// Flag definitions for CheckConnFiles (must match CheckConnFiles.c)
// ===================================================================
#define CONN_FLAG_CONN_FOLDER_MISSING    0x01
#define CONN_FLAG_DIR_ACCESS_ERROR       0x02
#define CONN_FLAG_ELECTRON_COUNT         0x04
#define CONN_FLAG_HOLE_COUNT             0x08
#define CONN_FLAG_FILE_OPEN_ERROR        0x10

// Typ wskaźnika do funkcji walidacyjnej
typedef int (*CheckFunction)(const char*);

// Funkcje dekodujące flagi na tekst
std::string decodeTrimFlags(int flags) {
    if (flags == 0) return "OK";
    
    std::vector<std::string> messages;
    if (flags & TRIM_FLAG_TRIM_FOLDER_MISSING) messages.push_back("TRIM_FILES_FOLDER_MISSING");
    if (flags & TRIM_FLAG_DIR_ACCESS_ERROR)    messages.push_back("DIR_ACCESS_ERROR");
    if (flags & TRIM_FLAG_ELECTRON_COUNT)      messages.push_back("ELECTRON_COUNT_ERROR");
    if (flags & TRIM_FLAG_HOLE_COUNT)          messages.push_back("HOLE_COUNT_ERROR");
    if (flags & TRIM_FLAG_FILE_OPEN_ERROR)     messages.push_back("FILE_OPEN_ERROR");
    
    std::string result;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) result += " | ";
        result += messages[i];
    }
    return result;
}

std::string decodePscanFlags(int flags) {
    if (flags == 0) return "OK";
    
    std::vector<std::string> messages;
    if (flags & PSCAN_FLAG_PSCAN_FOLDER_MISSING)  messages.push_back("PSCAN_FILES_FOLDER_MISSING");
    if (flags & PSCAN_FLAG_DIR_ACCESS_ERROR)      messages.push_back("DIR_ACCESS_ERROR");
    if (flags & PSCAN_FLAG_ELECTRON_TXT)          messages.push_back("ELECTRON_TXT_COUNT_ERROR");
    if (flags & PSCAN_FLAG_HOLE_TXT)              messages.push_back("HOLE_TXT_COUNT_ERROR");
    if (flags & PSCAN_FLAG_ELECTRON_ROOT)         messages.push_back("ELECTRON_ROOT_COUNT_ERROR");
    if (flags & PSCAN_FLAG_HOLE_ROOT)             messages.push_back("HOLE_ROOT_COUNT_ERROR");
    if (flags & PSCAN_FLAG_FILE_OPEN_ERROR)       messages.push_back("FILE_OPEN_ERROR");
    if (flags & PSCAN_FLAG_MODULE_ROOT)           messages.push_back("MODULE_ROOT_ERROR");
    if (flags & PSCAN_FLAG_MODULE_TXT)            messages.push_back("MODULE_TXT_ERROR");
    if (flags & PSCAN_FLAG_MODULE_PDF)            messages.push_back("MODULE_PDF_MISSING");
    
    std::string result;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) result += " | ";
        result += messages[i];
    }
    return result;
}

std::string decodeConnFlags(int flags) {
    if (flags == 0) return "OK";
    
    std::vector<std::string> messages;
    if (flags & CONN_FLAG_CONN_FOLDER_MISSING)   messages.push_back("CONN_CHECK_FILES_FOLDER_MISSING");
    if (flags & CONN_FLAG_DIR_ACCESS_ERROR)      messages.push_back("DIR_ACCESS_ERROR");
    if (flags & CONN_FLAG_ELECTRON_COUNT)        messages.push_back("ELECTRON_COUNT_ERROR");
    if (flags & CONN_FLAG_HOLE_COUNT)            messages.push_back("HOLE_COUNT_ERROR");
    if (flags & CONN_FLAG_FILE_OPEN_ERROR)       messages.push_back("FILE_OPEN_ERROR");
    
    std::string result;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) result += " | ";
        result += messages[i];
    }
    return result;
}

void Clear() {
    // Pobierz listę folderów w bieżącym katalogu
    TSystemDirectory dir(".", ".");
    TList *files = dir.GetListOfFiles();
    if (!files) {
        std::cerr << "Error: Could not get file list!" << std::endl;
        return;
    }

    std::vector<TString> dirNames;
    TIter next(files);
    TSystemFile *file;
    TObject *obj;
    while ((obj = next())) {
        file = dynamic_cast<TSystemFile*>(obj);
        if (!file) continue;
        
        TString name = file->GetName();
        if (name == "." || name == ".." || name.BeginsWith(".")) continue;

        if (file->IsDirectory()) {
            dirNames.push_back(name);
        }
    }
    delete files;

    if (dirNames.empty()) {
        std::cout << "No directories found!" << std::endl;
        return;
    }

    // Sprawdź dostępność programów
    bool useExecutableTrim = (gSystem->AccessPathName("CheckTrimFiles") == 0);
    bool useSourceTrim = (gSystem->AccessPathName("CheckTrimFiles.c") == 0);
    bool trimAvailable = useExecutableTrim || useSourceTrim;

    bool useExecutablePscan = (gSystem->AccessPathName("CheckPscanFiles") == 0);
    bool useSourcePscan = (gSystem->AccessPathName("CheckPscanFiles.c") == 0);
    bool pscanAvailable = useExecutablePscan || useSourcePscan;
    
    bool useExecutableConn = (gSystem->AccessPathName("CheckConnFiles") == 0);
    bool useSourceConn = (gSystem->AccessPathName("CheckConnFiles.c") == 0);
    bool connAvailable = useExecutableConn || useSourceConn;

    if (!trimAvailable && !pscanAvailable && !connAvailable) {
        std::cerr << "Error: No valid programs found!" << std::endl;
        return;
    }

    // Przygotuj wskaźniki funkcji (dla wersji źródłowej)
    CheckFunction trimFuncPtr = nullptr;
    CheckFunction pscanFuncPtr = nullptr;
    CheckFunction connFuncPtr = nullptr;

    // Załaduj funkcje z plików źródłowych (jeśli dostępne)
    if (useSourceTrim) {
        gInterpreter->ProcessLine(".L CheckTrimFiles.c+");
        trimFuncPtr = (CheckFunction)gInterpreter->ProcessLine("CheckTrimFiles;");
        if (!trimFuncPtr) {
            std::cerr << "Error loading CheckTrimFiles function!" << std::endl;
            trimAvailable = false;
        }
    }
    
    if (useSourcePscan) {
        gInterpreter->ProcessLine(".L CheckPscanFiles.c+");
        pscanFuncPtr = (CheckFunction)gInterpreter->ProcessLine("CheckPscanFiles;");
        if (!pscanFuncPtr) {
            std::cerr << "Error loading CheckPscanFiles function!" << std::endl;
            pscanAvailable = false;
        }
    }
    
    if (useSourceConn) {
        gInterpreter->ProcessLine(".L CheckConnFiles.c+");
        connFuncPtr = (CheckFunction)gInterpreter->ProcessLine("CheckConnFiles;");
        if (!connFuncPtr) {
            std::cerr << "Error loading CheckConnFiles function!" << std::endl;
            connAvailable = false;
        }
    }

    // Statystyki
    int totalDirs = dirNames.size();
    int passedDirs = 0;
    int failedDirs = 0;

    // Nagłówek raportu
    std::cout << "\n\n";
    std::cout << "====================================================" << std::endl;
    std::cout << "STARTING VALIDATION FOR " << totalDirs << " DIRECTORIES" << std::endl;
    std::cout << "====================================================\n" << std::endl;

    // Uruchom programy dla każdego folderu
    for (auto& dir : dirNames) {
        int trimFlags = 0;
        int pscanFlags = 0;
        int connFlags = 0;
        bool dirPassed = true;

        std::cout << "\n\n====================================================" << std::endl;
        std::cout << "VALIDATING DIRECTORY: " << dir << std::endl;
        std::cout << "====================================================" << std::endl;
        
        // 1. Uruchom CheckTrimFiles
        if (trimAvailable) {
            if (useExecutableTrim) {
                TString command = TString::Format("./CheckTrimFiles \"%s\"", dir.Data());
                std::cout << "\n[1/3] EXEC: " << command << std::endl;
                trimFlags = gSystem->Exec(command);
            } else {
                TString command = TString::Format(".x CheckTrimFiles.c+(\"%s\")", dir.Data());
                std::cout << "\n[1/3] EXEC: " << command << std::endl;
                if (trimFuncPtr) {
                    trimFlags = trimFuncPtr(dir.Data());
                } else {
                    std::cerr << "ERROR: CheckTrimFiles function not available!" << std::endl;
                    trimFlags = 0xFF; // Error code
                }
            }
            if (trimFlags != 0) dirPassed = false;
        }

        // 2. Uruchom CheckPscanFiles
        if (pscanAvailable) {
            if (useExecutablePscan) {
                TString command = TString::Format("./CheckPscanFiles \"%s\"", dir.Data());
                std::cout << "\n[2/3] EXEC: " << command << std::endl;
                pscanFlags = gSystem->Exec(command);
            } else {
                TString command = TString::Format(".x CheckPscanFiles.c+(\"%s\")", dir.Data());
                std::cout << "\n[2/3] EXEC: " << command << std::endl;
                if (pscanFuncPtr) {
                    pscanFlags = pscanFuncPtr(dir.Data());
                } else {
                    std::cerr << "ERROR: CheckPscanFiles function not available!" << std::endl;
                    pscanFlags = 0xFF; // Error code
                }
            }
            if (pscanFlags != 0) dirPassed = false;
        }
        
        // 3. Uruchom CheckConnFiles
        if (connAvailable) {
            if (useExecutableConn) {
                TString command = TString::Format("./CheckConnFiles \"%s\"", dir.Data());
                std::cout << "\n[3/3] EXEC: " << command << std::endl;
                connFlags = gSystem->Exec(command);
            } else {
                TString command = TString::Format(".x CheckConnFiles.c+(\"%s\")", dir.Data());
                std::cout << "\n[3/3] EXEC: " << command << std::endl;
                if (connFuncPtr) {
                    connFlags = connFuncPtr(dir.Data());
                } else {
                    std::cerr << "ERROR: CheckConnFiles function not available!" << std::endl;
                    connFlags = 0xFF; // Error code
                }
            }
            if (connFlags != 0) dirPassed = false;
        }

        // Generuj raport dla folderu
        std::cout << "\n\n====================================================" << std::endl;
        std::cout << "VALIDATION REPORT FOR: " << dir << std::endl;
        std::cout << "====================================================" << std::endl;
        
        if (trimAvailable) {
            std::cout << "CheckTrimFiles:  " << decodeTrimFlags(trimFlags) << std::endl;
        } else {
            std::cout << "CheckTrimFiles:  SKIPPED" << std::endl;
        }
        
        if (pscanAvailable) {
            std::cout << "CheckPscanFiles: " << decodePscanFlags(pscanFlags) << std::endl;
        } else {
            std::cout << "CheckPscanFiles: SKIPPED" << std::endl;
        }
        
        if (connAvailable) {
            std::cout << "CheckConnFiles:  " << decodeConnFlags(connFlags) << std::endl;
        } else {
            std::cout << "CheckConnFiles:  SKIPPED" << std::endl;
        }
        
        std::cout << "\nDIRECTORY STATUS: " 
                  << (dirPassed ? "PASSED" : "FAILED") 
                  << std::endl;
        std::cout << "====================================================\n" << std::endl;

        // Aktualizuj statystyki
        if (dirPassed) passedDirs++;
        else failedDirs++;
    }

    // Podsumowanie globalne
    std::cout << "\n\n";
    std::cout << "====================================================" << std::endl;
    std::cout << "FINAL VALIDATION SUMMARY" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << "Scanned directories: " << totalDirs << std::endl;
    std::cout << "Passed:             " << passedDirs << std::endl;
    std::cout << "Failed:             " << failedDirs << std::endl;
    std::cout << "Success rate:       " << std::fixed << std::setprecision(1) 
              << (totalDirs > 0 ? (100.0 * passedDirs / totalDirs) : 0) 
              << "%" << std::endl;
    std::cout << "====================================================" << std::endl;
}