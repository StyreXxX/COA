#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <optional>
using namespace std;

// FSM States
enum class State {
    IDLE,
    COMPARE_TAG,
    WRITE_BACK,
    ALLOCATE
};

// Helper function to print the state
string stateToString(State s) {
    switch (s) {
        case State::IDLE: return "IDLE";
        case State::COMPARE_TAG: return "COMPARE_TAG";
        case State::WRITE_BACK: return "WRITE_BACK";
        case State::ALLOCATE: return "ALLOCATE";
        default: return "UNKNOWN";
    }
}

// Memory Module
class Memory {
public:
    int latency;
    int timer;
    bool busy;

    Memory(int latency = 2) : latency(latency), timer(0), busy(false) {}

    bool request() {
        if (!busy) {
            busy = true;
            timer = latency;
            return false; // Not ready yet
        }
        
        timer--;
        if (timer == 0) {
            busy = false;
            return true;  // Ready
        }
        return false;
    }
};

// Cache Block/Line
struct CacheLine {
    bool valid = false;
    bool dirty = false;
    int tag = -1;
};

// CPU Request Structure
struct CPURequest {
    string op;
    int addr;
};

// Cache Controller
class CacheController {
public:
    State state;
    map<int, CacheLine> cache; // 4-block direct mapped cache
    Memory memory;
    
    // Interfacing signals
    bool cpu_ready;
    bool mem_read;
    bool mem_write;

    CacheController() : state(State::IDLE), memory(2), cpu_ready(true), mem_read(false), mem_write(false) {
        for (int i = 0; i < 4; i++) {
            cache[i] = CacheLine();
        }
    }

    void extract_index_tag(int address, int& index, int& tag) {
        index = address % 4; // 4 blocks
        tag = address / 4;
    }

    void tick(optional<CPURequest> cpu_req) {
        cout << "Current State: " << stateToString(state) << endl;
        
        bool req_valid = cpu_req.has_value();
        int index = 0, tag = 0;
        CacheLine* line = nullptr;

        if (req_valid) {
            extract_index_tag(cpu_req->addr, index, tag);
            line = &cache[index];
        }

        if (state == State::IDLE) {
            cpu_ready = true;
            if (req_valid) {
                cpu_ready = false;
                state = State::COMPARE_TAG;
                cout << "  -> CPU issued " << cpu_req->op << " for Address " << cpu_req->addr 
                          << " (Index:" << index << ", Tag:" << tag << ")" << endl;
            }
        } 
        else if (state == State::COMPARE_TAG) {
            // Check Hit or Miss
            bool hit = line->valid && (line->tag == tag);
            
            if (hit) {
                cout << "  -> Cache HIT!" << endl;
                if (cpu_req->op == "WRITE") {
                    line->dirty = true;
                }
                cpu_ready = true;
                state = State::IDLE;
            } else {
                cout << "  -> Cache MISS! (Valid: " << (line->valid ? "True" : "False") 
                          << ", Dirty: " << (line->dirty ? "True" : "False") << ")" << endl;
                if (line->valid && line->dirty) {
                    state = State::WRITE_BACK;
                } else {
                    state = State::ALLOCATE;
                }
            }
        } 
        else if (state == State::WRITE_BACK) {
            mem_write = true;
            cout << "  -> Writing dirty block back to Memory..." << endl;
            if (memory.request()) { // Wait for memory latency
                cout << "  -> Memory Write Complete." << endl;
                mem_write = false;
                line->dirty = false;
                state = State::ALLOCATE;
            }
        } 
        else if (state == State::ALLOCATE) {
            mem_read = true;
            cout << "  -> Reading new block from Memory..." << endl;
            if (memory.request()) { // Wait for memory latency
                cout << "  -> Memory Read Complete." << endl;
                mem_read = false;
                // Update cache line
                line->valid = true;
                line->tag = tag;
                line->dirty = false;
                state = State::COMPARE_TAG;
            }
        }
    }
};

void run_simulation(const vector<CPURequest>& requests) {
    CacheController cache;
    int cycle = 1;
    size_t req_idx = 0;
    
    cout << "=== STARTING CACHE FSM SIMULATION ===\n\n";
    while (req_idx < requests.size() || cache.state != State::IDLE) {
        cout << "--- Cycle " << cycle << " ---\n";
        
        // Provide next request if CPU is ready and requests are left
        optional<CPURequest> current_req = nullopt;
        if (cache.cpu_ready && req_idx < requests.size()) {
            current_req = requests[req_idx];
            req_idx++;
        } else if (!cache.cpu_ready && req_idx > 0) {
            current_req = requests[req_idx - 1]; // CPU holding the request
        }
            
        cache.tick(current_req);
        cycle++;
        cout << endl;
    }
    
    cout << "=== SIMULATION COMPLETE ===" << endl;
}

int main() {
    cout << "Test Sequence: \n";
    string input;
    vector<CPURequest> requests;
    while(true){
        cout << "Enter CPU Request (e.g., READ 0, WRITE 4) or 'END' to finish: ";
        getline(cin, input);
        if (input == "END") break;

        size_t space_pos = input.find(' ');
        if (space_pos == string::npos) {
            cout << "Invalid input format. Please try again." << endl;
            continue;
        }

        string op = input.substr(0, space_pos);
        int addr = stoi(input.substr(space_pos + 1));
        requests.push_back({op, addr});
    }

    cout << endl;

    run_simulation(requests);

    return 0;
}