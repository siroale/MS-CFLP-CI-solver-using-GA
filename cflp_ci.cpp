#include <iostream>
#include <vector>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <random>
#include <chrono>
#include <filesystem>
#include <climits>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <bitset>

using namespace std;

constexpr int MAX_CLIENTS = 8192;

/**
 * @brief Representacion de una asignacion de demanda
 */
struct Assignment {
    int client;
    int facility;
    int amount;
};

/**
 * @brief Cromosoma con codificacion indirecta
 *
 * Contiene un vector booleano para la prioridad de las instalaciones y un
 * vector de permutacion para establecer el orden de asignacion de los clientes.
 */
struct Chromosome {
    vector<bool> facility_priority;
    vector<int> client_priority;
};

/**
 * @brief Solucion decodificada y factible
 *
 * Mantiene las asignaciones concretas, el costo total (fitness), 
 * el tiempo de ejecucion y el cromosoma de origen.
 */
struct Solution {
    vector<Assignment> assignments;
    vector<bool> open_facilities;
    long long total_cost = -1;
    int seed_used;
    double execution_time_ms;
    int iterations;
    Chromosome chrom;
};

/**
 * @brief Estructura que almacena los datos de la instancia del MS-CFLP-CI
 *
 * Incluye capacidades, costos, demandas, incompatibilidades y mascaras de bits
 * (std::bitset) precomputadas para validacion de factibilidad.
 */
struct Instance {
    int m;
    int n;
    vector<int> capacities;
    vector<int> opening_costs;
    vector<int> demands;
    vector<vector<int>> unit_costs;
    int num_incompatibilities;
    vector<pair<int, int>> incompatibilities;
    vector<bitset<MAX_CLIENTS>> incompatibility_mask;
    vector<vector<int>> base_facility_order;
};

/**
 * @brief Configuracion global de parametros del algoritmo
 */
struct Config {
    int pop_size = 50;
    int num_generations = 500;
    double mut_rate_fac = 0.1;
    double mut_rate_cli = 0.05;
    double crossover_rate = 0.8;
    int tournament_k = 3;
    int max_generations_without_improvement = 50;
    vector<int> seeds = {1234};
    double facility_priority_init_prob = 0.10;
    int shuffle_prob_pct = 30;
    int shuffle_window = 3;
    int max_init_attempts = 1000;
    int num_threads = 1;
    vector<string> target_instances;
};

/**
 * @brief Lee los parametros desde el archivo de configuracion
 * @param filename Ruta al archivo (ej: config.txt)
 * @return Config Objeto con los parametros cargados
 */
Config readConfig(const string& filename) {
    Config config;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Advertencia: No se pudo abrir " << filename << ", usando configuracion por defecto.\n";
        return config;
    }
    
    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos == string::npos) continue;
        
        string key = line.substr(0, pos);
        string val = line.substr(pos + 1);
        
        if (key == "pop_size") config.pop_size = stoi(val);
        else if (key == "num_generations") config.num_generations = stoi(val);
        else if (key == "mut_rate_fac") config.mut_rate_fac = stod(val);
        else if (key == "mut_rate_cli") config.mut_rate_cli = stod(val);
        else if (key == "crossover_rate") config.crossover_rate = stod(val);
        else if (key == "tournament_k") config.tournament_k = stoi(val);
        else if (key == "max_generations_without_improvement") config.max_generations_without_improvement = stoi(val);
        else if (key == "facility_priority_init_prob") config.facility_priority_init_prob = stod(val);
        else if (key == "shuffle_prob_pct") config.shuffle_prob_pct = stoi(val);
        else if (key == "shuffle_window") config.shuffle_window = stoi(val);
        else if (key == "max_init_attempts") config.max_init_attempts = stoi(val);
        else if (key == "num_threads") config.num_threads = stoi(val);
        else if (key == "seeds") {
            config.seeds.clear();
            stringstream ss(val);
            string seed_str;
            while (getline(ss, seed_str, ',')) {
                config.seeds.push_back(stoi(seed_str));
            }
        }
        else if (key == "target_instances") {
            stringstream ss(val);
            string inst_str;
            while (getline(ss, inst_str, ',')) {
                config.target_instances.push_back(inst_str);
            }
        }
    }
    return config;
}

