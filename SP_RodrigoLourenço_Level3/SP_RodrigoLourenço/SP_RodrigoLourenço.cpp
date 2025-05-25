// SP_RodrigoLourenço_Level3.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <limits>          // for std::numeric_limits

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>      // for SetConsoleCP / SetConsoleOutputCP
#endif

#include "Vector.h"        // lab dynamic array
#include "Map.h"           // lab associative map
#include "HashMap.h"       // lab hash table

// Years of data
static const Vector<std::string> YEARS = []() {
    Vector<std::string> v;
    v.push_back("2020");
    v.push_back("2021");
    v.push_back("2022");
    v.push_back("2023");
    v.push_back("2024");
    return v;
    }();

// Utility: strip non-alphanumeric from codes
static std::string cleanCode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back(c);
    }
    return out;
}

// === Level 1 flat data ===
struct FlatMunicipality {
    std::string name, code;
    int male = 0, female = 0;
};

// Load one year’s CSV into flat vector
static void loadFlat(const std::string& fn, Vector<FlatMunicipality>& out) {
    std::ifstream f(fn);
    if (!f) return;
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
        std::getline(ss, tmp, ';'); // skip
        std::getline(ss, tmp, ';'); m.female = std::stoi(tmp);
        out.push_back(m);
    }
}

// Print one flat-data record
static void printFlat(const FlatMunicipality& m) {
    std::cout << "Name: " << m.name
        << ", Code: " << m.code
        << ", Male=" << m.male
        << ", Female=" << m.female
        << ", Total=" << (m.male + m.female)
        << "\n";
}

// Generic filter algorithm (from Level 1)
template<typename T, typename Pred>
Vector<T> filter(const Vector<T>& data, Pred pred) {
    Vector<T> result;
    for (size_t i = 0; i < data.size(); ++i) {
        if (pred(data[i])) {
            result.push_back(data[i]);
        }
    }
    return result;
}

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
        for (size_t i = 0; i < children.size(); ++i)
            delete children[i];
    }
};

static std::string determineType(const std::string& c) {
    if (c == "AT")                return "Country";
    if (c.rfind("AT", 0) == 0) {
        size_t L = c.size() - 2;
        if (L == 1) return "GeoDiv";
        if (L == 2) return "State";
        if (L == 3) return "Region";
    }
    return "Municipality";
}

// Build lookup code → node
static void buildLookup(HierarchyNode* node,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    lookup[node->unit.code] = node;
    for (size_t i = 0; i < node->children.size(); ++i)
        buildLookup(node->children[i], lookup);
}

// Load GeoDiv/State/Region from country.csv
static HierarchyNode* loadRegions(const std::string& fn) {
    std::ifstream f(fn);
    if (!f) throw std::runtime_error("Cannot open " + fn);

    // Root = Austria
    HierarchyNode* root = new HierarchyNode({ "Austria","AT","Country" });

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

    // Temporary code→node map
    HashMap<std::string, HierarchyNode*> temp;
    temp["AT"] = root;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        temp[e.second] = new HierarchyNode({
            e.first,
            e.second,
            determineType(e.second)
            });
    }
    // Link parent ↔ child
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        std::string c = e.second;
        std::string p = (c.size() > 2 ? c.substr(0, c.size() - 1) : "AT");
        auto it = temp.find(p);
        if (it != temp.end()) {
            HierarchyNode* par = it->second;
            HierarchyNode* ch = temp[c];
            ch->parent = par;
            par->children.push_back(ch);
        }
    }
    return root;
}

// Load municipalities → regions
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

        HierarchyNode* node = new HierarchyNode({
            nm, code, "Municipality"
            });
        auto it = lookup.find(region);
        if (it != lookup.end()) {
            node->parent = it->second;
            it->second->children.push_back(node);
            lookup[code] = node;
        }
        else {
            delete node;  // orphan
        }
    }
}

// Load population per code & year
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

            int m = std::stoi(mstr);
            int fe = std::stoi(fstr);
            std::string code = cleanCode(cr);

            auto it = lookup.find(code);
            if (it != lookup.end()) {
                auto& entry = it->second->unit.popByYear[yrs[yi]];
                entry.first += m;
                entry.second += fe;
            }
        }
    }
}

// Accumulate child → parent populations
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

// Print cumulative summary
static void printSummary(const TerritorialUnit& u) {
    std::cout << "\n[Summary] " << u.type << " " << u.name
        << " (" << u.code << ")\n";
    int tm = 0, tf = 0;
    for (size_t i = 0; i < YEARS.size(); ++i) {
        auto yr = YEARS[i];
        auto it = u.popByYear.find(yr);
        int m = 0, f = 0;
        if (it != u.popByYear.end()) {
            m = it->second.first;
            f = it->second.second;
        }
        std::cout << " " << yr
            << ": Male=" << m
            << ", Female=" << f
            << ", Total=" << (m + f) << "\n";
        tm += m; tf += f;
    }
    std::cout << " Total (all years): Male=" << tm
        << ", Female=" << tf
        << ", Total=" << (tm + tf) << "\n\n";
}

