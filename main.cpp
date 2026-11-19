#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <stack>
#include <algorithm>

using namespace std;

struct ResourceUnit 
{
    string role;
    int id;
    int shift_start;
    int shift_end;
    int cooldown;
    int busy_until;
};

struct UnitCompare 
{
    bool operator()(const ResourceUnit* a, const ResourceUnit* b) const 
    {
        if (a->shift_end != b->shift_end) return a->shift_end > b->shift_end;
        return a->id > b->id;
    }
};

struct TaskNode 
{
    string key;           
    string patient_id;
    string name;
    int priority;         
    int arrival_time;
    int duration;
    vector<string> roles;
    vector<string> adj;
    int in_degree = 0;
    int est = 0;
    int start_time = -1;
    int finish_time = -1;
    string status = "LOCKED";
    vector<pair<string, int>> locked_units;
};

struct ReadyEntry 
{
    int priority;
    int est;
    string key;

    bool operator>(const ReadyEntry& other) const 
    {
        if (priority != other.priority) return priority > other.priority;
        return est > other.est;
    }
};

struct ScheduleLog 
{
    string patient_id;
    string task_name;
    int priority;
    int start_time;
    int end_time;
    vector<pair<string, int>> units;
};

class HospitalScheduler 
{
private:
    unordered_map<string, vector<ResourceUnit>> resources;
    unordered_map<string, TaskNode> tasks;
    vector<string> all_task_keys;
    vector<ScheduleLog> timetable;
    vector<int> completed_finish_times;

public:
    void addResourceUnit(const string& role, int shift_start, int shift_end, int cooldown = 0) 
    {
        int id = (int)resources[role].size();
        resources[role].push_back({role, id, shift_start, shift_end, cooldown, 0});
    }

    void addPatient(const string& patient_id, int arrival_time, int priority,
                    const unordered_map<string, vector<string>>& p_graph,
                    const unordered_map<string, int>& p_durations,
                    const unordered_map<string, vector<string>>& p_roles) 
    {
        for (const auto& pair : p_durations) 
        {
            string t_name = pair.first;
            string key = patient_id + ":" + t_name;
            TaskNode node;
            node.key = key;
            node.patient_id = patient_id;
            node.name = t_name;
            node.priority = priority;
            node.arrival_time = arrival_time;
            node.duration = pair.second;
            node.roles = p_roles.at(t_name);
            node.est = arrival_time;

            tasks[key] = node;
            all_task_keys.push_back(key);
        }

        for (const auto& pair : p_graph) 
        {
            string u_key = patient_id + ":" + pair.first;
            for (const string& v_name : pair.second) 
            {
                string v_key = patient_id + ":" + v_name;
                tasks[u_key].adj.push_back(v_key);
                tasks[v_key].in_degree++;
            }
        }
    }

    void analyzePatientCriticalPath(const string& patient_id) 
    {
        unordered_map<string, int> temp_in;
        unordered_map<string, int> dp_est;
        unordered_map<string, string> parent;
        queue<string> q;
        vector<string> p_keys;

        for (const string& key : all_task_keys) 
        {
            if (tasks[key].patient_id == patient_id) 
            {
                p_keys.push_back(key);
                temp_in[key] = tasks[key].in_degree;
                dp_est[key] = tasks[key].arrival_time;
                parent[key] = "";
                if (temp_in[key] == 0) q.push(key);
            }
        }

        int visited = 0;
        while (!q.empty()) 
        {
            string u = q.front();
            q.pop();
            visited++;

            for (const string& v : tasks[u].adj) 
            {
                if (dp_est[u] + tasks[u].duration > dp_est[v]) 
                {
                    dp_est[v] = dp_est[u] + tasks[u].duration;
                    parent[v] = u;
                }
                if (--temp_in[v] == 0) q.push(v);
            }
        }

        if (visited < (int)p_keys.size()) 
        {
            cout << "[ERROR] Cycle detected in treatment DAG for " << patient_id << "!\n";
            return;
        }

        int max_end = 0;
        string end_node = "";
        for (const string& key : p_keys) 
        {
            if (dp_est[key] + tasks[key].duration > max_end) 
            {
                max_end = dp_est[key] + tasks[key].duration;
                end_node = key;
            }
        }

        stack<string> st;
        for (string curr = end_node; curr != ""; curr = parent[curr]) 
        {
            st.push(tasks[curr].name);
        }

        cout << patient_id << " Critical Path (Min Discharge: Hr " << max_end << "): ";
        while (!st.empty()) 
        {
            cout << st.top();
            st.pop();
            if (!st.empty()) cout << " -> ";
        }
        cout << "\n";
    }

