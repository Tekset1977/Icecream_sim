#include <bits/stdc++.h>
using namespace std;
using Clock = double; // minutes

enum class EventType { ARRIVAL, DEPARTURE };

struct Event {
    Clock time;
    EventType type;
    int customerId;
    int serverId; // for departure
    bool operator<(Event const& o) const {
        // priority_queue is max-heap, so invert
        return time > o.time;
    }
};

struct Customer {
    int id;
    Clock arrivalTime;
    Clock serviceStart = -1;
    Clock departureTime = -1;
    int scoops = 1;
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Parameters with defaults
    int numServers = (argc > 1) ? stoi(argv[1]) : 3;
    double arrivalRatePerMin = (argc > 2) ? stod(argv[2]) : 0.5; // lambda (customers per minute)
    double avgServiceMin = (argc > 3) ? stod(argv[3]) : 1.2; // mean service time in minutes
    double pricePerScoop = (argc > 4) ? stod(argv[4]) : 3.0; // revenue per scoop
    double SIM_MINUTES = (argc > 5) ? stod(argv[5]) : 8*60; // simulation length (minutes)

    // Random generators
    std::mt19937_64 rng((unsigned)chrono::high_resolution_clock::now().time_since_epoch().count());
    // inter arrival exponential
    std::exponential_distribution<double> interarrival_dist(arrivalRatePerMin);
    // service exponential (mean = avgServiceMin)
    std::exponential_distribution<double> service_dist(1.0 / avgServiceMin);
    // scoops: discrete distribution (1..3)
    std::discrete_distribution<int> scoops_dist({0.6, 0.3, 0.1}); // Probabilities for 1,2,3 scoops

    priority_queue<Event> events;
    queue<int> waitingQueue; // store customer ids

    vector<Customer> customers;
    customers.reserve(100000);

    vector<bool> serverBusy(numServers, false);
    vector<int> serverServingCustomer(numServers, -1);

    // Schedule first arrival at t=0
    double t = 0.0;
    int nextCustId = 0;
    events.push(Event{t, EventType::ARRIVAL, nextCustId, -1});
    customers.push_back(Customer{nextCustId, t});
    nextCustId++;

    // Metrics
    double totalWaitingTime = 0.0;
    double totalServiceTime = 0.0;
    int servedCount = 0;
    double totalRevenue = 0.0;
    double idleTimeAcc = 0.0;
    vector<double> serverLastFreeTime(numServers, 0.0);

    while (!events.empty()) {
        Event ev = events.top(); events.pop();
        t = ev.time;
        if (t > SIM_MINUTES) break;

        if (ev.type == EventType::ARRIVAL) {
            // handle arrival
            int cid = ev.customerId;
            if ((int)customers.size() <= cid) {
                // shouldn't happen, but safeguard
                customers.push_back(Customer{cid, t});
            } else {
                customers[cid].arrivalTime = t;
            }

            // find a free server
            int freeServer = -1;
            for (int i = 0; i < numServers; ++i) {
                if (!serverBusy[i]) { freeServer = i; break; }
            }

            if (freeServer >= 0) {
                // start service immediately
                serverBusy[freeServer] = true;
                serverServingCustomer[freeServer] = cid;
                customers[cid].serviceStart = t;

                // scoops chosen
                int scoops = scoops_dist(rng) + 1; // distribution indexes 0->1 scoop, etc
                customers[cid].scoops = scoops;

                double serviceTime = service_dist(rng) * scoops; // scale by scoops
                totalServiceTime += serviceTime;
                double departureT = t + serviceTime;
                events.push(Event{departureT, EventType::DEPARTURE, cid, freeServer});

                // accumulate idle time for this server
                idleTimeAcc += max(0.0, t - serverLastFreeTime[freeServer]);
            } else {
                // enqueue
                waitingQueue.push(cid);
            }

            // schedule next arrival
            double ia = interarrival_dist(rng);
            double nextT = t + ia;
            if (nextT <= SIM_MINUTES) {
                int id = nextCustId++;
                customers.push_back(Customer{id, nextT});
                events.push(Event{nextT, EventType::ARRIVAL, id, -1});
            }

        } else if (ev.type == EventType::DEPARTURE) {
            int cid = ev.customerId;
            int sid = ev.serverId;
            customers[cid].departureTime = t;
            serverBusy[sid] = false;
            serverServingCustomer[sid] = -1;
            serverLastFreeTime[sid] = t;

            // record stats
            double wait = customers[cid].serviceStart - customers[cid].arrivalTime;
            totalWaitingTime += max(0.0, wait);
            servedCount++;
            totalRevenue += customers[cid].scoops * pricePerScoop;

            // if queue non-empty, start next
            if (!waitingQueue.empty()) {
                int nextCid = waitingQueue.front(); waitingQueue.pop();
                serverBusy[sid] = true;
                serverServingCustomer[sid] = nextCid;
                customers[nextCid].serviceStart = t;

                int scoops = scoops_dist(rng) + 1;
                customers[nextCid].scoops = scoops;

                double serviceTime = service_dist(rng) * scoops;
                totalServiceTime += serviceTime;
                double departureT = t + serviceTime;
                events.push(Event{departureT, EventType::DEPARTURE, nextCid, sid});

                // server was just free at t, so no extra idle accumulation here
            }
        }
    }

    // finalize metrics
    double avgWait = (servedCount>0) ? totalWaitingTime / servedCount : 0.0;
    double avgService = (servedCount>0) ? totalServiceTime / servedCount : 0.0;
    double throughputPerMin = (SIM_MINUTES>0) ? (double)servedCount / SIM_MINUTES : 0.0;

    cout << "=== Ice-Cream Shop Simulation Report ===\n";
    cout << "Servers (clerks): " << numServers << "\n";
    cout << "Simulation minutes: " << SIM_MINUTES << "\n";
    cout << "Customers served: " << servedCount << "\n";
    cout << "Throughput (cust/min): " << throughputPerMin << "\n";
    cout << fixed << setprecision(3);
    cout << "Average wait (min): " << avgWait << "\n";
    cout << "Average service time (min): " << avgService << "\n";
    cout << "Total revenue: $" << totalRevenue << "\n";
    cout << "Remaining in queue at end: " << waitingQueue.size() << "\n";
    cout << "----------------------------------------\n";
    cout << "Note: stochastic simulation -> run multiple times to estimate confidence.\n";

    return 0;
} 