/**
 * @brief Lee una instancia desde un archivo .in y precomputa estructuras auxiliares
 * @param filename Ruta al archivo de la instancia
 * @return Instance Estructura con la instancia cargada
 */
Instance readInstance(const string& filename) {
    Instance inst;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error al abrir archivo: " << filename << endl;
        exit(1);
    }

    file >> inst.m >> inst.n;
    
    if (inst.n > MAX_CLIENTS) {
        cerr << "Error: la instancia tiene " << inst.n << " clientes, supera MAX_CLIENTS=" << MAX_CLIENTS << endl;
        exit(1);
    }
    
    inst.capacities.resize(inst.m);
    for (int i = 0; i < inst.m; ++i) file >> inst.capacities[i];
    
    inst.opening_costs.resize(inst.m);
    for (int i = 0; i < inst.m; ++i) file >> inst.opening_costs[i];
    
    inst.demands.resize(inst.n);
    for (int i = 0; i < inst.n; ++i) file >> inst.demands[i];
    
    inst.unit_costs.assign(inst.n, vector<int>(inst.m));
    for (int i = 0; i < inst.n; ++i) {
        for (int j = 0; j < inst.m; ++j) {
            file >> inst.unit_costs[i][j];
        }
    }
    
    file >> inst.num_incompatibilities;
    inst.incompatibility_mask.assign(inst.n, bitset<MAX_CLIENTS>());
    for (int k = 0; k < inst.num_incompatibilities; ++k) {
        int u, v;
        file >> u >> v;
        u--; v--;
        inst.incompatibilities.push_back({u, v});
        inst.incompatibility_mask[u].set(v);
        inst.incompatibility_mask[v].set(u);
    }
    
    inst.base_facility_order.assign(inst.n, vector<int>(inst.m));
    for (int i = 0; i < inst.n; ++i) {
        vector<int>& order = inst.base_facility_order[i];
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(), [&](int a, int b) {
            return inst.unit_costs[i][a] < inst.unit_costs[i][b];
        });
    }
    
    return inst;
}

/**
 * @brief Decodificador heuristico que transforma un Cromosoma en una Solucion factible
 *
 * Utiliza mascara de bits (bitsets) para verificar colisiones por incompatibilidad de clientes.
 *
 * @param chrom Cromosoma a decodificar
 * @param inst Datos de la instancia
 * @param seed Semilla
 * @param rng Generador de numeros aleatorios
 * @param config Parametros del algoritmo
 * @return Solution Solucion generada (factible o con total_cost = -1 si falla)
 */
