// SP_RodrigoLourenço_Level4.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <stdexcept>
#include <limits>
#include <functional>
#include <locale>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>      // for SetConsoleCP / SetConsoleOutputCP
#endif

#include "Vector.h"        // your lab dynamic array
#include "Map.h"           // your lab associative map
#include "HashMap.h"       // your lab hash table

// === Level 1 flat data ===

struct FlatMunicipality {
    std::string name, code;
    int male = 0, female = 0;
};

static std::string cleanCode(const std::string& s) {
    std::string out;
    for (unsigned char c : s)
        if (std::isalnum(c)) out.push_back(c);
    return out;
}

static void loadFlat(const std::string& fn, Vector<FlatMunicipality>& out) {
    std::ifstream f(fn);
    if (!f) return;
    std::string line;
    std::getline(f, line);  // skip header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        FlatMunicipality m;
        std::string tmp;
        std::getline(ss, m.name, ';');
        std::getline(ss, tmp, ';'); m.code = cleanCode(tmp);
        std::getline(ss, tmp, ';'); m.male = std::stoi(tmp);
        std::getline(ss, tmp, ';');  // skip
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
        if (pred(data[i]))
            result.push_back(data[i]);
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

// === Level 2/3 hierarchy ===

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
    if (c == "AT") return "Country";
    if (c.rfind("AT", 0) == 0) {
        size_t L = c.size() - 2;
        if (L == 1) return "GeoDiv";
        if (L == 2) return "State";
        if (L == 3) return "Region";
    }
    return "Municipality";
}

static void buildLookup(HierarchyNode* node,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    lookup[node->unit.code] = node;
    for (size_t i = 0; i < node->children.size(); ++i)
        buildLookup(node->children[i], lookup);
}

static HierarchyNode* loadRegions(const std::string& fn) {
    std::ifstream f(fn);
    if (!f) throw std::runtime_error("Cannot open " + fn);
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
    HashMap<std::string, HierarchyNode*> temp;
    temp["AT"] = root;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        temp[e.second] = new HierarchyNode({ e.first, e.second, determineType(e.second) });
    }
    for (size_t i = 0; i < entries.size(); ++i) {
        std::string c = entries[i].second;
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
        HierarchyNode* node = new HierarchyNode({ nm, code, "Municipality" });
        auto it = lookup.find(region);
        if (it != lookup.end()) {
            node->parent = it->second;
            it->second->children.push_back(node);
            lookup[code] = node;
        }
        else {
            delete node;
        }
    }
}

static void loadPopData(const Vector<std::string>& yrs,
    HashMap<std::string, HierarchyNode*>& lookup)
{
    for (size_t yi = 0; yi < yrs.size(); ++yi) {
        const std::string yr = yrs[yi];
        std::ifstream f(yr + ".csv");
        if (!f) continue;
        std::string line;
        std::getline(f, line);
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
                auto& entry = it->second->unit.popByYear[yr];
                entry.first += m;
                entry.second += fe;
            }
        }
    }
}

static void accumulate(HierarchyNode* n) {
    for (size_t i = 0; i < n->children.size(); ++i) {
        HierarchyNode* ch = n->children[i];
        accumulate(ch);
        for (auto it = ch->unit.popByYear.begin(); it != ch->unit.popByYear.end(); ++it) {
            auto& p = n->unit.popByYear[it->first];
            p.first += it->second.first;
            p.second += it->second.second;
        }
    }
}

