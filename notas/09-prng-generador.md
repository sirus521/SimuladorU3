---
tipo: prng
proyecto: Simulador de Taller de Reparación
estado: aprobado
---

# Generador de números pseudoaleatorios

> **Nota de la asignatura.** Especificación del generador que usa el motor.
> La demostración matemática original (pruebas de periodo, retícula 2D,
> histogramas) está en el documento de la unidad anterior; aquí queda la
> versión que **el código implementa**, con los valores esperados para las
> pruebas.

## 1. Qué es un PRNG

Un generador de números pseudoaleatorios produce una secuencia que, aunque
determinista, imita la aleatoriedad. La diferencia con un generador real es que
todo depende de un único número: la **semilla**. Con la misma semilla, la
misma secuencia. Siempre.

## 2. La semilla

- Rango: `x0 ∈ [1, 65535]` (16 bits sin signo).
- Semillas de prueba: **1** y **2026**.
- Sobre `x0 = 0`: el diseño **no** lo excluye. Como `gcd(c, m) = 1`, la
  transición es una permutación de `{0..65535}` y todos los estados están en un
  mismo ciclo, así que el periodo también es 65536. La convención `x0 ∈ [1,
  65535]` se adopta porque `0` suele confundirse con "generador sin
  inicializar", no por una limitación real.

## 3. Restricciones de Hull-Dobell

Para que un LCG `x(n+1) = (a·x(n) + c) mod m` tenga periodo máximo `m`:

| Condición | Verificación | Resultado |
|---|---|---|
| 1. `gcd(c, m) = 1` | `c = 12345`, `m = 2^16 = 65536`, `gcd(12345, 65536) = 1` | Cumple |
| 2. `a - 1` divisible por todos los factores primos de `m` | único primo de `m` es 2; `a - 1 = 25204`, par | Cumple |
| 3. Si `4 | m`, entonces `4 | (a - 1)` | `4 | 65536` y `25204 mod 4 = 0` | Cumple |

**Conclusión:** con `a = 25205`, `c = 12345`, `m = 65536` el periodo máximo es
65536.

## 4. Fórmula del estado

```
x(n+1) = (25205 · x(n) + 12345) mod 65536
```

## 5. Etapa de mezcla

El LCG puro tiene correlación entre valores consecutivos (retícula 2D), sobre
todo al truncar bytes bajos. Se rompe con una mezcla **biyectiva** tipo xorshift
de 16 bits aplicada **solo a la salida**, nunca al estado:

```
z ← z XOR ((z << 7) AND 0xFFFF)
z ← z XOR (z >> 9)
z ← z XOR ((z << 8) AND 0xFFFF)
```

La mezcla es una permutación de `{0..65535}`, así que **no acorta el periodo**:
sigue siendo 65536.

Efecto medido sobre el ciclo completo:

| Métrica | LCG crudo | Con mezcla |
|---|---|---|
| Celdas 2D visitadas (pares de bytes altos) | 6912 / 65536 | **41654 / 65536** |
| Celdas 2D visitadas (pares de 16 bits) | — | **65536 / 65536** |
| Apariciones de cada byte alto | — | **256 cada uno** (uniformidad exacta) |

## 6. Mapeo a dado (método de multiplicación con hueco)

Para obtener enteros uniformes en `{1..6}` sin el sesgo del truncamiento
directo, se usa el método de multiplicación con hueco sobre enteros de 16 bits:

```
r = z · 6
si (r mod 65536) < 4  →  rechazar z y repetir
d = 1 + (r >> 16)
```

El hueco son 4 valores: `65535·6 = 393210 = 6·65536 − 6`, y `65536 = 6·10922 +
4`. Sobre el ciclo completo se aceptan **65532** valores (65536 − 4 rechazos) y
cada cara aparece **10922** veces: uniformidad exacta.

## 7. Pseudocódigo

```
x ← x0
bucle:
    x ← (25205 · x + 12345) mod 65536     ← el estado avanza siempre
    z ← mezcla(x)                          ← la mezcla es solo de salida
    r ← z · 6
    si (r mod 65536) < 4:
        rechazar y continuar
    devolver d = 1 + (r >> 16)
```

## 8. Casos áureos (los usa `tests/test_prng.c`)

### Semilla 1

