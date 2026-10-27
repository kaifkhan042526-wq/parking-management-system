// Parking Management System - console based, C++17
// Compile: g++ -std=c++17 -O2 -o parking parking_system.cpp
// Run:     ./parking

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <limits>
#include <algorithm>

using namespace std;

enum class VehicleType { BIKE = 1, CAR = 2, TRUCK = 3 };

string typeName(VehicleType t) {
    switch (t) {
        case VehicleType::BIKE:  return "Bike";
        case VehicleType::CAR:   return "Car";
        case VehicleType::TRUCK: return "Truck";
    }
    return "Unknown";
}

double hourlyRate(VehicleType t) {
    switch (t) {
        case VehicleType::BIKE:  return 10.0;
        case VehicleType::CAR:   return 20.0;
        case VehicleType::TRUCK: return 40.0;
    }
    return 0.0;
}

string formatTime(time_t t) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buf;
}

string toUpper(string s) {
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

struct Slot {
    int id;
    int floor;
    VehicleType type;   // slot size/type
    bool occupied = false;
    string plate;
};

struct Ticket {
    int id;
    string plate;
    VehicleType type;
    int slotId;
    time_t entry;
};

class ParkingLot {
    vector<Slot> slots;
    map<string, Ticket> active;   // plate -> ticket
    int nextTicketId = 1;
    double revenue = 0.0;
    int totalServed = 0;
    const string dataFile = "parking_data.txt";

public:
    ParkingLot(int floors, int bikePerFloor, int carPerFloor, int truckPerFloor) {
        int id = 1;
        for (int f = 1; f <= floors; ++f) {
            for (int i = 0; i < bikePerFloor;  ++i) slots.push_back({id++, f, VehicleType::BIKE});
            for (int i = 0; i < carPerFloor;   ++i) slots.push_back({id++, f, VehicleType::CAR});
            for (int i = 0; i < truckPerFloor; ++i) slots.push_back({id++, f, VehicleType::TRUCK});
        }
    }

    // A vehicle takes the first free slot of its own type.
    Slot* findFreeSlot(VehicleType t) {
        for (auto& s : slots)
            if (!s.occupied && s.type == t) return &s;
        return nullptr;
    }

    bool parkVehicle(const string& rawPlate, VehicleType t) {
        string plate = toUpper(rawPlate);
        if (active.count(plate)) {
            cout << "  [!] Vehicle " << plate << " is already parked.\n";
            return false;
        }
        Slot* s = findFreeSlot(t);
        if (!s) {
            cout << "  [!] Sorry, no free " << typeName(t) << " slots.\n";
            return false;
        }
        s->occupied = true;
        s->plate = plate;
        Ticket tk{nextTicketId++, plate, t, s->id, time(nullptr)};
        active[plate] = tk;

        cout << "\n  ===== PARKING TICKET =====\n"
             << "  Ticket ID : " << tk.id << "\n"
             << "  Plate     : " << plate << "\n"
             << "  Type      : " << typeName(t) << "\n"
             << "  Slot      : #" << s->id << " (Floor " << s->floor << ")\n"
             << "  Entry     : " << formatTime(tk.entry) << "\n"
             << "  Rate      : Rs " << hourlyRate(t) << "/hour\n"
             << "  ==========================\n";
        return true;
    }

    // Fee = rate * hours, rounded up, minimum 1 hour.
    static double calculateFee(const Ticket& tk, time_t exitTime) {
        double secs = difftime(exitTime, tk.entry);
        int hours = max(1, (int)ceil(secs / 3600.0));
        return hours * hourlyRate(tk.type);
    }

    bool removeVehicle(const string& rawPlate) {
        string plate = toUpper(rawPlate);
        auto it = active.find(plate);
        if (it == active.end()) {
            cout << "  [!] No parked vehicle with plate " << plate << ".\n";
            return false;
        }
        Ticket tk = it->second;
        time_t now = time(nullptr);
        double fee = calculateFee(tk, now);
        int hours = max(1, (int)ceil(difftime(now, tk.entry) / 3600.0));

        for (auto& s : slots)
            if (s.id == tk.slotId) { s.occupied = false; s.plate.clear(); break; }
        active.erase(it);
        revenue += fee;
        ++totalServed;

        cout << "\n  ======== RECEIPT ========\n"
             << "  Ticket ID : " << tk.id << "\n"
             << "  Plate     : " << tk.plate << "\n"
             << "  Entry     : " << formatTime(tk.entry) << "\n"
             << "  Exit      : " << formatTime(now) << "\n"
             << "  Billed    : " << hours << " hour(s)\n"
             << "  Amount    : Rs " << fixed << setprecision(2) << fee << "\n"
             << "  =========================\n";
        return true;
    }

    void searchVehicle(const string& rawPlate) const {
        string plate = toUpper(rawPlate);
        auto it = active.find(plate);
        if (it == active.end()) {
            cout << "  Vehicle " << plate << " not found in the lot.\n";
            return;
        }
        const Ticket& tk = it->second;
        int floor = 0;
        for (auto& s : slots) if (s.id == tk.slotId) floor = s.floor;
        cout << "  Found: " << plate << " (" << typeName(tk.type) << ") in slot #"
             << tk.slotId << ", Floor " << floor
             << ", parked since " << formatTime(tk.entry) << "\n";
    }

    void showStatus() const {
        cout << "\n  --- Slot Availability ---\n";
        for (VehicleType t : {VehicleType::BIKE, VehicleType::CAR, VehicleType::TRUCK}) {
            int total = 0, used = 0;
            for (auto& s : slots) if (s.type == t) { ++total; if (s.occupied) ++used; }
            cout << "  " << left << setw(6) << typeName(t) << ": "
                 << used << "/" << total << " occupied, " << (total - used) << " free\n";
        }
        cout << "\n  --- Slot Map ('.' free, 'X' occupied) ---\n";
        int curFloor = 0;
        for (auto& s : slots) {
            if (s.floor != curFloor) {
                curFloor = s.floor;
                cout << "\n  Floor " << curFloor << ":\n    ";
            }
            char c = (s.type == VehicleType::BIKE) ? 'B' : (s.type == VehicleType::CAR ? 'C' : 'T');
            cout << c << (s.occupied ? 'X' : '.') << ' ';
        }
        cout << "\n  (B=Bike, C=Car, T=Truck)\n";
    }

    void listParked() const {
        if (active.empty()) { cout << "  The lot is empty.\n"; return; }
        cout << "\n  " << left << setw(8) << "Ticket" << setw(14) << "Plate"
             << setw(8) << "Type" << setw(6) << "Slot" << "Entry time\n";
        cout << "  " << string(56, '-') << "\n";
        for (auto& [plate, tk] : active)
            cout << "  " << left << setw(8) << tk.id << setw(14) << plate
                 << setw(8) << typeName(tk.type) << setw(6) << tk.slotId
                 << formatTime(tk.entry) << "\n";
    }

    void showReport() const {
        cout << "\n  --- Revenue Report ---\n"
             << "  Vehicles currently parked : " << active.size() << "\n"
             << "  Vehicles served (exited)  : " << totalServed << "\n"
             << "  Total revenue             : Rs " << fixed << setprecision(2) << revenue << "\n";
    }

    // ---- Persistence ----
    void save() const {
        ofstream out(dataFile);
        if (!out) { cout << "  [!] Could not save data.\n"; return; }
        out << nextTicketId << ' ' << revenue << ' ' << totalServed << '\n';
        for (auto& [plate, tk] : active)
            out << tk.id << ' ' << tk.plate << ' ' << (int)tk.type << ' '
                << tk.slotId << ' ' << (long long)tk.entry << '\n';
    }

    void load() {
        ifstream in(dataFile);
        if (!in) return;
        in >> nextTicketId >> revenue >> totalServed;
        Ticket tk;
        int type; long long entry;
        while (in >> tk.id >> tk.plate >> type >> tk.slotId >> entry) {
            tk.type = (VehicleType)type;
            tk.entry = (time_t)entry;
            for (auto& s : slots)
                if (s.id == tk.slotId && s.type == tk.type) {
                    s.occupied = true; s.plate = tk.plate; break;
                }
            active[tk.plate] = tk;
        }
        cout << "  Loaded previous session: " << active.size() << " vehicle(s) parked.\n";
    }
};

// ---- Input helpers ----
int readInt(const string& prompt, int lo, int hi) {
    int v;
    while (true) {
        cout << prompt;
        if (cin >> v && v >= lo && v <= hi) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return v;
        }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  Invalid input. Enter a number between " << lo << " and " << hi << ".\n";
    }
}

