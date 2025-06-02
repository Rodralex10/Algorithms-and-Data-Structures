// SP_RodrigoLourenço_Level4.cpp

#include <iostream>         // For standard I/O streams
#include <fstream>          // For file input
#include <sstream>          // For string streams (parsing lines)
#include <string>           // For std::string
#include <cctype>           // For character classification (std::tolower)
#include <stdexcept>        // For std::runtime_error
#include <limits>           // For std::numeric_limits
#include <functional>       // For std::function
#include <locale>           // For locale and collation
#define _CRTDBG_MAP_ALLOC    // Enable memory leak detection on Windows
#include <cstdlib>          // For general utilities
#include <crtdbg.h>         // For heap debug routines

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX        // Prevent Windows headers from defining min/max macros
#  endif
#  include <Windows.h>      // For SetConsoleCP / SetConsoleOutputCP on Windows
#endif

#include "Vector.h"       
#include "Map.h"          
#include "HashMap.h"       

// === Level 1 flat-data structures & functions ===
struct FlatMunicipality {
    std::string name;     // MName
    std::string code;     // MCode
    int male = 0;         // MaleP
    int female = 0;       // FemaleP
};

// Function to remove all non-alphanumeric characters from a code string
static std::string cleanCode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            out.push_back(c); // Only keep alphanumeric characters
        }
    }
    return out;
}

// Load one year's CSV file into a Vector<FlatMunicipality>
static void loadFlat(const std::string& fn, Vector<FlatMunicipality>& out) {
    std::ifstream f(fn);              // Open file named 'fn'
    if (!f) {
        std::cerr << "[loadFlat] Could not open " << fn << "\n";
        return;                       
    }
    std::string line;
    std::getline(f, line);          
    while (std::getline(f, line)) {
        if (line.empty()) continue;    // Skip empty lines
        std::istringstream ss(line);   
        FlatMunicipality m;
        std::string tmp;
        std::getline(ss, m.name, ';');            // Read MunicipalityN
        std::getline(ss, tmp, ';'); m.code = cleanCode(tmp); // Read raw code and clean it
        std::getline(ss, tmp, ';'); m.male = std::stoi(tmp); // Read MaleCount
        std::getline(ss, tmp, ';');                   // Skip field
        std::getline(ss, tmp, ';'); m.female = std::stoi(tmp); // Read female count
        out.push_back(m);                       // Add entry Vector
    }
}

// Print one FlatMunicipality record to console
static void printFlat(const FlatMunicipality& m) {
    std::cout << "Name: " << m.name
        << ", Code: " << m.code
        << ", Male=" << m.male
        << ", Female=" << m.female
        << ", Total=" << (m.male + m.female)
        << "\n";
}

// Generic filter function for any Vector<T> using a predicate
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
// Structure to hold a territorial unit's data in the hierarchy (name, code, type, population map)
struct TerritorialUnit {
    std::string name;   // Name of the unit (Country/GeoDiv/State/Region/Municipality)
    std::string code;   // Code (AT, AT1)
    std::string type;   // Type: "Country", "GeoDiv", "State", "Region", "Municipality"
    Map<std::string, std::pair<int, int>> popByYear;
};

// Node in the hierarchy tree, holding a TerritorialUnit and pointers to children/parent
struct HierarchyNode {
    TerritorialUnit unit;                // Data for this node
    Vector<HierarchyNode*> children;     // Vector of pointers to child nodes
    HierarchyNode* parent = nullptr;     // Pointer to parent node (nullptr for root)

    HierarchyNode(const TerritorialUnit& u) : unit(u) {}
    ~HierarchyNode() {
        // Recursively delete all children to avoid memory leaks
        for (size_t i = 0; i < children.size(); ++i) {
            delete children[i];
        }
    }
};

// Determine the type of a territorial unit from its code prefix
static std::string determineType(const std::string& c) {
    if (c == "AT")                 return "Country"; // Root code
    if (c.rfind("AT", 0) == 0) {   // Starts with "AT"
        size_t L = c.size() - 2;    
        if (L == 1) return "GeoDiv";  // AT1 → GeoDiv
        if (L == 2) return "State";   // AT11 → State
        if (L == 3) return "Region";  // AT111 → Region
    }
    return "Municipality";         // Otherwise, it's a municipality
}

