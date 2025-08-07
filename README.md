EXORCISM - Ladder Test Data Validation System
=============================================

Overview
--------
EXORCISM is a comprehensive validation system for semiconductor ladder test data directories. It performs automated checks on directory structures, file naming conventions, content validity, and generates detailed reports before and after cleanup operations.

Key Features
------------
- Multi-level Validation: Checks log files, trim files, parameter scan (pscan) files, and connection test files
- Smart Matching: Automatically matches test data files with corresponding tester FEB files
- Comprehensive Reporting: Generates reports in TXT, ROOT, and PDF formats
- Interactive Cleanup: Guided removal of problematic files with user confirmation
- Status Classification: Classifies directories as PASSED, PASSED WITH ISSUES, or FAILED
- Visual Analytics: Includes pie charts and color-coded status indicators in PDF reports

Requirements
------------
- ROOT data analysis framework (v6.24 or later)
- C++17 compatible compiler
- Standard Unix/Linux environment

Installation
------------
1. Clone the repository:
   git clone https://github.com/yourusername/exorcism.git
   cd exorcism

2. Compile the program:
   make

Usage
-----
Basic Validation:
./Exorcism

The program will:
1. Scan the current directory for test data folders
2. Perform validation checks on all found directories
3. Generate pre-cleanup reports
4. Guide you through interactive cleanup
5. Perform post-cleanup validation
6. Generate final reports

Report Files
------------
The system creates three types of reports:
- Text reports: Human-readable plain text format
- ROOT reports: For programmatic analysis
- PDF reports: Professional formatted with visual elements

Reports are generated both before and after cleanup with timestamps in filenames.

Validation Checks
-----------------
Log Files Validation:
- Directory existence
- Presence of main log file
- Data files existence and content
- Matching tester FEB files
- File naming conventions with timestamps
- File accessibility and content validity

Trim Files Validation:
- Subdirectory existence
- Exactly 8 electron files (*_elect.txt)
- Exactly 8 hole files (*_holes.txt)
- Proper HW index format (0-7) in filenames
- No duplicate HW indices
- File accessibility
- No unexpected files

Pscan Files Validation:
- Subdirectory existence
- Module test files (root/txt/pdf)
- Exactly 8 electron text files
- Exactly 8 hole text files
- Exactly 8 electron root files
- Exactly 8 hole root files
- File accessibility and validity
- No unexpected files

Connection Files Validation:
- Subdirectory existence
- Exactly 8 electron files
- Exactly 8 hole files
- File accessibility
- No unexpected files

Cleanup Features
---------------
The interactive cleanup system (Extra_Omnes) helps remove:
- Invalid data files and their matched tester files
- Empty files
- Files with incorrect naming formats
- Unexpected files in test directories

All cleanup operations require user confirmation before execution.

Sample Output
-------------
VALIDATION SUMMARY
====================================================
Ladder:          Ladder_12AB
Total directories: 24
Passed:          18
Passed with issues: 4
Failed:          2
Success rate:    91.7%
Warning rate among passed: 18.2%
Overall Status: "GOOD (≥80% success)"

Contributing
------------
Contributions are welcome! Please fork the repository and submit pull requests.

License
-------
This project is licensed under the MIT License.

Support
-------
For issues or questions, please open an issue in the GitHub repository.
