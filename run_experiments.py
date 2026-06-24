import os
import subprocess
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def write_config(params):
    base_config = {
        "pop_size": 50,
        "num_generations": 500,
        "mut_rate_fac": 0.1,
        "mut_rate_cli": 0.05,
        "crossover_rate": 0.8,
        "tournament_k": 3,
        "max_generations_without_improvement": 50,
        "seeds": "1234,5678,91011,121314,151617",
        "facility_priority_init_prob": 0.10,
        "shuffle_prob_pct": 30,
        "shuffle_window": 3,
        "max_init_attempts": 1000,
        "num_threads": 5,
        "target_instances": "wlp01,wlp04,wlp05,wlp07,wlp10"
    }
    base_config.update(params)
    with open("config.txt", "w") as f:
        for k, v in base_config.items():
            f.write(f"{k}={v}\n")

def run_experiment(exp_name, instances, variations):
    print(f"--- Ejecutando {exp_name} ---")
    results = {}
    for var_name, params in variations.items():
        print(f"  Variacion: {var_name}")
        write_config(params)
        subprocess.run(["./cflp_ci_opt_fix"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        results[var_name] = {}
        for inst in instances:
            dfs = []
            for seed in ["1234", "5678", "91011", "121314", "151617"]:
                csv_file = f"convergencia_{inst}_{seed}.csv"
                if os.path.exists(csv_file):
                    df_seed = pd.read_csv(csv_file)
                    dfs.append(df_seed.set_index('Generation')['BestCost'])
            if dfs:
                df_all = pd.concat(dfs, axis=1).ffill().min(axis=1).reset_index()
                df_all.columns = ['Generation', 'BestCost']
                results[var_name][inst] = df_all
            else:
                print(f"    Faltan CSVs para {inst}")
    return results

def plot_experiment(exp_name, results, instance):
    plt.figure(figsize=(10, 6))
    for var_name, inst_data in results.items():
        if instance in inst_data:
            df = inst_data[instance]
            plt.plot(df['Generation'], df['BestCost'], label=var_name)
    plt.title(f"{exp_name} - {instance.upper()}")
    plt.xlabel("Generación")
    plt.ylabel("Mejor Costo")
    plt.legend()
    plt.grid(True)
    plt.savefig(f"{exp_name.replace(' ', '_')}_{instance}.png", dpi=300)
    plt.close()

def main():
    target_instances = ["wlp01", "wlp04", "wlp05", "wlp07", "wlp10"]
    
    exp2_vars = {
        "Greedy (0%)": {"shuffle_prob_pct": 0},
        "Equilibrado (30%)": {"shuffle_prob_pct": 30},
        "Caotico (100%)": {"shuffle_prob_pct": 100}
    }
    res2 = run_experiment("Exp2_Shuffle", target_instances, exp2_vars)
    for inst in target_instances: plot_experiment("Exp2_Shuffle", res2, inst)

    exp4_vars = {
        "Pequeña (Pop 20)": {"pop_size": 20, "num_generations": 1250},
        "Mediana (Pop 50)": {"pop_size": 50, "num_generations": 500},
        "Grande (Pop 200)": {"pop_size": 200, "num_generations": 125}
    }
    print(f"--- Ejecutando Exp4_Poblacion ---")
    res4 = {}
    for var_name, params in exp4_vars.items():
        print(f"  Variacion: {var_name}")
        write_config(params)
        subprocess.run(["./cflp_ci_opt_fix"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        res4[var_name] = {}
        for inst in target_instances:
            dfs = []
            for seed in ["1234", "5678", "91011", "121314", "151617"]:
                csv_file = f"convergencia_{inst}_{seed}.csv"
                if os.path.exists(csv_file):
                    df_seed = pd.read_csv(csv_file)
                    df_seed['Evaluations'] = df_seed['Generation'] * params['pop_size']
                    dfs.append(df_seed.set_index('Evaluations')['BestCost'])
            if dfs:
                df_all = pd.concat(dfs, axis=1).ffill().min(axis=1).reset_index()
                df_all.columns = ['Evaluations', 'BestCost']
                res4[var_name][inst] = df_all

    for inst in target_instances:
        plt.figure(figsize=(10, 6))
        for var_name, inst_data in res4.items():
            if inst in inst_data:
                df = inst_data[inst]
                plt.plot(df['Evaluations'], df['BestCost'], label=var_name)
        plt.title(f"Exp4_Poblacion - {inst.upper()}")
        plt.xlabel("Evaluaciones Totales")
        plt.ylabel("Mejor Costo")
        plt.legend()
        plt.grid(True)
        plt.savefig(f"Exp4_Poblacion_{inst}.png", dpi=300)
        plt.close()

    print("--- Ejecutando corrida final (Parametros por defecto) para la tabla ---")
    write_config({"target_instances": "wlp01,wlp04,wlp05,wlp07,wlp10,wlp30"})
    subprocess.run(["./cflp_ci_opt_fix"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("--- Terminado ---")

if __name__ == "__main__":
    main()
