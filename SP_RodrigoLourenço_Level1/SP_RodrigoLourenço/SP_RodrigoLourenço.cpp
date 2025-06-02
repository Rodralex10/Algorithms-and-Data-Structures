// SP_RodrigoLourenço_Level1.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <limits>
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>


#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "Vector.h"
#include "Map.h"

// Array of years for iteration
static const std::string YEARS[] = { "2020", "2021", "2022", "2023", "2024" };
static const size_t N_YEARS = sizeof(YEARS) / sizeof(YEARS[0]);

// Clean up non-alphanumeric characters from code strings
static std::string cleanCode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back(c);
    }
    return out;
}

// Data per municipality with 5 years of data
struct FlatMunicipality {
    std::string name, code;
    int male[N_YEARS] = { 0 };
    int female[N_YEARS] = { 0 };
};

// Load all years and merge by municipality code
static void loadAll(Vector<FlatMunicipality>& out) {
    Map<std::string, size_t> indexMap;

    for (size_t yi = 0; yi < N_YEARS; ++yi) {
        std::ifstream file(YEARS[yi] + ".csv");
        if (!file) {
            std::cerr << "Warning: Cannot open " << YEARS[yi] << ".csv\n";
            continue;
        }

        std::string line;
        std::getline(file, line);  // skip header

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            FlatMunicipality temp;
            std::string token;

            std::getline(ss, temp.name, ';');
            std::getline(ss, token, ';'); temp.code = cleanCode(token);
            std::getline(ss, token, ';'); temp.male[yi] = std::stoi(token);
            std::getline(ss, token, ';'); // skip
            std::getline(ss, token, ';'); temp.female[yi] = std::stoi(token);

            auto it = indexMap.find(temp.code);
            if (it == indexMap.end()) {
                out.push_back(temp);
                indexMap[temp.code] = out.size() - 1;
            }
            else {
                size_t idx = it->second;
                out[idx].male[yi] = temp.male[yi];
                out[idx].female[yi] = temp.female[yi];
            }
        }
    }
}

// Generic filter for any predicate
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

// Print one municipality's record
static void printFlat(const FlatMunicipality& m) {
    std::cout << "Name: " << m.name << ", Code: " << m.code << "\n";
    for (size_t i = 0; i < N_YEARS; ++i) {
        std::cout << "  " << YEARS[i] << ": Male=" << m.male[i]
            << ", Female=" << m.female[i]
                << ", Total=" << (m.male[i] + m.female[i]) << "\n";
    }
    std::cout << "\n";
}

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);


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

            if (matches.size() == 0) std::cout << "No matches.\n\n";
            else for (size_t i = 0; i < matches.size(); ++i) printFlat(matches[i]);
        }
        else if (choice == 2 || choice == 3) {
            std::cout << "Enter year (2020–2024): ";
            std::string yr;
            std::cin >> yr;
            size_t yi = 0;
            while (yi < N_YEARS && YEARS[yi] != yr) ++yi;
            if (yi == N_YEARS) {
                std::cerr << "Invalid year.\n\n";
                continue;
            }

            std::cout << "Enter " << (choice == 2 ? "max" : "min") << " total population: ";
            int threshold;
            std::cin >> threshold;

            auto matches = filter(data, [&](const FlatMunicipality& m) {
                int total = m.male[yi] + m.female[yi];
                return choice == 2 ? (total <= threshold) : (total >= threshold);
                });

            if (matches.size() == 0) std::cout << "No matches.\n\n";
            else for (size_t i = 0; i < matches.size(); ++i) printFlat(matches[i]);
        }

    } while (choice != 0);

    return 0;
}