# Algoritmo Genético para CFLP-CI

Este repositorio contiene la implementación en C++ de un Algoritmo Genético con decodificador heurístico y optimización mediante máscaras de bits para resolver el *Capacitated Facility Location Problem with Customer Incompatibilities* (CFLP-CI).

## Requisitos y Dependencias

C++17.
Make

## Compilación

Abrir una terminal en el directorio del proyecto y ejecutar el comando:

```bash
make
```

Esto generará el archivo binario ejecutable `cflp_ci`. Si desea limpiar los archivos compilados previamente y los outputs generados, puede ejecutar:

```bash
make clean
```

## Ejecución

Una vez compilado, el algoritmo puede ejecutarse directamente. Para procesar múltiples instancias y aplicar las configuraciones, se pueden utilizar las siguientes alternativas:

### Ejecución Directa

```bash
./cflp_ci
```

Por defecto, el programa leerá las instancias en formato `.in` que se encuentren en el directorio `instancias/`, y los parámetros del archivo `config.txt`.

### Ejecución de Experimentos

Se incluye un script en Python llamado `run_experiments.py` cuyo único propósito es automatizar la generación de tablas de resultados y gráficos experimentales (no es necesario para la ejecución principal del algoritmo). Si desea ejecutar las pruebas y generar los gráficos:

```bash
python3 run_experiments.py
```

## Configuración de Parámetros

El comportamiento del algoritmo se controla completamente a través del archivo de configuración de texto plano `config.txt`. No es necesario modificar ni recompilar el código fuente para alterar los parámetros.

El archivo `config.txt` debe contener los parámetros en formato `clave=valor`. Las claves disponibles son:

- `pop_size`: Tamaño de la población (ej: 50).
- `num_generations`: Número máximo de generaciones (ej: 500).
- `mut_rate_fac`: Tasa de mutación para las instalaciones (ej: 0.1).
- `mut_rate_cli`: Tasa de mutación para los clientes (ej: 0.05).
- `crossover_rate`: Tasa de cruzamiento (ej: 0.8).
- `tournament_k`: Tamaño del torneo para la selección (ej: 3).
- `max_generations_without_improvement`: Umbral para el criterio de parada de purga (ej: 50).
- `seeds`: Lista de semillas separadas por coma para ejecución concurrente (ej: 1234,5678,91011).
- `facility_priority_init_prob`: Probabilidad de inicializar una instalación con prioridad alta (ej: 0.1).
- `shuffle_prob_pct`: Porcentaje de inyección de ruido (Shuffle) en el decodificador (ej: 30).
- `shuffle_window`: Tamaño de la ventana para el operador Shuffle (ej: 3).
- `max_init_attempts`: Intentos máximos de generación aleatoria factible (ej: 1000).
- `num_threads`: Número de hilos simultáneos a nivel de sistema operativo para procesar semillas (ej: 5).
- `target_instances`: Lista de instancias a procesar separadas por coma (ej: wlp01,wlp30).

Ejemplo de `config.txt`:
```ini
pop_size=200
num_generations=125
mut_rate_fac=0.1
mut_rate_cli=0.05
crossover_rate=0.8
tournament_k=3
max_generations_without_improvement=50
seeds=1234,5678,91011,121314,151617
facility_priority_init_prob=0.10
shuffle_prob_pct=30
shuffle_window=3
max_init_attempts=1000
num_threads=5
target_instances=wlp01,wlp04,wlp05,wlp07,wlp10,wlp30
```

## Estructura del Código
- `cflp_ci.cpp`: Archivo principal con la lógica central del Algoritmo Genético.
- `Makefile`: Script de compilación.
- `config.txt`: Configuración principal.
- `instancias/`: Directorio con los archivos de entrada `.in` del problema CFLP-CI.
- `run_experiments.py`: Script de automatización de experimentos (Python 3).