// Simple navigator (unchanged)
static void navigate(HierarchyNode* root) {
    HierarchyNode* cur = root;
    int opt;
    do {
        std::cout << "\n[Navigate] " << cur->unit.name
            << " (" << cur->unit.code << ")"
            << " [" << cur->unit.type << "]\n"
            << "1) List Children\n"
            << "2) Move to Child\n"
            << "3) Move to Parent\n"
            << "4) Show Summary\n"
            << "0) Back\n"
            << "Choose: ";
        std::cin >> opt;
        if (opt == 1) {
            for (size_t i = 0; i < cur->children.size(); ++i) {
                std::cout << " [" << i << "] "
                    << cur->children[i]->unit.name << "\n";
            }
        }
        else if (opt == 2) {
            size_t idx;
            std::cout << "Child Index: "; std::cin >> idx;
            if (idx < cur->children.size())
                cur = cur->children[idx];
            else
                std::cout << "Invalid index\n";
        }
        else if (opt == 3) {
            if (cur->parent)
                cur = cur->parent;
            else
                std::cout << "Already at root\n";
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
    if (t == "Country")      countryT[node->unit.name].push_back(node);
    else if (t == "GeoDiv")       geoDivT[node->unit.name].push_back(node);
    else if (t == "State")        stateT[node->unit.name].push_back(node);
    else if (t == "Region")       regionT[node->unit.name].push_back(node);
    else if (t == "Municipality") muniT[node->unit.name].push_back(node);

    for (size_t i = 0; i < node->children.size(); ++i) {
        buildTables(node->children[i],
            countryT, geoDivT,
            stateT, regionT,
            muniT);
    }
}

int main() {
#ifdef _WIN32
    // ensure UTF-8 console for diacritics
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    // 1) build hierarchy & load populations
    HierarchyNode* root = nullptr;
    try {
        root = loadRegions("country.csv");
        HashMap<std::string, HierarchyNode*> lookup;
        buildLookup(root, lookup);
        loadMunicipalities("municipalities.csv", lookup);
        loadPopData(YEARS, lookup);
        accumulate(root);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        delete root;
        return 1;
    }

    // 2) build Level 3 search tables
    HashMap<std::string, Vector<HierarchyNode*>> countryTable,
        geoDivTable,
        stateTable,
        regionTable,
        municipalityTable;
    buildTables(root,
        countryTable, geoDivTable,
        stateTable, regionTable,
        municipalityTable);

    // 3) interactive menu: filters, navigation, plus new type/name search
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
                for (size_t i = 0; i < sub.size(); ++i)
                    sub[i] = std::tolower(static_cast<unsigned char>(sub[i]));

                auto res = filter(flat, [&](auto& m) {
                    std::string low = m.name;
                    for (size_t j = 0; j < low.size(); ++j)
                        low[j] = std::tolower(static_cast<unsigned char>(low[j]));
                    return low.find(sub) != std::string::npos;
                    });

                if (res.size() == 0)
                    std::cout << "No matches.\n";
                else
                    for (size_t i = 0; i < res.size(); ++i)
                        printFlat(res[i]);
            }
            else {
                std::cout << "Enter the number of population: ";
                int thr;
                std::cin >> thr;
                auto res = filter(flat, [&](auto& m) {
                    int tot = m.male + m.female;
                    return choice == 2 ? (tot <= thr) : (tot >= thr);
                    });
                if (res.size() == 0)
                    std::cout << "No matches.\n";
                else
                    for (size_t i = 0; i < res.size(); ++i)
                        printFlat(res[i]);
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

            // select the correct table by pointer
            HashMap<std::string, Vector<HierarchyNode*>>* table = nullptr;
            if (type == "Country")      table = &countryTable;
            else if (type == "GeoDiv")       table = &geoDivTable;
            else if (type == "State")        table = &stateTable;
            else if (type == "Region")       table = &regionTable;
            else if (type == "Municipality") table = &municipalityTable;

            if (!table) {
                std::cout << "Invalid type.\n";
            }
            else {
                auto it = table->find(name);
                if (it == table->end()) {
                    std::cout << "No " << type
                        << " named \"" << name << "\".\n";
                }
                else {
                    // print all matching nodes
                    for (size_t i = 0; i < it->second.size(); ++i) {
                        HierarchyNode* ptr = it->second[i];
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

    delete root;
    return 0;
}