// SP_RodrigoLourenço_Level1.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX        // prevent Windows.h from defining min/max macros
#endif
#include <Windows.h>      // for SetConsoleCP / SetConsoleOutputCP
#endif

#include "Vector.h"          // lab-implemented dynamic array
#include "Map.h"             // lab-implemented associative map

// Array of years, used both for file names and indexing
static const std::string YEARS[] = { "2020", "2021", "2022", "2023", "2024" };
static const size_t N_YEARS = sizeof(YEARS) / sizeof(YEARS[0]);

// Helper: remove any non-alphanumeric characters from a code string
static std::string cleanCode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            out.push_back(c);
        }
    }
    return out;
}

// Data structure holding one municipality's name, code, and
// male/female counts for each of the five years
struct FlatMunicipality {
    std::string name;
    std::string code;
    int male[N_YEARS] = { 0 };
    int female[N_YEARS] = { 0 };
};

// Load all five CSV files (named "2020.csv", ..., "2024.csv").
// Merge records by municipality code into a single Vector.
// Uses the lab Map to track code → index mapping.
static void loadAll(Vector<FlatMunicipality>& out) {
    Map<std::string, size_t> indexMap;

    for (size_t yi = 0; yi < N_YEARS; ++yi) {
        std::ifstream file(YEARS[yi] + ".csv");
        if (!file) {
            std::cerr << "Warning: cannot open " << YEARS[yi] << ".csv\n";
            continue;
        }

        std::string line;
        std::getline(file, line);  // skip header row

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::istringstream ss(line);
            FlatMunicipality temp;
            std::string token;

            // Read name
            std::getline(ss, temp.name, ';');

            // Read and clean code
            std::getline(ss, token, ';');
            temp.code = cleanCode(token);

            // Read male count for this year
            std::getline(ss, token, ';');
            temp.male[yi] = std::stoi(token);

            // Skip next column
            std::getline(ss, token, ';');

            // Read female count for this year
            std::getline(ss, token, ';');
            temp.female[yi] = std::stoi(token);

            // Merge into 'out' using the Map for lookup
            auto it = indexMap.find(temp.code);
            if (it == indexMap.end()) {
                // Not yet inserted: add new record
                out.push_back(temp);
                indexMap[temp.code] = out.size() - 1;
            }
            else {
                // Already exists: update the existing entry's counts
                size_t idx = it->second;
                out[idx].male[yi] = temp.male[yi];
                out[idx].female[yi] = temp.female[yi];
            }
        }
    }
}

// Generic filter algorithm: takes a container and a predicate,
// returns a new Vector of elements satisfying the predicate.
template<typename Pred>
Vector<FlatMunicipality> filter(const Vector<FlatMunicipality>& data, Pred predicate) {
    Vector<FlatMunicipality> result;
    for (size_t i = 0; i < data.size(); ++i) {
        if (predicate(data[i])) {
            result.push_back(data[i]);
        }
    }
    return result;
}

// Print one municipality record, showing name, code, and
// male/female/total for each year.
static void printFlat(const FlatMunicipality& m) {
    std::cout << "Name: " << m.name
        << ", Code: " << m.code << "\n";
    for (size_t i = 0; i < N_YEARS; ++i) {
        int mn = m.male[i], fn = m.female[i];
        std::cout << "  " << YEARS[i]
            << ": Male=" << mn
                << ", Female=" << fn
                << ", Total=" << (mn + fn)
                << "\n";
    }
    std::cout << "\n";
}

// Program entry point:
// - On Windows, set console to UTF-8 so accents display properly
// - Load all data once
// - Present interactive menu to filter by substring, max, or min
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    Vector<FlatMunicipality> data;
    try {
        loadAll(data);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal load error: " << e.what() << "\n";
        return 1;
    }

    int choice;
    do {
        std::cout << "/=============== MENU ================/\n"
            << "/  [1] Filter by Name substring       /\n"
            << "/  [2] Filter by Maximum population   /\n"
            << "/  [3] Filter by Minimum population   /\n"
            << "/  [0] Exit                           /\n"
            << "/=====================================/\n"
            << "Choose: ";
        std::cin >> choice;

        if (choice == 1) {
            // Substring filter
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Enter substring: ";
            std::string sub;
            std::getline(std::cin, sub);
            for (auto& c : sub) c = std::tolower(static_cast<unsigned char>(c));

            auto matches = filter(data, [&](const FlatMunicipality& m) {
                std::string low = m.name;
                for (auto& c : low) c = std::tolower(static_cast<unsigned char>(c));
                return low.find(sub) != std::string::npos;
                });

            if (matches.size() == 0) {
                std::cout << "No matches.\n\n";
            }
            else {
                for (size_t i = 0; i < matches.size(); ++i)
                    printFlat(matches[i]);
            }
        }
        else if (choice == 2 || choice == 3) {
            // Max/min population filter for a specific year
            std::cout << "Enter year (Between 2020 and 2024): ";
            std::string yr;
            std::cin >> yr;

            size_t yi = 0;
            while (yi < N_YEARS && YEARS[yi] != yr) ++yi;
            if (yi == N_YEARS) {
                std::cerr << "Invalid year.\n\n";
                continue;
            }

            std::cout << "Enter "
                << (choice == 2 ? "max" : "min")
                << " total population: ";
            int threshold;
            std::cin >> threshold;

            auto matches = filter(data, [&](const FlatMunicipality& m) {
                int total = m.male[yi] + m.female[yi];
                return choice == 2 ? (total <= threshold)
                    : (total >= threshold);
                });

            if (matches.size() == 0) {
                std::cout << "No matches.\n\n";
            }
            else {
                for (size_t i = 0; i < matches.size(); ++i)
                    printFlat(matches[i]);
            }
        }

    } while (choice != 0);

    return 0;
}