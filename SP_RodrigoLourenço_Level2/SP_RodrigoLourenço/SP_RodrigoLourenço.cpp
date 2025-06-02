// SP_RodrigoLourenço_Level2.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <limits>
#include <locale>
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>


#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>  // for SetConsoleCP / SetConsoleOutputCP
#endif

#include "Vector.h"    // lab dynamic array
#include "Map.h"       // lab associative map (for per‐year pop data)
#include "HashMap.h"   // custom hash table

// === Level 1 flat data ===

struct FlatMunicipality {
    std::string name, code;
    int male = 0, female = 0;
};

// Remove non‐alphanumeric chars from code strings (e.g., “AT 31;” → “AT31”)
static std::string cleanCode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back(c);
    }
    return out;
}

// Load one year’s CSV (semicolon‐separated) into a flat Vector<FlatMunicipality>
static void loadFlat(const std::string& fn, Vector<FlatMunicipality>& out) {
    std::ifstream f(fn);
    if (!f) {
        std::cerr << "  [loadFlat] Could not open " << fn << "\n";
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
    // Map: year → (male, female)
    Map<std::string, std::pair<int, int>> popByYear;
};

struct HierarchyNode {
    TerritorialUnit unit;
    Vector<HierarchyNode*> children;
    HierarchyNode* parent = nullptr;

    HierarchyNode(const TerritorialUnit& u) : unit(u) {}
    ~HierarchyNode() {
        // Recursively delete all children
        for (size_t i = 0; i < children.size(); ++i) {
            delete children[i];
        }
    }
};

// Determine type by code prefix
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

// Build a lookup table mapping code → HierarchyNode*
static void buildLookup(HierarchyNode* node,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    lookup[node->unit.code] = node;
    for (size_t i = 0; i < node->children.size(); ++i) {
        buildLookup(node->children[i], lookup);
    }
}

// Load “country.csv”: each line “Name;Code” (no header assumed).
// Builds nodes for GeoDiv/State/Region and links them under “AT”.
static HierarchyNode* loadRegions(const std::string& fn) {
    std::ifstream f(fn);
    if (!f) throw std::runtime_error("Cannot open " + fn);

    // Create root = Austria
    HierarchyNode* root = new HierarchyNode({ "Austria", "AT", "Country" });

    // Temporary storage of pairs (name, code)
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

    // Build a temp HashMap code → node*
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
        // Parent code = all but last character, or “AT” if length ≤ 2
        std::string p = (c.size() > 2 ? c.substr(0, c.size() - 1) : "AT");
        HierarchyNode* parent = nullptr;
        if (HierarchyNode** ptr = temp.find(p)) {
            parent = *ptr;
        }
        if (parent) {
            HierarchyNode* child = temp[c];
            child->parent = parent;
            parent->children.push_back(child);
        }
    }

    return root;
}

// Load “municipalities.csv”: each line “Name;Code;RegionCode”
// Attach municipalities to their region node if found; or delete them otherwise.
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
        HierarchyNode* parent = nullptr;
        if (HierarchyNode** ptr = lookup.find(region)) {
            parent = *ptr;
        }
        if (parent) {
            node->parent = parent;
            parent->children.push_back(node);
            lookup[code] = node;
        }
        else {
            // Orphan municipality: delete
            delete node;
        }
    }
}

// Load population data from “YYYY.csv” for each year in yrs.
// Each row: “Name;Code;Male;Skip;Female;…”, same format as loadFlat.
// Accumulate male+female into popByYear for node if code matches.
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
            if (HierarchyNode** ptr = lookup.find(code)) {
                HierarchyNode* node = *ptr;
                auto& entry = node->unit.popByYear[yrs[yi]];
                entry.first += male;
                entry.second += female;
            }
        }
    }
}

// Post‐order accumulation: each parent’s popByYear += sum of its children’s popByYear
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

// Print summary of a TerritorialUnit’s populations across YEARS
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

// Simple text‐based navigator over the hierarchy
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

// === Main Program (Level 2) ===

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#ifdef _WIN32
    // Ensure UTF-8 console on Windows for diacritics
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    // 1) Build hierarchy & load populations
    HierarchyNode* root = nullptr;
    try {
        root = loadRegions("country.csv");

        // Build lookup table: code → node*
        HashMap<std::string, HierarchyNode*> lookup;
        buildLookup(root, lookup);

        // Load municipalities and attach
        loadMunicipalities("municipalities.csv", lookup);

        // Load population data for each year
        loadPopData(YEARS, lookup);

        // Accumulate child‐pop into parent‐pop
        accumulate(root);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        delete root;
        return 1;
    }

    // 2) Interactive menu: Level 1 flat filters and hierarchy navigation
    Vector<FlatMunicipality> flat;
    int choice;
    do {
        std::cout << "/================ MENU ================/\n"
            << "/   [1] Filter by Name substring       /\n"
            << "/   [2] Filter by Max population       /\n"
            << "/   [3] Filter by Min population       /\n"
            << "/   [4] Hierarchy: Navigate            /\n"
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
    } while (choice != 0);

    // 3) Clean up entire tree
    delete root;
    return 0;
}