string readPlate() {
    string p;
    while (true) {
        cout << "  Enter plate number: ";
        getline(cin, p);
        p.erase(remove(p.begin(), p.end(), ' '), p.end());
        if (!p.empty()) return p;
        cout << "  Plate cannot be empty.\n";
    }
}

int main() {
    // 3 floors; each floor: 5 bike, 8 car, 2 truck slots
    ParkingLot lot(3, 5, 8, 2);
    cout << "=====================================\n"
         << "     PARKING MANAGEMENT SYSTEM\n"
         << "=====================================\n";
    lot.load();

    while (true) {
        cout << "\n  1. Park a vehicle\n"
             << "  2. Remove a vehicle (checkout)\n"
             << "  3. Search a vehicle\n"
             << "  4. Show slot status\n"
             << "  5. List parked vehicles\n"
             << "  6. Revenue report\n"
             << "  0. Save & exit\n";
        int choice = readInt("  Choose: ", 0, 6);

        switch (choice) {
            case 1: {
                string plate = readPlate();
                int t = readInt("  Type (1=Bike, 2=Car, 3=Truck): ", 1, 3);
                lot.parkVehicle(plate, (VehicleType)t);
                lot.save();
                break;
            }
            case 2:
                lot.removeVehicle(readPlate());
                lot.save();
                break;
            case 3: lot.searchVehicle(readPlate()); break;
            case 4: lot.showStatus(); break;
            case 5: lot.listParked(); break;
            case 6: lot.showReport(); break;
            case 0:
                lot.save();
                cout << "  Data saved. Goodbye!\n";
                return 0;
        }
    }
}