Solution decodeChromosome(
    const Chromosome& chrom,
    const Instance& inst,
    int seed, mt19937& rng,
    const Config& config
) {
    Solution sol;
    sol.open_facilities.assign(inst.m, false);
    sol.seed_used = seed;
    sol.chrom = chrom;
    
    vector<int> remaining_demand = inst.demands;
    vector<int> remaining_capacity = inst.capacities;
    vector<bitset<MAX_CLIENTS>> facility_assigned_mask(inst.m);
    
    long long running_cost = 0;
    
    vector<int> facilities(inst.m);
    
    for (int i : chrom.client_priority) {
        const vector<int>& base_order = inst.base_facility_order[i];
        
        int prio_count = 0;
        for (int j : base_order) {
            if (chrom.facility_priority[j]) {
                facilities[prio_count++] = j;
            }
        }
        int k = prio_count;
        for (int j : base_order) {
            if (!chrom.facility_priority[j]) {
                facilities[k++] = j;
            }
        }
        
        uniform_int_distribution<int> prob(0, 99);
        if (prob(rng) < config.shuffle_prob_pct) {
            shuffle(facilities.begin(), facilities.begin() + min(inst.m, config.shuffle_window), rng);
        }
        
        for (int j : facilities) {
            if (remaining_demand[i] == 0) break;
            
            if (remaining_capacity[j] == 0) continue;
            
            if ((facility_assigned_mask[j] & inst.incompatibility_mask[i]).any()) continue;
            
            int amount = min(remaining_demand[i], remaining_capacity[j]);
            sol.assignments.push_back({i, j, amount});
            running_cost += (long long)amount * inst.unit_costs[i][j];
            
            remaining_demand[i] -= amount;
            remaining_capacity[j] -= amount;
            sol.open_facilities[j] = true;
            facility_assigned_mask[j].set(i);
        }
        
        if (remaining_demand[i] > 0) {
            sol.total_cost = -1;
            return sol; 
        }
    }
    
    for (int j = 0; j < inst.m; ++j) {
        if (sol.open_facilities[j]) {
            running_cost += inst.opening_costs[j];
        }
    }
    sol.total_cost = running_cost;
    return sol;
}

/**
 * @brief Genera un cromosoma aleatorio inicial
 *
 * Asigna prioridades aleatorias a las bodegas basadas en facility_priority_init_prob
 * y genera una permutacion aleatoria para los clientes.
 *
 * @param inst Datos de la instancia
 * @param rng Generador de numeros aleatorios
 * @param config Parametros del algoritmo
 * @return Chromosome Cromosoma generado
 */
Chromosome generateRandomChromosome(
    const Instance& inst,
    mt19937& rng,
    const Config& config
) {
    Chromosome chrom;
    chrom.facility_priority.resize(inst.m, false);
    uniform_real_distribution<double> prob(0.0, 1.0);
    for (int j = 0; j < inst.m; ++j) {
        chrom.facility_priority[j] = prob(rng) < config.facility_priority_init_prob;
    }
    
    chrom.client_priority.resize(inst.n);
    iota(chrom.client_priority.begin(), chrom.client_priority.end(), 0);
    shuffle(chrom.client_priority.begin(), chrom.client_priority.end(), rng);
    
    return chrom;
}

/**
 * @brief Genera iterativamente cromosomas hasta encontrar una Solucion factible
 *
 * Continua generando y decodificando cromosomas aleatorios hasta un maximo
 * de 'max_init_attempts'. Si el decodificador logra acomodar todos los clientes,
 * retorna la solucion.
 *
 * @param inst Datos de la instancia
 * @param rng Generador de numeros aleatorios
 * @param seed Semilla actual
 * @param config Parametros del algoritmo
 * @return Solution Solucion inicial factible
 */
Solution generateFeasibleSolution(
    const Instance& inst,
    mt19937& rng,
    int seed,
    const Config& config
) {
    int attempts = 0;
    Solution sol;
    do {
        Chromosome chrom = generateRandomChromosome(inst, rng, config);
        sol = decodeChromosome(chrom, inst, seed, rng, config);
        attempts++;
    } while (sol.total_cost == -1 && attempts < config.max_init_attempts);
    return sol;
}

/**
 * @brief Operador de seleccion por torneo
 *
 * Escoge 'k' individuos al azar y selecciona al que tenga menor costo (mejor fitness).
 *
 * @param pop Poblacion actual
 * @param rng Generador de numeros aleatorios
 * @param k Tamano del torneo
 * @return int Indice del individuo ganador en la poblacion
 */
int tournamentSelection(
    const vector<Solution>& pop,
    mt19937& rng,
    int k
) {
    uniform_int_distribution<int> dist(0, pop.size() - 1);
    int best_idx = dist(rng);
    long long best_cost = pop[best_idx].total_cost;
    if (best_cost == -1) best_cost = LLONG_MAX;

    for (int i = 1; i < k; ++i) {
        int idx = dist(rng);
        long long cost = pop[idx].total_cost;
        if (cost == -1) cost = LLONG_MAX;
        
        if (cost < best_cost) {
            best_cost = cost;
            best_idx = idx;
        }
    }
    return best_idx;
}

