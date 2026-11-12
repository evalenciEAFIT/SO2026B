# Evidencia para la Parte II — verificación crítica de la IA

Errores e imprecisiones de la IA (Claude) detectados durante el desarrollo, cómo se detectaron y cómo se corrigieron. Las secciones A–D son casos **reales** de esta conversación. La sección E lista las sugerencias típicas de LLMs que se verificaron y se descartaron.

**Procedimiento de validación usado:**

1. Compilar contra los headers reales (`make check` en Ubuntu 6.8.0-149-generic; antes, contra el árbol de Linux v6.8 con `W=1`).
2. Verificar que **cada símbolo externo** del `.o` esté exportado: `nm -u secure_mmap.o` → buscar `EXPORT_SYMBOL(_GPL)` en las fuentes 6.8.
3. Probar en la VM con snapshot: `insmod`, `dmesg`, batería de ataques y control (Paso 6).

---

## A. Función inexistente (alucinación de API)

- **Qué generó la IA:** `crypto_tfm_alg_driver_name(crypto_sync_skcipher_tfm(tfm))`.
- **Cómo se detectó:** error de compilación `implicit declaration of function 'crypto_sync_skcipher_tfm'`.
- **Verificación:** en `include/crypto/skcipher.h` (6.8), `struct crypto_sync_skcipher { struct crypto_skcipher base; }` y existe `crypto_skcipher_driver_name()`.
- **Corrección:** `crypto_skcipher_driver_name(&tfm->base)`.

## B. Colisión con un símbolo del kernel

- **Qué generó la IA:** un parámetro de módulo `static bool enable_kprobe`.
- **Cómo se detectó:** `error: 'enable_kprobe' redeclared as different kind of symbol`. `linux/kprobes.h` ya declara `int enable_kprobe(struct kprobe *)`.
- **Corrección:** se renombró a `use_kprobe`.
- **Lección:** el kernel es un único espacio de nombres global en C; los nombres genéricos chocan.

## C. Suposición de entorno incorrecta

- **Qué dijo la IA:** que la ISO Ubuntu 24.04.4 instalaría un kernel 6.x.
- **Realidad:** `uname -r` → `7.0.0-43-generic`. El instalador actualizó al kernel HWE más reciente.
- **Corrección:** se instaló `linux-image-generic` (6.8, kernel GA de 24.04) y se fijó en GRUB (`GRUB_DEFAULT=saved`), porque el enunciado exige kernel 5.x/6.x.


## D. Decisión de la IA que compila y funciona, pero es lenta (detectada midiendo)

- **Qué generó la IA:** `crypto_alloc_sync_skcipher("ctr(aes)")` con la solicitud en la pila (`SYNC_SKCIPHER_REQUEST_ON_STACK`). Es correcto y pasó las 18 pruebas.
- **Cómo se detectó:** `make bench` midió ~34 µs por page fault (`/proc/secure_mmap`) contra ~0.65 µs por página de un archivo normal. En ftrace (`function_graph`), `smm_vault_decrypt_page` se lleva ~97 % de `smm_fault`. El `dmesg` mostraba `ctr(aes-aesni)`.
- **Causa:** en 6.8 la implementación rápida `ctr-aes-aesni` (AES-NI sobre la página completa) está registrada como **asíncrona** (envoltorio SIMD). La API *sync* la excluye, así que el kernel compone la plantilla genérica `ctr(...)` sobre el cifrador de **un bloque** `aes-aesni`: 256 llamadas por página, cada una con `kernel_fpu_begin/end`. Dentro de una VM sobre Hyper-V, guardar y restaurar la FPU es especialmente caro.
- **Corrección:** `crypto_alloc_skcipher()` sin máscara de sincronía, `crypto_wait_req()` por si la respuesta es asíncrona, y una única `skcipher_request` preasignada y protegida por `vault_lock` (sin `kmalloc` en el page fault).
- **Lección:** que compile y pase las pruebas no basta. La verificación de la IA también debe incluir medir.

---

## E. Sugerencias típicas de LLMs verificadas y descartadas

| Sugerencia | Por qué es incorrecta o insegura | Qué se hizo |
|---|---|---|
| `vma->vm_flags \|= VM_IO;` | Desde 6.3 `vm_flags` es `const`; hay que usar `vm_flags_set()`/`vm_flags_clear()` (toman el lock por-VMA). | Wrapper `smm_vm_flags_set()` con `LINUX_VERSION_CODE` en `smm.h`. |
| `class_create(THIS_MODULE, "x")` | En 6.4 cambió a `class_create("x")`. | `#if` por versión en `smm_init()`. |
| Hook en `sys_call_table` usando `kallsyms_lookup_name()` y desactivando el bit WP de CR0 (`write_cr0(read_cr0() & ~0x10000)`) | `kallsyms_lookup_name` dejó de exportarse en 5.7. Apagar WP es global para la CPU (otra CPU/IRQ escribe sin protección) y no tiene control de concurrencia. Desde 5.3 `native_write_cr0()` vuelve a fijar el bit WP (*CR pinning*) y avisa. | Se usa `file_operations.mmap` + `vm_ops->fault` (interfaz oficial). |
| "Registrar un LSM desde un módulo con `register_security()`" | Los LSM no son cargables desde 2.6.24; `security_add_hooks` es `__init` y no se exporta. | kprobe en `security_mmap_file` (solo auditoría) y BPF-LSM como trabajo futuro. |
| `copy_from_user()` dentro de la sección con `vault_lock` tomado | Si el buffer de usuario es la propia región mmap, el page fault vuelve a pedir el mutex → **deadlock**. | Copiar primero a un buffer de kernel y tomar el lock después (`smm_write`, `smm_vault_update`). |
| AES-CTR con IV = número de página fijo | Re-cifrar la misma página con el mismo IV reutiliza el flujo de llave (filtra P1⊕P2). | IV = índice \| generación \| contador; la generación aumenta en cada re-cifrado. |
| `remap_pfn_range()` de toda la bóveda en `mmap()` | Mapea todo de una vez (sin paginación por demanda) y expone una única copia física compartida del texto plano. | Página propia por mapeo, descifrada en `.fault`. |
| Bloquear desde un kprobe cambiando `regs->ax` | Un `pre_handler` no puede cambiar el retorno de la función sondeada (requiere `ALLOW_ERROR_INJECTION` + `bpf_override_return`). | Solo auditoría en el kprobe; bloqueo en `smm_mmap`. |

## Tabla de prompts (plantilla para completar en el informe)

| # | Rol asignado | Contexto | Restricción | Resultado esperado |
|---|---|---|---|---|
| 1 | Arquitecto de kernel Linux | Enunciado + repo CapaKernel (Paso 6) | Kernel 6.8, sin recompilar el kernel, sin LSM | Arquitectura y puntos de intercepción |
| 2 | Desarrollador de drivers | `vm_operations_struct`, `vm_flags` en 6.8 | Solo símbolos exportados; compilar sin warnings | Código del módulo |
| 3 | Pentester | Dirty COW, `madvise`, `/proc/self/mem`, LD_PRELOAD | PoC controlados, sin dañar el sistema | Scripts de ataque |
| 4 | Ingeniero de rendimiento | `perf`, `strace`, `ftrace` | Comparar contra línea base sin cifrado | Plan de medición |