// Build a lookup table (HashMap) mapping code string to HierarchyNode* for fast access
static void buildLookup(HierarchyNode* node,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    lookup[node->unit.code] = node;            
    for (size_t i = 0; i < node->children.size(); ++i) {
        buildLookup(node->children[i], lookup); 
    }
}

// Load regions (GeoDiv, State, Region) from "country.csv"

static HierarchyNode* loadRegions(const std::string& fn) {
    std::ifstream f(fn);
    if (!f) throw std::runtime_error("Cannot open " + fn);

    // Create root node representing the country Austria
    HierarchyNode* root = new HierarchyNode({ "Austria", "AT", "Country" });

    Vector<std::pair<std::string, std::string>> entries;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;            // Skip empty lines
        std::istringstream ss(line);
        std::string nm, cr;
        std::getline(ss, nm, ';');            // Read name
        std::getline(ss, cr, ';');            // Read code (raw)
        entries.push_back({ nm, cleanCode(cr) });
    }

    // Temporary HashMap: code → newly created HierarchyNode*
    HashMap<std::string, HierarchyNode*> temp;
    temp["AT"] = root;                         // Root entry
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        TerritorialUnit u;
        u.name = e.first;
        u.code = e.second;
        u.type = determineType(e.second);
        temp[e.second] = new HierarchyNode(u);
    }

    // Link each node to its parent based on code: parent code is code without last digit (or "AT" if length ≤ 2)
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        std::string c = e.second;
        std::string p = (c.size() > 2 ? c.substr(0, c.size() - 1) : "AT");
        HierarchyNode** pptr = temp.find(p);
        if (pptr) {
            HierarchyNode* parent = *pptr;
            HierarchyNode* child = temp[c];
            child->parent = parent;              // Set child's parent
            parent->children.push_back(child);   // Add child to parent's children vector
        }
    }

    return root; // Return root of the region hierarchy
}

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
        std::getline(ss, nm, ';');             // Municipality name
        std::getline(ss, cr, ';');             // Municipality code
        std::getline(ss, rr, ';');             // Region code
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
            lookup[code] = node;                // Add municipality to lookup map
        }
        else {
            delete node; // If region not found, drop this municipality
        }
    }
}

// Load population data from "YYYY.csv" for each year listed in 'yrs'
static void loadPopData(const Vector<std::string>& yrs,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    for (size_t yi = 0; yi < yrs.size(); ++yi) {
        std::ifstream f(yrs[yi] + ".csv");
        if (!f) continue;                    // Skip if file not found
        std::string line;
        std::getline(f, line);             // Skip header line
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            std::string nm, cr, mstr, skip, fstr;
            std::getline(ss, nm, ';');      // Name (unused)
            std::getline(ss, cr, ';');      // Code (raw)
            std::getline(ss, mstr, ';');    // MaleCount
            std::getline(ss, skip, ';');    // Skip one field
            std::getline(ss, fstr, ';');    // FemaleCount
            int male = std::stoi(mstr);
            int female = std::stoi(fstr);
            std::string code = cleanCode(cr);
            HierarchyNode** nptr = lookup.find(code);
            if (nptr) {
                HierarchyNode* node = *nptr;
                // Add/populate entry in popByYear map for this node and year
                auto& entry = node->unit.popByYear[yrs[yi]];
                entry.first += male;
                entry.second += female;
            }
        }
    }
}

// Post-order traversal: accumulate each parent's popByYear by summing its children's popByYear
static void accumulate(HierarchyNode* n) {
    for (size_t i = 0; i < n->children.size(); ++i) {
        accumulate(n->children[i]);         // Recursively accumulate children first
        auto& me = n->unit.popByYear;
        auto& ch = n->children[i]->unit.popByYear;
        for (auto it = ch.begin(); it != ch.end(); ++it) {
            // Add child's male and female to parent's counts for each year
            me[it->first].first += it->second.first;
            me[it->first].second += it->second.second;
        }
    }
}

