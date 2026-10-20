# Makefile — Simulador de Taller de Reparación
#
#   make            compila todo
#   make test       corre las pruebas (puerta de calidad de cada fase)
#   make bin/simulador   compila el CLI
#   make bin/simulador-web  compila el servidor HTTP
#   make web        instala dependencias y compila la interfaz
#   make run        levanta el servidor con la interfaz ya compilada
#   make dev        levanta el servidor C (para desarrollo, puerto 8080)
#   make clean      borra los artefactos

CC      ?= gcc
CFLAGS  ?= -std=c17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Iinclude -Itercer-partes
LDFLAGS ?=
LDLIBS  ?= -lm

BUILD   := build
BIN     := bin

# cJSON es código de terceros: se compila sin los warnings estrictos del
# proyecto, porque no es nuestro código y no lo vamos a arreglar.
CJSON_SRC := tercer-partes/cjson/cJSON.c
CJSON_OBJ := $(BUILD)/cJSON.o

MOTOR_SRC := src/prng.c src/distrib.c src/analitico.c src/config.c \
             src/modelo.c src/agregado.c
MOTOR_OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(MOTOR_SRC))

CLI_OBJ    := $(BUILD)/cli_main.o
SERVIDOR_OBJ := $(BUILD)/http.o $(BUILD)/rutas.o $(BUILD)/servidor_main.o

BIN_SIMULADOR := $(BIN)/simulador
BIN_SERVIDOR  := $(BIN)/simulador-web

.PHONY: all test web run dev clean instalar-web

all: $(BIN_SIMULADOR) $(BIN_SERVIDOR)

$(BUILD) $(BIN):
	@mkdir -p $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(CJSON_OBJ): $(CJSON_SRC) | $(BUILD)
	$(CC) -std=c17 -O2 -Itercer-partes -c $< -o $@

$(BUILD)/cli_main.o: cli/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/http.o: servidor/http.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/rutas.o: servidor/rutas.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/servidor_main.o: servidor/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_SIMULADOR): $(MOTOR_OBJ) $(CLI_OBJ) $(CJSON_OBJ) | $(BIN)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_SERVIDOR): $(MOTOR_OBJ) $(SERVIDOR_OBJ) $(CJSON_OBJ) | $(BIN)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# --- Pruebas ---------------------------------------------------------

TEST_SRC := $(wildcard tests/test_*.c)
TEST_BIN := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRC))

# La prueba del servidor necesita el binario ya compilado, porque lo
# levanta como proceso hijo.
$(BUILD)/test_%: tests/test_%.c $(MOTOR_OBJ) $(CJSON_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $< $(MOTOR_OBJ) $(CJSON_OBJ) -o $@ $(LDLIBS)

test: all $(TEST_BIN)
	@echo "== Pruebas del simulador =="
	@@fail=0; for t in $(TEST_BIN); do \
		echo "-- $$t"; \
		if ! $$t; then fail=1; fi; \
	done; \
	if [ $$fail -ne 0 ]; then echo "== HAY PRUEBAS FALLIDAS =="; exit 1; fi; \
	echo "== Todas las pruebas en C pasaron =="

# Pruebas de la interfaz (node:test, sin dependencias extra).
test-web:
	cd web && npm test

# --- Interfaz web ---------------------------------------------------

instalar-web:
	cd web && npm install

web: instalar-web
	cd web && npm run build

run: all web
	$(BIN_SERVIDOR) --raiz-web web/dist --puerto 8080

dev: all
	$(BIN_SERVIDOR) --raiz-web web/dist --puerto 8080

clean:
	rm -rf $(BUILD) $(BIN)