/**
 * @brief Operador de cruzamiento Partially Mapped Crossover (PMX) Circular
 *
 * Combina dos permutaciones de clientes heredando un segmento acotado (maximo n/2)
 * y mapeando los elementos restantes para preservar la validez de la permutacion.
 *
 * @param p1 Permutacion padre 1
 * @param p2 Permutacion padre 2
 * @param rng Generador de numeros aleatorios
 * @return vector<int> Permutacion hija resultante
 */
vector<int> pmxCrossover(
    const vector<int>& p1,
    const vector<int>& p2,
    mt19937& rng
) {
    int n = p1.size();
    vector<int> child = p1;
    
    int max_segment = n / 2;
    uniform_int_distribution<int> seg_len_dist(0, max_segment);
    int seg_len = seg_len_dist(rng);
    
    uniform_int_distribution<int> start_dist(0, n - 1);
    int pt1 = start_dist(rng);
    
    vector<int> mapping(n, -1);
    vector<bool> in_segment(n, false);
    
    for (int offset = 0; offset <= seg_len; ++offset) {
        int idx = (pt1 + offset) % n;
        child[idx] = p2[idx];
        mapping[p2[idx]] = p1[idx];
        in_segment[p2[idx]] = true;
    }
    
    for (int i = 0; i < n; ++i) {
        bool inside = false;
        int rel = (i - pt1 + n) % n;
        if (rel <= seg_len) inside = true;
        if (inside) continue;
        int val = p1[i];
        while (in_segment[val]) {
            val = mapping[val];
        }
        child[i] = val;
    }
    
    return child;
}

/**
 * @brief Operador de Uniform Crossover para las bodegas
 *
 * Cruza las prioridades binarias de instalaciones del padre 2 hacia el hijo (in place).
 *
 * @param child_fac Vector de prioridad a modificar
 * @param p2_fac Vector de prioridad del segundo padre
 * @param rng Generador de numeros aleatorios
 */
void uniformCrossoverInto(
    vector<bool>& child_fac,
    const vector<bool>& p2_fac,
    mt19937& rng
) {
    int m = child_fac.size();
    uniform_int_distribution<int> dist(0, 1);
    for (int j = 0; j < m; ++j) {
        if (dist(rng)) {
            child_fac[j] = p2_fac[j];
        }
    }
}

/**
 * @brief Operador maestro de cruzamiento del Cromosoma
 *
 * Invoca PMX circular para clientes y Uniform Crossover para bodegas.
 *
 * @param p1 Cromosoma padre 1
 * @param p2 Cromosoma padre 2
 * @param rng Generador de numeros aleatorios
 * @return Chromosome Cromosoma hijo
 */
Chromosome crossover(
    const Chromosome& p1,
    const Chromosome& p2,
    mt19937& rng
) {
    Chromosome child;
    child.client_priority = pmxCrossover(p1.client_priority, p2.client_priority, rng);
    child.facility_priority = p1.facility_priority;
    uniformCrossoverInto(child.facility_priority, p2.facility_priority, rng);
    return child;
}

/**
 * @brief Operador de mutacion del Cromosoma (in place)
 *
 * Aplica Bit-flip para las prioridades de instalaciones, y operador Swap
 * para la permutacion de clientes segun las tasas de mutacion.
 *
 * @param chrom Cromosoma a mutar
 * @param mut_rate_fac Tasa de mutacion (bit-flip) para instalaciones
 * @param mut_rate_cli Tasa de mutacion (swap) para clientes
 * @param rng Generador aleatorio
 */