// Print population summary of a TerritorialUnit across YEARS
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

// === Level 3: build name-&-type search tables ===
// Recursively traverse the hierarchy and insert pointers into separate HashMaps by type and name
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

// === Level 4: flatten + sort subtree ===
// Recursively collect every unit under 'node' (including itself) into 'out'
static void flattenSubtree(HierarchyNode* node, Vector<TerritorialUnit>& out) {
    out.push_back(node->unit);                // Add current node's unit
    for (size_t i = 0; i < node->children.size(); ++i) {
        flattenSubtree(node->children[i], out); // Recurse into children
    }
}

// Interactive function for user to navigate hierarchy and select a subtree root
static HierarchyNode* chooseSubtree(HierarchyNode* root) {
    HierarchyNode* cur = root;
    int opt;
    do {
        std::cout << "\n[Subtree navigation] " << cur->unit.name
            << " (" << cur->unit.code << ") [" << cur->unit.type << "]\n"
            << " 1) List Children\n"
            << " 2) Move to Child\n"
            << " 3) Move to Parent\n"
            << " 0) Select this subtree\n"
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
            std::cout << "Child index: ";
            std::cin >> idx;
            if (idx < cur->children.size()) {
                cur = cur->children[idx]; // Move down to selected child
            }
            else {
                std::cout << "Invalid index\n";
            }
        }
        else if (opt == 3) {
            if (cur->parent) {
                cur = cur->parent;      // Move up to parent
            }
            else {
                std::cout << "Already at root\n";
            }
        }
    } while (opt != 0);

    return cur; // Return the chosen subtree root
}

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // Enable heap leak checking
#ifdef _WIN32
    // Ensure UTF-8 console on Windows (for diacritics support)
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    // ==== 1) Build hierarchy & load populations ====
    HierarchyNode* root = nullptr;
    try {
        // (1) Load region hierarchy from "country.csv"
        root = loadRegions("country.csv");

        // (2) Build lookup table: code → HierarchyNode*
        HashMap<std::string, HierarchyNode*> lookup;
        buildLookup(root, lookup);

        // (3) Load municipalities and attach to regions
        loadMunicipalities("municipalities.csv", lookup);

        // (4) Load population data for each year
        loadPopData(YEARS, lookup);

        // (5) Accumulate population counts upward through hierarchy
        accumulate(root);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        delete root;                      // Clean up if error occurs
        return 1;
    }

    // ==== 2) Build Level 3 name/type search tables ====
    HashMap<std::string, Vector<HierarchyNode*>> countryTable,
        geoDivTable,
        stateTable,
        regionTable,
        municipalityTable;
    buildTables(root,
        countryTable, geoDivTable,
        stateTable, regionTable,
        municipalityTable);

    // ===== 3) Main interactive menu (Levels 1–4) =====
    int choice;
    do {
        std::cout << "/================ MENU =================/\n"
            << "/   LVL 1 - [1] Name substring          /\n"
            << "/   LVL 1 - [2] Max population          /\n"
            << "/   LVL 1 - [3] Min population          /\n"
            << "/   LVL 2 - [4] Hierarchy: Navigate     /\n"
            << "/   LVL 3 - [5] Search by Type & Name   /\n"
            << "/   LVL 4 - [6] Filter+Sort from Subtree/\n"
            << "/   [0] Exit                            /\n"
            << "/=======================================/\n"
            << "Choose: ";
        std::cin >> choice;

        // ─── Levels 1–3: flat filters & hierarchy & type/name search ───
        if (choice >= 1 && choice <= 3) {
            Vector<FlatMunicipality> flat;
            std::string yr;
            std::cout << "Year (2020–2024): ";
            std::cin >> yr;
            loadFlat(yr + ".csv", flat);      // Load flat data for specified year

            if (choice == 1) {
                // Filter by name substring
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
                        printFlat(res[i]);          // Print each matching municipality
                    }
                }
            }
            else {
                // Filter by max/min total population
                std::cout << "Enter number of people: ";
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
                        printFlat(res[i]);          // Print each matching municipality
                    }
                }
            }
        }
        else if (choice == 4) {
            // Hierarchy: Navigate through the tree (Level 2 functionality)
            HierarchyNode* cur = root;
            int opt;
            do {
                std::cout << "\n[Navigate] " << cur->unit.name
                    << " (" << cur->unit.code << ")"
                    << " [" << cur->unit.type << "]\n"
                    << " 1) List Children\n"
                    << " 2) Move to Child\n"
                    << " 3) Move to Parent\n"
                    << " 0) Back\n"
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
                    std::cout << "Index: ";
                    std::cin >> idx;
                    if (idx < cur->children.size()) {
                        cur = cur->children[idx]; // Move to child
                    }
                    else {
                        std::cout << "Invalid index\n";
                    }
                }
                else if (opt == 3) {
                    if (cur->parent) {
                        cur = cur->parent;      // Move to parent
                    }
                    else {
                        std::cout << "Already at root\n";
                    }
                }
            } while (opt != 0);
        }
        else if (choice == 5) {
            // Search by Type & Name (Level 3 functionality)
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Type (Country/GeoDiv/State/Region/Municipality): ";
            std::string tp;
            std::getline(std::cin, tp);
            std::cout << "Name: ";
            std::string nm;
            std::getline(std::cin, nm);

            HashMap<std::string, Vector<HierarchyNode*>>* tbl = nullptr;
            if (tp == "Country")         tbl = &countryTable;
            else if (tp == "GeoDiv")     tbl = &geoDivTable;
            else if (tp == "State")      tbl = &stateTable;
            else if (tp == "Region")     tbl = &regionTable;
            else if (tp == "Municipality") tbl = &municipalityTable;

            if (!tbl) {
                std::cout << "Invalid type.\n";
            }
            else {
                Vector<HierarchyNode*>* vecPtr = tbl->find(nm);
                if (vecPtr == nullptr) {
                    std::cout << "No " << tp << " named \"" << nm << "\".\n";
                }
                else {
                    for (size_t i = 0; i < vecPtr->size(); ++i) {
                        HierarchyNode* ptr = (*vecPtr)[i];
                        std::cout << "\n[Search Result] "
                            << ptr->unit.type << " "
                            << ptr->unit.name << " ("
                            << ptr->unit.code << ")\n";
                        printSummary(ptr->unit);   // Show population summary
                    }
                }
            }
        }
        else if (choice == 6) {
            // Filter + Sort from chosen Subtree (Level 4 functionality)
            std::cout << "\n-- Choose subtree: --\n";
            HierarchyNode* subRoot = chooseSubtree(root); // Let user pick a subtree root
            if (!subRoot) continue;

            // 1) Flatten the subtree into a Vector<TerritorialUnit>
            Vector<TerritorialUnit> items;
            flattenSubtree(subRoot, items);

            // 2) Ask the user which filter to apply (name substring, max or min pop)
            std::cout << "Filter by:\n"
                << "  1) Name substring\n"
                << "  2) Max population\n"
                << "  3) Min population\n"
                << "Choice: ";
            int fchoice;
            std::cin >> fchoice;

            // Build filter predicate (std::function<bool(const TerritorialUnit&)>)
            std::function<bool(const TerritorialUnit&)> pred;
            std::string yr;     // Will hold year for pop filters
            std::string sub;    // Substring for name filter
            int thr;            // Threshold for pop filter

            if (fchoice == 1) {
                // Name substring filter
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter substring: ";
                std::getline(std::cin, sub);
                for (size_t i = 0; i < sub.size(); ++i) {
                    sub[i] = std::tolower(static_cast<unsigned char>(sub[i]));
                }
                pred = [&](auto& u) {
                    std::string low = u.name;
                    for (size_t j = 0; j < low.size(); ++j) {
                        low[j] = std::tolower(static_cast<unsigned char>(low[j]));
                    }
                    return low.find(sub) != std::string::npos;
                };
            }
            else if (fchoice == 2 || fchoice == 3) {
                // Max or Min population filter
                std::cout << "Year (2020–2024): ";
                std::cin >> yr;
                std::cout << "Number of people: ";
                std::cin >> thr;
                pred = [&](auto& u) {
                    auto it = u.popByYear.find(yr);
                    int m = 0, f = 0;
                    if (it != u.popByYear.end()) {
                        m = it->second.first;
                        f = it->second.second;
                    }
                    int tot = m + f;
                    // Return true if total ≤ thr (max) or total ≥ thr (min)
                    return (fchoice == 2) ? (tot <= thr) : (tot >= thr);
                };
            }
            else {
                std::cout << "Invalid filter choice.\n";
                continue;
            }

            // Apply filter to flattened items
            Vector<TerritorialUnit> filtered = filter(items, pred);
            if (filtered.size() == 0) {
                std::cout << "No matches.\n";
                continue;
            }

            // 3) Ask how to sort: alphabetically or by population
            std::cout << "Sort by:\n"
                << "  1) Alphabet\n"
                << "  2) Population\n"
                << "Choice: ";
            int sortChoice;
            std::cin >> sortChoice;

            // 'sex' remains in scope for comparator and printing (male/female/total)
            std::string sex;

            // Build sort comparator: std::function<int(const TerritorialUnit&, const TerritorialUnit&)>
            std::function<int(const TerritorialUnit&, const TerritorialUnit&)> cmp;
            if (sortChoice == 1) {
                std::locale loc(""); // Use environment's default locale
                cmp = [&](auto& a, auto& b) {
                    const std::string& sa = a.name;
                    const std::string& sb = b.name;
                    int r = std::use_facet<std::collate<char>>(loc).compare(
                        sa.data(), sa.data() + sa.size(),
                        sb.data(), sb.data() + sb.size());
                    return (r < 0 ? -1 : (r > 0 ? 1 : 0));
                };
            }
            else if (sortChoice == 2) {
                // Sort by population for a given year and sex
                std::cout << "Year (2020–2024): ";
                std::cin >> yr;
                std::cout << "Sex (male/female/total): ";
                std::cin >> sex;
                for (size_t i = 0; i < sex.size(); ++i) {
                    sex[i] = std::tolower(static_cast<unsigned char>(sex[i]));
                }
                cmp = [&](auto& a, auto& b) {
                    auto ia = a.popByYear.find(yr);
                    auto ib = b.popByYear.find(yr);
                    int am = 0, af = 0, bm = 0, bf = 0;
                    if (ia != a.popByYear.end()) { am = ia->second.first; af = ia->second.second; }
                    if (ib != b.popByYear.end()) { bm = ib->second.first; bf = ib->second.second; }
                    int va = (sex == "male" ? am : (sex == "female" ? af : (am + af)));
                    int vb = (sex == "male" ? bm : (sex == "female" ? bf : (bm + bf)));
                    return (va < vb ? -1 : (va > vb ? 1 : 0));
                };
            }
            else {
                std::cout << "Invalid sort choice.\n";
                continue;
            }

            // 4) Sort the filtered list using insertion sort (stable, simple enough for small N)
            for (size_t i = 1; i < filtered.size(); ++i) {
                TerritorialUnit key = filtered[i];
                size_t j = i;
                while (j > 0 && cmp(key, filtered[j - 1]) < 0) {
                    filtered[j] = filtered[j - 1];   // Shift element to the right
                    --j;
                }
                filtered[j] = key;                   // Insert key at correct position
            }

            // 5) Print sorted results to console
            std::cout << "\n[Results]\n";
            for (size_t i = 0; i < filtered.size(); ++i) {
                auto& u = filtered[i];
                std::cout << u.name << " (" << u.code << ")";
                if (sortChoice == 2) {
                    auto it = u.popByYear.find(yr);
                    int m = 0, f = 0;
                    if (it != u.popByYear.end()) {
                        m = it->second.first;
                        f = it->second.second;
                    }
                    int val = (sex == "male" ? m : (sex == "female" ? f : (m + f)));
                    std::cout << ": " << yr << "-" << sex << "=" << val;
                }
                std::cout << "\n";
            }
        }
    } while (choice != 0);

    // ==== 4) Clean up entire tree ====
    delete root; // Recursively deletes all nodes and children

    return 0;
}