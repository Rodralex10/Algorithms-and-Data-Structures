// SP_RodrigoLourenço_Level3.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <limits>
#include <functional>
#include <locale>
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>


#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>    // for SetConsoleCP / SetConsoleOutputCP
#endif

#include "Vector.h"        // dynamic array (with assert in operator[])
#include "Map.h"           // associative map for per-year pop data
#include "HashMap.h"       // custom hash table (never deletes values)

// === Level 1 flat-data structures & functions ===

struct FlatMunicipality {
    std::string name, code;
    int male = 0, female = 0;
};

static std::string cleanCode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back(c);
    }
    return out;
}

static void loadFlat(const std::string& fn, Vector<FlatMunicipality>& out) {
    std::ifstream f(fn);
    if (!f) {
        std::cerr << "[loadFlat] Could not open " << fn << "\n";
        return;
    }
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        FlatMunicipality m;
        std::string tmp;
        std::getline(ss, m.name, ';');
        std::getline(ss, tmp, ';'); m.code = cleanCode(tmp);
        std::getline(ss, tmp, ';'); m.male = std::stoi(tmp);
        std::getline(ss, tmp, ';');            // skip one field
        std::getline(ss, tmp, ';'); m.female = std::stoi(tmp);
        out.push_back(m);
    }
}

static void printFlat(const FlatMunicipality& m) {
    std::cout << "Name: " << m.name
        << ", Code: " << m.code
        << ", Male=" << m.male
        << ", Female=" << m.female
        << ", Total=" << (m.male + m.female)
        << "\n";
}

template<typename T, typename Pred>
static Vector<T> filter(const Vector<T>& data, Pred pred) {
    Vector<T> result;
    for (size_t i = 0; i < data.size(); ++i) {
        if (pred(data[i])) {
            result.push_back(data[i]);
        }
    }
    return result;
}

// === Years of data ===

static const Vector<std::string> YEARS = []() {
    Vector<std::string> v;
    v.push_back("2020");
    v.push_back("2021");
    v.push_back("2022");
    v.push_back("2023");
    v.push_back("2024");
    return v;
    }();

// === Level 2 hierarchy definitions ===

struct TerritorialUnit {
    std::string name, code, type;
    Map<std::string, std::pair<int, int>> popByYear;
};

struct HierarchyNode {
    TerritorialUnit unit;
    Vector<HierarchyNode*> children;
    HierarchyNode* parent = nullptr;

    HierarchyNode(const TerritorialUnit& u) : unit(u) {}
    ~HierarchyNode() {
        for (size_t i = 0; i < children.size(); ++i) {
            delete children[i];
        }
    }
};

static std::string determineType(const std::string& c) {
    if (c == "AT")                 return "Country";
    if (c.rfind("AT", 0) == 0) {
        size_t L = c.size() - 2;
        if (L == 1) return "GeoDiv";
        if (L == 2) return "State";
        if (L == 3) return "Region";
    }
    return "Municipality";
}

// Build lookup: code → HierarchyNode*
static void buildLookup(HierarchyNode* node,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    lookup[node->unit.code] = node;
    for (size_t i = 0; i < node->children.size(); ++i) {
        buildLookup(node->children[i], lookup);
    }
}

// Load regions from country.csv (format: Name;Code)
static HierarchyNode* loadRegions(const std::string& fn) {
    std::ifstream f(fn);
    if (!f) throw std::runtime_error("Cannot open " + fn);

    // Root node: Austria
    HierarchyNode* root = new HierarchyNode({ "Austria", "AT", "Country" });

    Vector<std::pair<std::string, std::string>> entries;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string nm, cr;
        std::getline(ss, nm, ';');
        std::getline(ss, cr, ';');
        entries.push_back({ nm, cleanCode(cr) });
    }

    // Temporary map code → new node pointer
    HashMap<std::string, HierarchyNode*> temp;
    temp["AT"] = root;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        TerritorialUnit u;
        u.name = e.first;
        u.code = e.second;
        u.type = determineType(e.second);
        temp[e.second] = new HierarchyNode(u);
    }

    // Link parent↔child
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        std::string c = e.second;
        std::string p = (c.size() > 2 ? c.substr(0, c.size() - 1) : "AT");
        HierarchyNode** pptr = temp.find(p);
        if (pptr) {
            HierarchyNode* parent = *pptr;
            HierarchyNode* child = temp[c];
            child->parent = parent;
            parent->children.push_back(child);
        }
    }

    return root;
}

