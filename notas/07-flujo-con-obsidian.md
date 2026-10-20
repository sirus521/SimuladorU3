---
tipo: flujo
proyecto: Simulador de Taller de Reparación
---

# Cómo se trabaja este proyecto con Obsidian

## La idea

Este repo **es** un vault de Obsidian. Las notas de `notas/` son markdown
normal, se editan en Obsidian y también en cualquier editor, y se versionan en
git junto con el código.

Que las notas estén en el repo y no en una carpeta personal es deliberado: el
reporte que hay que entregar se construye con estas notas, y tiene que quedar
en el mismo control de versiones que el código que lo genera. Si las notas
vivieran aparte, el reporte y el código se desincronizan sin que nadie lo note.

## Abrir el proyecto en Obsidian

### Opción A — desde el vault personal (ya configurada)

El vault principal está en `~/Documents/Obsidian Vault`. El proyecto ya está
enlazado desde ahí:

```
~/Documents/Obsidian Vault/TecNM/SimuladorU3
    -> ~/Workspace/TecNM/Workspace/Simulacion/SimuladorU3
```

Las notas aparecen dentro de la carpeta **TecNM**, junto a las de las otras
materias, y se pueden enlazar entre ellas con `[[...]]`.

**Para recrear el enlace** (por ejemplo, en otra máquina):

```bash
ln -s ~/Workspace/TecNM/Workspace/Simulacion/SimuladorU3 \
      ~/Documents/Obsidian\ Vault/TecNM/SimuladorU3
```

**Atención:** con esta opción, si además se abre el repo como vault (opción
B), Obsidian ve los mismos archivos dos veces y los duplica en el índice. Usa
**una de las dos**, no las dos.

### Opción B — el repo como vault

1. En Obsidian: **Abrir carpeta como vault**.
2. Elegir la carpeta `SimuladorU3/`.

Resultado: las notas aparecen en la raíz del vault. Es la opción útil si
trabajas con el repo aislado y no quieres ver el resto de tus notas.

## Convención de nombres

```
notas/00-index.md                  índice y mapa del proyecto
notas/01-definicion-del-simulador.md
notas/02-modelo-del-negocio.md
notas/03-modelo-matematico.md
notas/04-parte-visual.md
notas/05-plan-de-trabajo.md
notas/06-bitacora-y-decisiones.md
notas/07-flujo-con-obsidian.md
notas/08-reporte.md
notas/09-prng-generador.md
```

El número fija el **orden de lectura**. Los archivos se pueden renombrar, pero
hay que actualizar los `[[enlaces]]` correspondientes.

## Frontmatter

Todas las notas llevan frontmatter al principio, con las mismas claves:

```yaml
---
tipo: modelo-negocio
proyecto: Simulador de Taller de Reparación
estado: aprobado
---
```

| Clave | Valores | Para qué |
|---|---|---|
| `tipo` | `indice`, `definicion`, `modelo-negocio`, `modelo-matematico`, `parte-visual`, `plan`, `bitacora`, `flujo`, `reporte`, `prng` | Clasifica la nota |
| `proyecto` | siempre el mismo texto | Para buscar notas del proyecto |
| `estado` | `borrador`, `en-revision`, `aprobado` | Marca si la nota ya se puede citar en el reporte |

## Enlaces

- Entre notas: `[[02-modelo-del-negocio]]` (con el prefijo numérico, para que
  se ordenen en el panel de enlaces).
- Hacia código: `src/modelo.c` en backticks, o un enlace markdown relativo
  `[modelo.c](../src/modelo.c)`.
- Hacia un ADR: `[[06-bitacora-y-decisiones#ADR-003]]`.

## Regla de oro

> **Una nota responde una pregunta. Si una nota responde dos, se parte en dos.**

Esto es lo que hace que las notas sirvan de documento de Especificación y no
se vuelvan un documento gigante que nadie actualiza. Por eso el índice es
`00-index.md` y no un `README.md` dentro de las notas: el índice dice qué
pregunta contesta cada nota.

## Flujo de trabajo de una fase

1. **Antes de escribir código:** actualizar o crear la nota que describe lo que
   se va a hacer. Si el comportamiento no está en una nota, no se escribe.
2. **Durante:** la nota se corrige si al implementarlo se descubre que estaba
   mal. Se corrige la **nota**, y el código sigue a la nota.
3. **Al cerrar la fase:** la nota pasa a `estado: aprobado` y se hace el commit
   con el mensaje referenciando la nota.
4. **Al final:** `08-reporte.md` se arma con pedazos de las notas aprobadas, no
   con texto nuevo. Así el reporte y la especificación no pueden divergir.

## Lo que **no** se hace

- **No** se editan las notas desde la terminal si se está en Obsidian: se
  edita en Obsidian y se commitea. Editar el archivo por debajo de Obsidian
  puede dejar la vista del editor desactualizada.
- **No** se mete código dentro de las notas. Si hace falta un fragmento de
  código de ejemplo, va en bloque de código, y el código real vive en `src/`.
- **No** se borra el historial. Las notas se corrigen; el commit guarda lo que
  se pensaba antes, que a final de proyecto es evidencia de cómo se razona.