static void printSummary(const TerritorialUnit& u) {
    std::cout << "\n[Summary] " << u.type << " " << u.name
        << " (" << u.code << ")\n";
    for (size_t i = 0; i < YEARS.size(); ++i) {
        const std::string yr = YEARS[i];
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

// === Level 3: name/type tables ===

static void buildTables(
    HierarchyNode* node,
    HashMap<std::string, Vector<HierarchyNode*>>& ct,
    HashMap<std::string, Vector<HierarchyNode*>>& gv,
    HashMap<std::string, Vector<HierarchyNode*>>& st,
    HashMap<std::string, Vector<HierarchyNode*>>& rg,
    HashMap<std::string, Vector<HierarchyNode*>>& mn)
{
    const std::string& t = node->unit.type;
    const std::string& n = node->unit.name;
    if (t == "Country")      ct[n].push_back(node);
    else if (t == "GeoDiv")       gv[n].push_back(node);
    else if (t == "State")        st[n].push_back(node);
    else if (t == "Region")       rg[n].push_back(node);
    else if (t == "Municipality") mn[n].push_back(node);

    for (size_t i = 0; i < node->children.size(); ++i)
        buildTables(node->children[i], ct, gv, st, rg, mn);
}

// === Level 4: flatten + sort ===

static void flatten(HierarchyNode* node, Vector<TerritorialUnit>& out) {
    out.push_back(node->unit);
    for (size_t i = 0; i < node->children.size(); ++i)
        flatten(node->children[i], out);
}

static void sortData(Vector<TerritorialUnit>& data,
    std::function<int(const TerritorialUnit&, const TerritorialUnit&)> cmp)
{
    for (size_t i = 1; i < data.size(); ++i) {
        TerritorialUnit key = data[i];
        size_t j = i;
        while (j > 0 && cmp(key, data[j - 1]) < 0) {
            data[j] = data[j - 1];
            --j;
        }
        data[j] = key;
    }
}

static HierarchyNode* selectNode(HierarchyNode* root) {
    HierarchyNode* cur = root;
    int opt;
    do {
        std::cout << "\n[Select] " << cur->unit.name
            << " (" << cur->unit.code << ") [" << cur->unit.type << "]\n"
            << " 1) List Children\n"
            << " 2) Move to Child\n"
            << " 3) Move to Parent\n"
            << " 0) Done\n"
            << "Choice: ";
        std::cin >> opt;
        if (opt == 1) {
            for (size_t i = 0; i < cur->children.size(); ++i)
                std::cout << "  [" << i << "] " << cur->children[i]->unit.name << "\n";
        }
        else if (opt == 2) {
            size_t i; std::cout << " Index: "; std::cin >> i;
            if (i < cur->children.size()) cur = cur->children[i];
        }
        else if (opt == 3) {
            if (cur->parent) cur = cur->parent;
        }
    } while (opt != 0);
    return cur;
}

static void navigate(HierarchyNode* root) {
    HierarchyNode* cur = root;
    int opt;
    do {
        std::cout << "\n[Navigate] " << cur->unit.name
            << " (" << cur->unit.code << ") [" << cur->unit.type << "]\n"
            << " 1) List Children\n"
            << " 2) Move to Child\n"
            << " 3) Move to Parent\n"
            << " 4) Show Summary\n"
            << " 0) Back\n"
            << "Choice: ";
        std::cin >> opt;
        if (opt == 1) {
            for (size_t i = 0; i < cur->children.size(); ++i)
                std::cout << "  " << cur->children[i]->unit.name << "\n";
        }
        else if (opt == 2) {
            size_t i; std::cout << "Index: "; std::cin >> i;
            if (i < cur->children.size()) cur = cur->children[i];
        }
        else if (opt == 3) {
            if (cur->parent) cur = cur->parent;
        }
        else if (opt == 4) {
            printSummary(cur->unit);
        }
    } while (opt != 0);
}

// === Main menu ===

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    // Build hierarchy & data
    HierarchyNode* root = nullptr;
    try {
        root = loadRegions("country.csv");
        HashMap<std::string, HierarchyNode*> lookup;
        buildLookup(root, lookup);
        loadMunicipalities("municipalities.csv", lookup);
        loadPopData(YEARS, lookup);
        accumulate(root);
    }
    catch (std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        delete root;
        return 1;
    }

    // Build name/type tables
    HashMap<std::string, Vector<HierarchyNode*>> countryT, geoDivT, stateT, regionT, muniT;
    buildTables(root, countryT, geoDivT, stateT, regionT, muniT);

    int choice;
    do {
        std::cout << "/============= MENU =============/\n"
                  << "/   [1] Name substring           /\n"
                  << "/   [2] Max population           /\n"
                  << "/   [3] Min population           /\n"
                  << "/   [4] Hierarchy: Navigate      /\n"
                  << "/   [5] Search by Type & Name    /\n"
                  << "/   [6] Filter+Sort from Subtree /\n"
                  << "7   [0] Exit                     /\n"
                  << "/================================/\n"
            << "Choose: ";
        std::cin >> choice;

        // Levels 1–3
        if (choice >= 1 && choice <= 3) {
            Vector<FlatMunicipality> flat;
            std::string yr;
            std::cout << "Year (Between 2020 and 2024): ";
            std::cin >> yr;
            loadFlat(yr + ".csv", flat);

            if (choice == 1) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter substring: ";
                std::string sub; std::getline(std::cin, sub);
                for (size_t i = 0; i < sub.size(); ++i) sub[i] = std::tolower((unsigned char)sub[i]);
                auto res = filter(flat, [&](auto& m) {
                    std::string low = m.name;
                    for (size_t j = 0; j < low.size(); ++j) low[j] = std::tolower((unsigned char)low[j]);
                    return low.find(sub) != std::string::npos;
                    });
                if (res.size() == 0) std::cout << "No matches.\n";
                else for (size_t i = 0; i < res.size(); ++i) printFlat(res[i]);
            }
            else {
                std::cout << "Number of People: "; int thr; std::cin >> thr;
                auto res = filter(flat, [&](auto& m) {
                    int tot = m.male + m.female;
                    return choice == 2 ? (tot <= thr) : (tot >= thr);
                    });
                if (res.size() == 0) std::cout << "No matches.\n";
                else for (size_t i = 0; i < res.size(); ++i) printFlat(res[i]);
            }
        }
        else if (choice == 4) {
            navigate(root);
        }
        else if (choice == 5) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Type (Country/GeoDiv/State/Region/Municipality): ";
            std::string tp; std::getline(std::cin, tp);
            std::cout << "Name: ";
            std::string nm; std::getline(std::cin, nm);
            HashMap<std::string, Vector<HierarchyNode*>>* tbl =
                tp == "Country" ? &countryT :
                tp == "GeoDiv" ? &geoDivT :
                tp == "State" ? &stateT :
                tp == "Region" ? &regionT :
                &muniT;
            auto it = tbl->find(nm);
            if (it == tbl->end()) {
                std::cout << "No " << tp << " named '" << nm << "'.\n";
            }
            else {
                for (size_t i = 0; i < it->second.size(); ++i)
                    printSummary(it->second[i]->unit);
            }
        }
        // Level 4
        else if (choice == 6) {
            HierarchyNode* sel = selectNode(root);
            Vector<TerritorialUnit> items;
            flatten(sel, items);
            std::cout << "Predicate:\n1) Substring\n2) Max pop\n3) Min pop\nChoose: ";
            int p; std::cin >> p;
            std::function<bool(const TerritorialUnit&)> pred;
            std::string sub, yr;
            int thr;
            if (p == 1) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Substring: "; std::getline(std::cin, sub);
                for (size_t i = 0; i < sub.size(); ++i) sub[i] = std::tolower((unsigned char)sub[i]);
                pred = [&](auto& u) {
                    std::string low = u.name;
                    for (size_t j = 0; j < low.size(); ++j) low[j] = std::tolower((unsigned char)low[j]);
                    return low.find(sub) != std::string::npos;
                    };
            }
            else {
                std::cout << "Year (Between 2020 and 2024): "; std::cin >> yr;
                std::cout << "Number of People: "; std::cin >> thr;
                pred = [&](auto& u) {
                    auto it = u.popByYear.find(yr);
                    int m = 0, f = 0;
                    if (it != u.popByYear.end()) { m = it->second.first; f = it->second.second; }
                    int tot = m + f;
                    return p == 2 ? (tot <= thr) : (tot >= thr);
                    };
            }
            auto filtered = filter(items, pred);
            if (filtered.size() == 0) {
                std::cout << "No matches.\n";
                continue;
            }
            std::cout << "Comparator:\n1) Alphabet\n2) Population\nChoose: "; int c; std::cin >> c;
            std::function<int(const TerritorialUnit&, const TerritorialUnit&)> cmp;
            std::string sex;
            if (c == 1) {
#ifdef _WIN32
                std::locale loc("Slovak_Slovakia.1250");
#else
                std::locale loc("sk_SK.UTF-8");
#endif
                // capture locale by value to extend lifetime
                cmp = [loc](auto& a, auto& b) {
                    auto const& coll = std::use_facet<std::collate<char>>(loc);
                    int r = coll.compare(
                        a.name.data(), a.name.data() + a.name.size(),
                        b.name.data(), b.name.data() + b.name.size());
                    return (r < 0 ? -1 : (r > 0 ? 1 : 0));
                    };
            }
            else {
                std::cout << "Year (Between 2020 and 2024): "; std::cin >> yr;
                std::cout << "Sex (male/female/total): "; std::cin >> sex;
                for (size_t i = 0; i < sex.size(); ++i) sex[i] = std::tolower((unsigned char)sex[i]);
                cmp = [&](auto& a, auto& b) {
                    auto ia = a.popByYear.find(yr), ib = b.popByYear.find(yr);
                    int am = 0, af = 0, bm = 0, bf = 0;
                    if (ia != a.popByYear.end()) { am = ia->second.first; af = ia->second.second; }
                    if (ib != b.popByYear.end()) { bm = ib->second.first; bf = ib->second.second; }
                    int va = (sex == "male" ? am : (sex == "female" ? af : (am + af)));
                    int vb = (sex == "male" ? bm : (sex == "female" ? bf : (bm + bf)));
                    return (va < vb ? -1 : (va > vb ? 1 : 0));
                    };
            }
            sortData(filtered, cmp);
            std::cout << "\n[Results]\n";
            for (size_t i = 0; i < filtered.size(); ++i) {
                auto& u = filtered[i];
                std::cout << u.name << " (" << u.code << ")";
                if (c == 2) {
                    auto it = u.popByYear.find(yr);
                    int m = 0, f = 0;
                    if (it != u.popByYear.end()) { m = it->second.first; f = it->second.second; }
                    int val = (sex == "male" ? m : (sex == "female" ? f : (m + f)));
                    std::cout << ": " << yr << "-" << sex << "=" << val;
                }
                std::cout << "\n";
            }
        }
    } while (choice != 0);

    delete root;
    return 0;
}