    void runSimulation(int max_hours = 48) 
    {
        priority_queue<int, vector<int>, greater<int>> event_pq;
        priority_queue<ReadyEntry, vector<ReadyEntry>, greater<ReadyEntry>> ready_pq;
        event_pq.push(0);

        for (auto& r_pair : resources) 
        {
            for (auto& unit : r_pair.second) 
            {
                if (unit.shift_start > 0) event_pq.push(unit.shift_start);
            }
        }

        for (const string& key : all_task_keys) 
        {
            if (tasks[key].in_degree == 0) 
            {
                tasks[key].status = "READY";
                ready_pq.push({tasks[key].priority, tasks[key].est, key});
                event_pq.push(tasks[key].arrival_time);
            }
        }

        int last_time = -1;
        while (!event_pq.empty()) 
        {
            int t = event_pq.top();
            event_pq.pop();
            if (t == last_time || t > max_hours) continue;
            last_time = t;

            for (const string& key : all_task_keys) 
            {
                TaskNode& task = tasks[key];
                if (task.status == "RUNNING" && task.finish_time == t) 
                {
                    task.status = "COMPLETED";
                    completed_finish_times.push_back(t);
                    timetable.push_back({task.patient_id, task.name, task.priority, task.start_time, t, task.locked_units});

                    for (auto& lu : task.locked_units) 
                    {
                        ResourceUnit& unit = resources[lu.first][lu.second];
                        unit.busy_until = t + unit.cooldown;
                        event_pq.push(unit.busy_until);
                    }

                    for (const string& next_key : task.adj) 
                    {
                        tasks[next_key].est = max(tasks[next_key].est, t);
                        if (--tasks[next_key].in_degree == 0 && tasks[next_key].status == "LOCKED") 
                        {
                            tasks[next_key].status = "READY";
                            ready_pq.push({tasks[next_key].priority, tasks[next_key].est, next_key});
                        }
                    }
                }
            }

            vector<ReadyEntry> deferred;
            while (!ready_pq.empty()) 
            {
                ReadyEntry top = ready_pq.top();
                ready_pq.pop();
                TaskNode& task = tasks[top.key];
                if (task.status != "READY") continue;

                if (task.est > t) 
                {
                    deferred.push_back(top);
                    event_pq.push(task.est);
                    continue;
                }

                vector<ResourceUnit*> chosen;
                bool all_free = true;
                for (const string& role : task.roles) 
                {
                    ResourceUnit* u = findAvailableUnit(role, t, task.duration, task.priority == 1);
                    if (u == nullptr) 
                    { 
                        all_free = false; 
                        break; 
                    }
                    chosen.push_back(u);
                }

                if (!all_free) 
                {
                    deferred.push_back(top);
                    continue;
                }

                task.status = "RUNNING";
                task.start_time = t;
                task.finish_time = t + task.duration;
                task.locked_units.clear();

                for (ResourceUnit* u : chosen) 
                {
                    u->busy_until = task.finish_time;
                    task.locked_units.push_back({u->role, u->id});
                }
                event_pq.push(task.finish_time);
            }
            for (const auto& entry : deferred) ready_pq.push(entry);
        }
        sort(completed_finish_times.begin(), completed_finish_times.end());
    }

    int completedTasksByHour(int target_hour) 
    {
        auto it = upper_bound(completed_finish_times.begin(), completed_finish_times.end(), target_hour);
        return distance(completed_finish_times.begin(), it);
    }

    void printTimetable() 
    {
        cout << "\n==================== HOSPITAL MASTER TIMETABLE ====================\n";
        for (const auto& log : timetable) 
        {
            string tag = (log.priority == 1) ? "[EMERGENCY]" : "[Routine]  ";
            cout << "Hr " << log.start_time << "-" << log.end_time << "\t| "
                 << log.patient_id << " " << tag << " | " << log.task_name << "\t| ";
            for (int i = 0; i < (int)log.units.size(); ++i) 
            {
                cout << log.units[i].first << " #" << log.units[i].second;
                if (i + 1 < (int)log.units.size()) cout << " + ";
            }
            cout << "\n";
        }
        cout << "===================================================================\n";
    }

private:
    ResourceUnit* findAvailableUnit(const string& role, int t, int dur, bool is_emergency) 
    {
        priority_queue<ResourceUnit*, vector<ResourceUnit*>, UnitCompare> free_pq;
        for (auto& unit : resources[role]) 
        {
            if (unit.busy_until <= t && t >= unit.shift_start && t < unit.shift_end) 
            {
                if (is_emergency || t + dur <= unit.shift_end) free_pq.push(&unit);
            }
        }
        return free_pq.empty() ? nullptr : free_pq.top();
    }
};

