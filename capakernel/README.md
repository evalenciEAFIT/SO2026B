# Parcial 3 — `secure_mmap`: seguridad de archivos mapeados con `mmap()` en la capa kernel

**Sistemas Operativos 2026-2 — Universidad EAFIT**
Referencia base: [SO2026B / USOS / CapaKernel](https://github.com/evalenciEAFIT/SO2026B/tree/main/USOS/CapaKernel), Paso 6 (`vm_operations_struct->fault`).
Probado en: Ubuntu 24.04 LTS, kernel **6.8.0-149-generic** (VirtualBox).

---

## 1. Qué hace

`secure_mmap.ko` crea `/dev/secure_mmap`: un **archivo confidencial que vive cifrado en memoria del kernel** y que solo un UID autorizado puede leer mediante `mmap()` (o `read()`). Toda la defensa ocurre en Ring 0, en los tres puntos que señala el enunciado:

| Punto de intervención | Dónde | Qué hace |
|---|---|---|
| **Alta del VMA** | `smm_mmap()` (`file_operations.mmap`) | Verifica UID, rango, `PROT_EXEC`, `MAP_PRIVATE+PROT_WRITE`, `MAP_SHARED+PROT_WRITE`. Retira `VM_MAYWRITE`/`VM_MAYEXEC` y añade `VM_IO`, `VM_DONTCOPY`, `VM_DONTDUMP`, `VM_DONTEXPAND`. |
| **Page fault** | `smm_fault()` (`vm_ops->fault`, invocado desde `handle_mm_fault`) | Descifra **solo** la página pedida (AES-256-CTR) en una página nueva **propia del mapeo** y verifica su CRC32; si fue manipulada → `SIGBUS`. |
| **Cierre / sincronización** | `smm_vma_close()` → `smm_map_release()` | Si la página cambió (MAP_SHARED escribible), la re-cifra con un IV nuevo; luego borra el texto plano (`memzero_explicit`) antes de liberarla. |
| **Archivos reales** | kprobe en `security_mmap_file` (el hook de los LSM) | Audita cada `mmap()` de un archivo vigilado (`watch_name`, por defecto `secreto.txt`) y alerta si es `MAP_SHARED+PROT_WRITE` o `PROT_EXEC`. |

Todo queda registrado en `/proc/secure_mmap` (contadores, latencia de `smm_fault`, últimos 32 eventos) y en `dmesg`.

## 2. Arquitectura

```
 Espacio de usuario (Ring 3)                         Espacio de kernel (Ring 0)
 ─────────────────────────────                       ─────────────────────────────────────────────
 p = mmap(fd /dev/secure_mmap) ── sys_mmap ──► security_mmap_file ◄── [kprobe: auditoría archivos reales]
                                                  │
                                                  ▼
                                             smm_mmap()  ── política: UID, W^X, no-COW, flags VMA
                                                  │ OK → vm_ops = smm_vm_ops (sin PTEs todavía)
 c = p[i]  ── MMU: PTE ausente → #PF ──► handle_mm_fault → smm_fault()
                                                  │  alloc_page → AES-CTR descifrar → CRC32 ok?
                                                  │     no → SIGBUS + evento "INTEGRIDAD VIOLADA"
                                                  ▼     sí → vmf->page (página privada del mapeo)
 accesos siguientes: directos a RAM (zero-copy, sin syscalls)
 munmap()/exit ──────────────────────────► smm_vma_close → re-cifrar si cambió → memzero → liberar
                                                  │
                                       ┌──────────┴───────────┐
                                       │  Bóveda (smm_vault)   │  páginas SIEMPRE cifradas
                                       │  llave AES-256 aleatoria, IV = idx|gen|ctr, CRC32
                                       └───────────────────────┘
```

Archivos:

```
capakernel/
├── Kbuild / Makefile          módulo + herramientas; `make check` = build limpio verificado
├── include/smm.h              interfaces comunes y compatibilidad 5.x/6.x
├── src/smm_main.c             char device, política de mmap, vm_ops (.fault/.close/.may_split)
├── src/smm_vault.c            bóveda cifrada (crypto API del kernel), integridad, re-cifrado
├── src/smm_audit.c            contadores atómicos, bitácora circular, /proc/secure_mmap
├── src/smm_kprobe.c           kprobe en security_mmap_file (auditoría de archivos reales)
├── tests/smm_attacks.c        17 PoC controlados (ver tabla 4)
├── tests/guard_preload.c      "defensa" LD_PRELOAD en usuario (contraejemplo evadible)
├── tests/smm_bench.c          medición mmap()+faults vs read()
├── scripts/run_attacks.sh     batería completa con veredicto PASA/FALLA
├── scripts/run_baseline.sh    los mismos ataques contra el Paso 6 del profesor (sin defensa)
├── scripts/run_bench.sh       perf stat, strace -c/-T, ftrace, latencia interna
└── docs/IA_alucinaciones.md   evidencia de verificación crítica de la IA (Parte II)
```

## 3. Compilar y ejecutar

Requisitos (Ubuntu 24.04, kernel 6.8): `sudo apt install build-essential linux-headers-$(uname -r) linux-tools-$(uname -r) strace`

```bash
cd capakernel
make check            # compila desde cero y falla si hay CUALQUIER warning
make attacks          # = sudo ./scripts/run_attacks.sh   (resultados en results/)
make baseline         # = sudo ./scripts/run_baseline.sh  (control sin defensa)
make bench            # = sudo ./scripts/run_bench.sh     (mediciones)
cat /proc/secure_mmap # estado y bitácora
make unload
```

Carga manual con parámetros:

```bash
sudo ./scripts/load.sh                     # allowed_uid = tu UID, solo lectura
sudo ./scripts/load.sh allow_write=1       # permite MAP_SHARED|PROT_WRITE y write()
sudo ./scripts/load.sh vault_pages=4096    # bóveda de 16 MiB
echo 1 | sudo tee /sys/module/secure_mmap/parameters/verbose   # registrar cada fault
```

| Parámetro | Defecto | Descripción |
|---|---|---|
| `allowed_uid` | 1000 | Único UID autorizado (los scripts usan el tuyo) |
| `allow_write` | 0 | Permite `MAP_SHARED+PROT_WRITE` y `write()` |
| `vault_pages` | 256 | Tamaño de la bóveda (páginas de 4 KiB) |
| `verbose` | 0 | Registra cada page fault en la bitácora |
| `watch_name` | secreto.txt | Archivo real vigilado por el kprobe |
| `use_kprobe` | 1 | Instalar el kprobe en `security_mmap_file` |

## 4. Ataques simulados

| ID | Ataque | Defensa que lo detiene | Sin secure_mmap (Paso 6) |
|---|---|---|---|
| T02/T03 | Usuario no autorizado hace `mmap()`/`read()` | UID verificado en `smm_mmap` aunque el nodo sea 0666 | éxito |
| T04 | Defensa LD_PRELOAD evadida con `syscall(SYS_mmap)` | *(se espera VULNERABLE: demuestra que usuario no basta)* | — |
| T05 | Misma evasión contra `/dev/secure_mmap` | La política está en el kernel | — |
| T06 | `MAP_SHARED+PROT_WRITE` | `allow_write=0` | — |
| T07 | `MAP_PRIVATE+PROT_WRITE` (COW) | Prohibido | éxito |
| T08 | `PROT_EXEC` | W^X, retira `VM_MAYEXEC` | éxito |
| T09/T10 | `mprotect()` RO→RW / RO→RX | `VM_MAYWRITE`/`VM_MAYEXEC` retirados → `EACCES` | éxito |
| T11 | Estilo Dirty COW: `/proc/self/mem` + `madvise(DONTNEED)` | Sin `VM_MAYWRITE` no hay COW forzado; `VM_IO` bloquea GUP | escritura forzada |
| T12 | Otro proceso lee el texto plano (`process_vm_readv`, `/proc/pid/mem`) | `VM_IO` → `get_user_pages()` falla | éxito |
| T13 | Heredar el mapeo con `fork()` | `VM_DONTCOPY` | éxito |
| T14 | Partir/crecer el VMA | `.may_split` → `EINVAL`, `VM_DONTEXPAND` | éxito |
| T15 | Manipular 1 byte del texto cifrado | CRC32 por página → `SIGBUS` | — |
| T16 | Carrera `madvise(DONTNEED)` vs escrituras | Página por mapeo persistente + re-cifrado al cerrar | — |
| T17 | `mmap(RW, MAP_SHARED)` de un archivo real | kprobe en `security_mmap_file` → ALERTA | — |
| T18 | Pregunta 8: `O_RDONLY` + `PROT_WRITE` + `MAP_PRIVATE` | Informativo (el kernel lo permite: COW privado) | — |

## 5. Decisiones de diseño (resumen para la sustentación)

- **¿Por qué un driver con `.fault` y no un hook a la `sys_call_table`?** Desde 5.7 `kallsyms_lookup_name` no está exportada y la tabla es de solo lectura (bit WP de CR0); parchearla es frágil, inseguro en SMP y lo bloquea CET/IBT. El `.fault` es la interfaz oficial para servir páginas.
- **¿Por qué no un LSM?** Los LSM no se pueden cargar como módulo desde Linux 2.6.24. Para archivos reales usamos un **kprobe** en el mismo hook (`security_mmap_file`) para auditar; bloquear requeriría **BPF-LSM** (trabajo futuro).
- **Texto plano por mapeo:** dos procesos autorizados no comparten la misma página descifrada, y ninguna queda en el page cache global.
- **Sin deadlocks:** `read()`/`write()` copian desde/hacia el usuario **sin** tener tomado `vault_lock`, porque el buffer de usuario podría ser la propia región mapeada (el page fault volvería a pedir el lock).
- **IV con generación:** cada re-cifrado incrementa `gen`, así AES-CTR nunca reutiliza el flujo de llave.

## 6. Limitaciones y trabajo futuro

- **root** puede descargar el módulo o leer la memoria física (`/dev/mem`, kdump). Mitigación: Secure Boot + *lockdown* (`lockdown=confidentiality`), firma de módulos.
- **CRC32 no es un MAC**: detecta manipulación accidental o ingenua, no a un atacante que pueda reescribir también el CRC. Futuro: AES-GCM (AEAD) o HMAC-SHA256.
- El texto plano existe en RAM mientras está mapeado: un ataque de **cold boot**, DMA o un hipervisor malicioso podría leerlo. Mitigación de hardware: **AMD SEV / Intel TDX** (memoria cifrada por VM), **Intel TME/MKTME**, IOMMU.
- **Canales laterales** (Spectre/Meltdown, Rowhammer): fuera del alcance; mitigación con KPTI, microcódigo, ECC/TRR.
- El kprobe **solo audita** archivos reales; bloquear requiere BPF-LSM (`lsm=...,bpf`) o un LSM compilado en el kernel.
- La llave vive solo en RAM: al descargar el módulo el contenido se pierde (no hay persistencia en disco). Futuro: sellar la llave con TPM.