| i | x(i-1) | x(i) | z | r = z·6 | r mod 65536 | dado |
|---|---|---|---|---|---|---|
| 1 | 1 | 37550 | 2508 | 15048 | 15048 | **1** |
| 2 | 37550 | 54719 | 12346 | 74076 | 8540 | **2** |
| 3 | 54719 | 65156 | 26330 | 157980 | 26908 | **3** |
| 4 | 65156 | 2701 | 26411 | 158466 | 27394 | **3** |
| 5 | 2701 | 64682 | 22526 | 135156 | 4084 | **3** |
| 6 | 64682 | 48619 | 1871 | 11226 | 11226 | **1** |
| 7 | 48619 | 62112 | 21489 | 128934 | 63398 | **2** |
| 8 | 62112 | 21337 | 22950 | 137700 | 6628 | **3** |
| 9 | 21337 | 23014 | 6579 | 39474 | 39474 | **1** |
| 10 | 23014 | 21079 | 37611 | 225666 | 29058 | **4** |
| 11 | 21079 | 8188 | 28044 | 168264 | 37192 | **3** |
| 12 | 8188 | 18021 | 43999 | 263994 | 1850 | **5** |

### Semilla 2026

| i | x(i-1) | x(i) | z | r = z·6 | r mod 65536 | dado |
|---|---|---|---|---|---|---|
| 1 | 2026 | 25131 | 59280 | 355680 | 28000 | **6** |
| 2 | 25131 | 33760 | 43737 | 262422 | 278 | **5** |
| 3 | 33760 | 13721 | 40037 | 240222 | 43614 | **4** |
| 4 | 13721 | 16678 | 40271 | 241626 | 45018 | **4** |
| 5 | 16678 | 33431 | 47731 | 286386 | 24242 | **5** |
| 6 | 33431 | 44348 | 5669 | 34014 | 34014 | **1** |
| 7 | 44348 | 21669 | 8230 | 49380 | 49380 | **1** |
| 8 | 21669 | 2466 | 5838 | 35028 | 35028 | **1** |
| 9 | 2466 | 39747 | 58590 | 351540 | 23860 | **6** |
| 10 | 39747 | 52184 | 60619 | 363714 | 36034 | **6** |
| 11 | 52184 | 2545 | 63497 | 380982 | 53302 | **6** |
| 12 | 2545 | 64862 | 9591 | 57546 | 57546 | **1** |

### Sobre el ciclo completo (65536 iteraciones)

| Propiedad | Valor esperado |
|---|---|
| Estados distintos | 65536 / 65536 |
| Salidas mezcladas distintas | 65536 / 65536 |
| Byte alto: apariciones por valor | 256 cada uno |
| Celdas 2D (bytes altos) | 41654 / 65536 |
| Celdas 2D (16 bits) | 65536 / 65536 |
| Caras de dado 1..6 | 10922 cada una |
| Valores aceptados | 65532 |
| Rechazos | 4 (0.0061 %) |

### Histograma de 60 000 muestras

| Semilla | 1 | 2 | 3 | 4 | 5 | 6 | Rechazos |
|---|---|---|---|---|---|---|---|
| 1 | 10014 | 9992 | 9978 | 10031 | 9996 | 9989 | 3 |
| 2026 | 9992 | 9977 | 9994 | 9986 | 10026 | 10025 | 4 |

## 9. Uniforme continuo

Las distribuciones continuas del modelo ([[03-modelo-matematico]]) necesitan un
uniforme en `[0,1)`. Se obtiene de la salida mezclada:

```
u = (z + 0.5) / 65536
```

El `+0.5` evita que `u` sea exactamente `0`, lo que rompería el logaritmo de la
inversa de la exponencial.

> **Nota sobre el periodo.** El LCG de 16 bits tiene periodo 65536. En una
> réplica de 30 días con 24 llegadas/día se consumen del orden de miles de
> muestras, muy por debajo del periodo, así que no hay ciclos cortos que
> Sesgoen el resultado. Aun así, el motor comprueba que ninguna réplica supere
> un cuarto del periodo, y avisa si lo hace.

## Ver también

- [[03-modelo-matematico]] — cómo se usa el generador en el modelo
- [[05-plan-de-trabajo]] — la fase F1 y su criterio de aceptación