void mutate(
    Chromosome& chrom,
    double mut_rate_fac,
    double mut_rate_cli,
    mt19937& rng
) {
    uniform_real_distribution<double> prob(0.0, 1.0);
    
    for (size_t j = 0; j < chrom.facility_priority.size(); ++j) {
        if (prob(rng) < mut_rate_fac) {
            chrom.facility_priority[j] = !chrom.facility_priority[j];
        }
    }
    
    for (size_t i = 0; i < chrom.client_priority.size(); ++i) {
        if (prob(rng) < mut_rate_cli) {
            uniform_int_distribution<int> dist(0, chrom.client_priority.size() - 1);
            int swap_idx = dist(rng);
            swap(chrom.client_priority[i], chrom.client_priority[swap_idx]);
        }
    }
}

/**
 * @brief Bucle principal del Algoritmo Genetico
 *
 * Controla la evolucion: seleccion, cruzamiento, mutacion y generacion
 * de descendencia. Implementa el criterio de termino por estancamiento.
 * Escribe localmente los historiales de convergencia.
 *
 * @param inst Datos de la instancia
 * @param seed Semilla actual
 * @param instance_name Nombre base de la instancia para los archivos de salida
 * @param config Parametros del AG
 * @return Solution Mejor solucion encontrada al terminar
 */
Solution runGA(
    const Instance& inst,
    int seed,
    const string& instance_name,
    const Config& config
) {
    mt19937 rng(seed);
    
    int pop_size = config.pop_size;
    int num_generations = config.num_generations;
    double mut_rate_fac = config.mut_rate_fac;
    double mut_rate_cli = config.mut_rate_cli;
    double crossover_rate = config.crossover_rate;
    
    auto start_time = chrono::high_resolution_clock::now();
    
    vector<Solution> population(pop_size);
    for (int i = 0; i < pop_size; ++i) {
        population[i] = generateFeasibleSolution(inst, rng, seed, config);
    }
    
    Solution best_overall = population[0];
    for (int i = 1; i < pop_size; ++i) {
        if (population[i].total_cost != -1 && (best_overall.total_cost == -1 || 
            population[i].total_cost < best_overall.total_cost)) {
            best_overall = population[i];
        }
    }
    
    stringstream conv_buffer;
    conv_buffer << "Generation,BestCost\n";
    conv_buffer << "0," << best_overall.total_cost << "\n";
    
    int gens_without_improvement = 0;

    for (int gen = 1; gen <= num_generations; ++gen) {
        vector<Solution> new_population;
        new_population.reserve(pop_size);
        new_population.push_back(best_overall);
        
        while ((int)new_population.size() < pop_size) {
            int p1_idx = tournamentSelection(population, rng, config.tournament_k);
            int p2_idx = tournamentSelection(population, rng, config.tournament_k);
            
            Chromosome child_chrom = population[p1_idx].chrom;
            
            uniform_real_distribution<double> prob(0.0, 1.0);
            if (prob(rng) < crossover_rate) {
                child_chrom = crossover(population[p1_idx].chrom, population[p2_idx].chrom, rng);
            }
            
            mutate(child_chrom, mut_rate_fac, mut_rate_cli, rng);
            Solution child = decodeChromosome(child_chrom, inst, seed, rng, config);
            new_population.push_back(std::move(child));
        }
        
        population = std::move(new_population);
        
        long long prev_best = best_overall.total_cost;
        for (const auto& sol : population) {
            if (sol.total_cost != -1 && (best_overall.total_cost == -1 || sol.total_cost < best_overall.total_cost)) {
                best_overall = sol;
            }
        }
        
        if (prev_best != -1 && best_overall.total_cost == prev_best) {
            gens_without_improvement++;
        } else {
            gens_without_improvement = 0;
        }

        conv_buffer << gen << "," << best_overall.total_cost << "\n";
        
        if (gens_without_improvement >= config.max_generations_without_improvement) {
            population[0] = best_overall;
            for (int i = 1; i < pop_size; ++i) {
                population[i] = generateFeasibleSolution(inst, rng, seed, config);
            }
            gens_without_improvement = 0;
        }
    }
    
    auto end_time = chrono::high_resolution_clock::now();
    best_overall.execution_time_ms = chrono::duration<double, milli>(end_time - start_time).count();
    best_overall.seed_used = seed;
    best_overall.iterations = num_generations;
    
    ofstream conv_file("convergencia_" + instance_name + "_" + to_string(seed) + ".csv");
    conv_file << conv_buffer.str();
    
    return best_overall;
}