int main() 
{
    HospitalScheduler hospital;

    hospital.addResourceUnit("Nurse", 0, 18, 0);
    hospital.addResourceUnit("Radiologist", 0, 18, 0);
    hospital.addResourceUnit("Surgeon", 0, 6, 0);
    hospital.addResourceUnit("Surgeon", 6, 18, 0);
    hospital.addResourceUnit("OR_Room", 0, 24, 1);

    vector<string> patient_ids;
    int option = 0;

    while (true) 
    {
        cout << "\n=== HOSPITAL CLINICAL SCHEDULER MENU ===\n";
        cout << "1. Add Patient -> Procedure 1: Standard Surgery (BloodWork -> Surgery -> Discharge)\n";
        cout << "2. Add Patient -> Procedure 2: Parallel Scan + Lab (BloodWork & MRI -> Surgery -> Discharge)\n";
        cout << "3. Add Patient -> Procedure 3: Emergency Trauma (TraumaScan -> EmergencyOp -> Discharge)\n";
        cout << "4. Add Patient -> Custom Procedure\n";
        cout << "5. Add Extra Staff / Room Shift\n";
        cout << "6. Run Schedule & View Results\n";
        cout << "Choose option: ";
        cin >> option;

        if (option == 1) 
        {
            string pid;
            int arrival, priority;
            cout << "Enter Patient Name, Arrival Hour, Priority (1=Emergency, 3=Routine): ";
            cin >> pid >> arrival >> priority;

            hospital.addPatient(pid, arrival, priority,
                {{"BloodWork", {"Surgery"}}, {"Surgery", {"Discharge"}}, {"Discharge", {}}},
                {{"BloodWork", 1}, {"Surgery", 3}, {"Discharge", 1}},
                {{"BloodWork", {"Nurse"}}, {"Surgery", {"Surgeon", "OR_Room"}}, {"Discharge", {"Nurse"}}}
            );
            patient_ids.push_back(pid);
        } 
        else if (option == 2) 
        {
            string pid;
            int arrival, priority;
            cout << "Enter Patient Name, Arrival Hour, Priority (1=Emergency, 3=Routine): ";
            cin >> pid >> arrival >> priority;

            hospital.addPatient(pid, arrival, priority,
                {{"BloodWork", {"Surgery"}}, {"MRI", {"Surgery"}}, {"Surgery", {"Discharge"}}, {"Discharge", {}}},
                {{"BloodWork", 1}, {"MRI", 2}, {"Surgery", 3}, {"Discharge", 1}},
                {{"BloodWork", {"Nurse"}}, {"MRI", {"Radiologist"}}, {"Surgery", {"Surgeon", "OR_Room"}}, {"Discharge", {"Nurse"}}}
            );
            patient_ids.push_back(pid);
        } 
        else if (option == 3) 
        {
            string pid;
            int arrival;
            cout << "Enter Emergency Patient Name and Arrival Hour: ";
            cin >> pid >> arrival;

            hospital.addPatient(pid, arrival, 1,
                {{"TraumaScan", {"EmergencyOp"}}, {"EmergencyOp", {"Discharge"}}, {"Discharge", {}}},
                {{"TraumaScan", 1}, {"EmergencyOp", 2}, {"Discharge", 1}},
                {{"TraumaScan", {"Radiologist"}}, {"EmergencyOp", {"Surgeon", "OR_Room"}}, {"Discharge", {"Nurse"}}}
            );
            patient_ids.push_back(pid);
        } 
        else if (option == 4) 
        {
            string pid;
            int arrival, priority, n_tasks;
            cout << "Enter Patient Name, Arrival Hour, Priority (1-3), Number of Tasks: ";
            cin >> pid >> arrival >> priority >> n_tasks;

            unordered_map<string, vector<string>> g;
            unordered_map<string, int> dur;
            unordered_map<string, vector<string>> roles;

            for (int i = 0; i < n_tasks; ++i) 
            {
                string tname;
                int d, rcount;
                cout << "  Task " << i + 1 << " (Name Duration NumRoles): ";
                cin >> tname >> d >> rcount;
                dur[tname] = d;
                g[tname] = {};
                cout << "  Enter " << rcount << " role(s): ";
                for (int r = 0; r < rcount; ++r) 
                {
                    string rname;
                    cin >> rname;
                    roles[tname].push_back(rname);
                }
            }

            int n_edges;
            cout << "  Number of dependency arrows (A -> B): ";
            cin >> n_edges;
            for (int e = 0; e < n_edges; ++e) 
            {
                string u, v;
                cout << "  Dependency " << e + 1 << " (Prerequisite NextTask): ";
                cin >> u >> v;
                g[u].push_back(v);
            }

            hospital.addPatient(pid, arrival, priority, g, dur, roles);
            patient_ids.push_back(pid);
        } 
        else if (option == 5) 
        {
            string role;
            int s_start, s_end, cool;
            cout << "Enter Role, ShiftStart, ShiftEnd, CooldownHours: ";
            cin >> role >> s_start >> s_end >> cool;
            hospital.addResourceUnit(role, s_start, s_end, cool);
        } 
        else if (option == 6) 
        {
            break;
        }
    }

    cout << "\n";
    for (const string& pid : patient_ids) 
    {
        hospital.analyzePatientCriticalPath(pid);
    }

    hospital.runSimulation();
    hospital.printTimetable();

    int query_hour;
    cout << "\nEnter target hour to check completed procedures : ";
    cin >> query_hour;
    cout << "Procedures completed by Hour " << query_hour << ": " << hospital.completedTasksByHour(query_hour) << "\n";

    return 0;
}