// Load municipalities from municipalities.csv (Name;Code;RegionCode)
static void loadMunicipalities(const std::string& fn,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    std::ifstream f(fn);
    if (!f) throw std::runtime_error("Cannot open " + fn);

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string nm, cr, rr;
        std::getline(ss, nm, ';');
        std::getline(ss, cr, ';');
        std::getline(ss, rr, ';');
        std::string code = cleanCode(cr);
        std::string region = cleanCode(rr);

        TerritorialUnit u;
        u.name = nm;
        u.code = code;
        u.type = "Municipality";

        HierarchyNode* node = new HierarchyNode(u);
        HierarchyNode** parentPtr = lookup.find(region);
        if (parentPtr) {
            HierarchyNode* parent = *parentPtr;
            node->parent = parent;
            parent->children.push_back(node);
            lookup[code] = node;
        }
        else {
            delete node;
        }
    }
}

// Load population data from YYYY.csv for each year in YEARS
static void loadPopData(const Vector<std::string>& yrs,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    for (size_t yi = 0; yi < yrs.size(); ++yi) {
        std::ifstream f(yrs[yi] + ".csv");
        if (!f) continue;
        std::string line;
        std::getline(f, line); // skip header
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            std::string nm, cr, mstr, skip, fstr;
            std::getline(ss, nm, ';');
            std::getline(ss, cr, ';');
            std::getline(ss, mstr, ';');
            std::getline(ss, skip, ';');
            std::getline(ss, fstr, ';');
            int male = std::stoi(mstr);
            int female = std::stoi(fstr);
            std::string code = cleanCode(cr);
            HierarchyNode** nptr = lookup.find(code);
            if (nptr) {
                HierarchyNode* node = *nptr;
                auto& entry = node->unit.popByYear[yrs[yi]];
                entry.first += male;
                entry.second += female;
            }
        }
    }
}

// Recursively accumulate child populations into parents
static void accumulate(HierarchyNode* n) {
    for (size_t i = 0; i < n->children.size(); ++i) {
        accumulate(n->children[i]);
        auto& me = n->unit.popByYear;
        auto& ch = n->children[i]->unit.popByYear;
        for (auto it = ch.begin(); it != ch.end(); ++it) {
            me[it->first].first += it->second.first;
            me[it->first].second += it->second.second;
        }
    }
}

// Print summary for one unit
static void printSummary(const TerritorialUnit& u) {
    std::cout << "\n[Summary] " << u.type << " " << u.name
        << " (" << u.code << ")\n";
    for (size_t i = 0; i < YEARS.size(); ++i) {
        const std::string& yr = YEARS[i];
        int m = 0, f = 0;
        auto it = u.popByYear.find(yr);
        if (it != u.popByYear.end()) {
            m = it->second.first;
            f = it->second.second;
        }
        std::cout << " " << yr
            << ": Male=" << m
            << ", Female=" << f
            << ", Total=" << (m + f)
            << "\n";
    }
}

// Simple navigator over the hierarchy
static void navigate(HierarchyNode* root) {
    HierarchyNode* cur = root;
    int opt;
    do {
        std::cout << "\n[Navigate] " << cur->unit.name
            << " (" << cur->unit.code << ")"
            << " [" << cur->unit.type << "]\n"
            << " 1) List Children\n"
            << " 2) Move to Child\n"
            << " 3) Move to Parent\n"
            << " 4) Show Summary\n"
            << " 0) Back to Main Menu\n"
            << "Choice: ";
        std::cin >> opt;

        if (opt == 1) {
            for (size_t i = 0; i < cur->children.size(); ++i) {
                std::cout << "  [" << i << "] "
                    << cur->children[i]->unit.name << "\n";
            }
        }
        else if (opt == 2) {
            size_t idx;
            std::cout << "Child Index: ";
            std::cin >> idx;
            if (idx < cur->children.size()) {
                cur = cur->children[idx];
            }
            else {
                std::cout << "Invalid index\n";
            }
        }
        else if (opt == 3) {
            if (cur->parent) {
                cur = cur->parent;
            }
            else {
                std::cout << "Already at root\n";
            }
        }
        else if (opt == 4) {
            printSummary(cur->unit);
        }
    } while (opt != 0);
}

// === Level 3: build name-&-type search tables ===