void writeSolution(const Solution& sol, const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Error al abrir archivo de salida: " << filename << endl;
        return;
    }
    if (sol.total_cost == -1) {
        file << "Infactible\n";
        return;
    }
    file << sol.total_cost << " " << sol.execution_time_ms << " " << sol.seed_used << " " << sol.iterations << "\n";
    for (const auto& a : sol.assignments) {
        file << a.client + 1 << " " << a.facility + 1 << " " << a.amount << "\n";
    }
}

/**
 * @brief Wrapper para procesar una semilla independientemente
 *
 * Ejecuta el AG para la semilla dada y reporta los resultados de manera segura
 * en entornos de multiprocesamiento sincronizando la consola.
 *
 * @param inst Instancia base
 * @param seed Semilla a evaluar
 * @param instance_name Nombre de la instancia
 * @param config Configuracion del AG
 * @param cout_mutex Mutex para la impresion en consola
 */
void processSeed(
    const Instance& inst,
    int seed,
    const string& instance_name,
    const Config& config,
    mutex& cout_mutex
) {
    Solution best_sol = runGA(inst, seed, instance_name, config);
    
    lock_guard<mutex> lock(cout_mutex);
    if (best_sol.total_cost != -1) {
        string out_filename = "solucion_" + instance_name + "_" + to_string(seed) + ".txt";
        writeSolution(best_sol, out_filename);
        cout << "    Mejor costo (semilla " << seed << "): " << best_sol.total_cost << " (Tiempo: " << best_sol.execution_time_ms << " ms)" << endl;
    } else {
        cout << "    No se encontro solucion factible (semilla " << seed << ")." << endl;
    }
}

/**
 * @brief Punto de entrada principal
 *
 * Lee la configuracion, parsea todos los archivos de instancias aplicables (.in)
 * en el directorio, e invoca el procesamiento en lotes utilizando multiprocesamiento.
 */
int main() {
    Config config = readConfig("config.txt");

    string dir_path = "instancias";
    if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path)) {
        cerr << "Error: No se encontro el directorio '" << dir_path << "'." << endl;
        return 1;
    }

    vector<int> seeds = config.seeds;
    mutex cout_mutex;
    
    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (entry.is_regular_file()) {
            string instance_path = entry.path().string();
            string filename = entry.path().filename().string();
            
            if (filename.length() < 3 || filename.substr(filename.length() - 3) != ".in") {
                continue;
            }

            string instance_name = filename.substr(0, filename.find_last_of("."));
            if (!config.target_instances.empty()) {
                if (find(config.target_instances.begin(), config.target_instances.end(), instance_name) == config.target_instances.end()) {
                    continue;
                }
            }

            cout << "Procesando instancia: " << filename << endl;
            Instance inst = readInstance(instance_path);
            
            int num_threads = max(1, config.num_threads);
            if (num_threads <= 1) {
                for (int seed : seeds) {
                    cout << "  Ejecutando con semilla: " << seed << endl;
                    processSeed(inst, seed, instance_name, config, cout_mutex);
                }
            } else {
                vector<thread> workers;
                for (size_t s = 0; s < seeds.size(); s += num_threads) {
                    workers.clear();
                    size_t end = min(seeds.size(), s + num_threads);
                    for (size_t k = s; k < end; ++k) {
                        workers.emplace_back(processSeed, cref(inst), seeds[k], cref(instance_name), cref(config), ref(cout_mutex));
                    }
                    for (auto& t : workers) t.join();
                }
            }
        }
    }
    
    return 0;
}