static void buildTables(
    HierarchyNode* node,
    HashMap<std::string, Vector<HierarchyNode*>>& countryT,
    HashMap<std::string, Vector<HierarchyNode*>>& geoDivT,
    HashMap<std::string, Vector<HierarchyNode*>>& stateT,
    HashMap<std::string, Vector<HierarchyNode*>>& regionT,
    HashMap<std::string, Vector<HierarchyNode*>>& muniT)
{
    const std::string& t = node->unit.type;
    const std::string& n = node->unit.name;
    if (t == "Country")         countryT[n].push_back(node);
    else if (t == "GeoDiv")     geoDivT[n].push_back(node);
    else if (t == "State")      stateT[n].push_back(node);
    else if (t == "Region")     regionT[n].push_back(node);
    else if (t == "Municipality") muniT[n].push_back(node);

    for (size_t i = 0; i < node->children.size(); ++i) {
        buildTables(node->children[i],
            countryT, geoDivT,
            stateT, regionT,
            muniT);
    }
}

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#ifdef _WIN32
    // Ensure UTF-8 console for diacritics on Windows
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    // 1) Build hierarchy & load populations
    HierarchyNode* root = nullptr;
    try {
        // (1) loadRegions
        root = loadRegions("country.csv");

        // (2) buildLookup
        HashMap<std::string, HierarchyNode*> lookup;
        buildLookup(root, lookup);

        // (3) loadMunicipalities
        loadMunicipalities("municipalities.csv", lookup);

        // (4) loadPopData
        loadPopData(YEARS, lookup);

        // (5) accumulate
        accumulate(root);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        delete root;
        return 1;
    }

    // 2) Build Level 3 search tables
    HashMap<std::string, Vector<HierarchyNode*>> countryTable,
        geoDivTable,
        stateTable,
        regionTable,
        municipalityTable;
    buildTables(root,
        countryTable, geoDivTable,
        stateTable, regionTable,
        municipalityTable);

    // 3) Interactive menu: flat filters, navigate, and type+name search
    Vector<FlatMunicipality> flat;
    int choice;
    do {
        std::cout << "/================ MENU ================/\n"
            << "/   [1] Filter by Name substring       /\n"
            << "/   [2] Filter by Max population       /\n"
            << "/   [3] Filter by Min population       /\n"
            << "/   [4] Hierarchy: Navigate            /\n"
            << "/   [5] Search by Type and Name        /\n"
            << "/   [0] Exit                           /\n"
            << "/======================================/\n"
            << "Choose: ";
        std::cin >> choice;

        if (choice >= 1 && choice <= 3) {
            flat.clear();
            std::string year;
            std::cout << "Enter year (2020–2024): ";
            std::cin >> year;
            loadFlat(year + ".csv", flat);

            if (choice == 1) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter substring: ";
                std::string sub;
                std::getline(std::cin, sub);
                for (size_t i = 0; i < sub.size(); ++i) {
                    sub[i] = std::tolower(static_cast<unsigned char>(sub[i]));
                }

                auto res = filter(flat, [&](const FlatMunicipality& m) {
                    std::string low = m.name;
                    for (size_t j = 0; j < low.size(); ++j) {
                        low[j] = std::tolower(static_cast<unsigned char>(low[j]));
                    }
                    return low.find(sub) != std::string::npos;
                    });

                if (res.size() == 0) {
                    std::cout << "No matches.\n";
                }
                else {
                    for (size_t i = 0; i < res.size(); ++i) {
                        printFlat(res[i]);
                    }
                }
            }
            else {
                std::cout << "Enter the number of people: ";
                int thr;
                std::cin >> thr;
                auto res = filter(flat, [&](const FlatMunicipality& m) {
                    int tot = m.male + m.female;
                    return (choice == 2) ? (tot <= thr) : (tot >= thr);
                    });
                if (res.size() == 0) {
                    std::cout << "No matches.\n";
                }
                else {
                    for (size_t i = 0; i < res.size(); ++i) {
                        printFlat(res[i]);
                    }
                }
            }
        }
        else if (choice == 4) {
            navigate(root);
        }
        else if (choice == 5) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Enter type (Country, GeoDiv, State, Region, Municipality): ";
            std::string type;
            std::getline(std::cin, type);

            std::cout << "Enter exact name: ";
            std::string name;
            std::getline(std::cin, name);

            HashMap<std::string, Vector<HierarchyNode*>>* table = nullptr;
            if (type == "Country")         table = &countryTable;
            else if (type == "GeoDiv")     table = &geoDivTable;
            else if (type == "State")      table = &stateTable;
            else if (type == "Region")     table = &regionTable;
            else if (type == "Municipality") table = &municipalityTable;

            if (!table) {
                std::cout << "Invalid type.\n";
            }
            else {
                Vector<HierarchyNode*>* vecPtr = table->find(name);
                if (vecPtr == nullptr) {
                    std::cout << "No " << type
                        << " named \"" << name << "\".\n";
                }
                else {
                    Vector<HierarchyNode*>& vec = *vecPtr;
                    for (size_t i = 0; i < vec.size(); ++i) {
                        HierarchyNode* ptr = vec[i];
                        std::cout << "\n[Search Result] "
                            << ptr->unit.type << " "
                            << ptr->unit.name << " ("
                            << ptr->unit.code << ")\n";
                        printSummary(ptr->unit);
                    }
                }
            }
        }
    } while (choice != 0);

    // 4) Clean up entire tree
    delete root;

    return 0;
}