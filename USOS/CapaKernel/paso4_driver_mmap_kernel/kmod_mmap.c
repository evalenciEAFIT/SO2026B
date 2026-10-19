/**
 * @file kmod_mmap.c
 * @brief PASO 4: Driver de Kernel optimizado implementando .mmap (Zero-Copy)
 * @course Sistemas Operativos - DOCENCIA 2026-B
 * 
 * ==============================================================================
 * GUÍA MAESTRA: CÓMO SE PROGRAMA EN LA CAPA KERNEL PARA EFICIENCIA CON MMAP
 * ==============================================================================
 * 
 * 1. EL PROBLEMA QUE QUEREMOS RESOLVER:
 *    En el Paso 3, vimos que dev_read() requiere copy_to_user(), lo cual obliga a
 *    la CPU a duplicar cada byte de memoria entre el kernel y el usuario.
 *    Para periféricos de alta velocidad (GPU, FPGA, tarjetas de red de 100Gbps,
 *    capturadoras de video 4K o IPC de ultra-baja latencia), este copiado satura
 *    el bus de memoria y degrada la latencia.
 * 
 * 2. LA SOLUCIÓN EN LA CAPA KERNEL:
 *    Implementar la función de callback .mmap en struct file_operations:
 * 
 *        static int dev_mmap(struct file *filp, struct vm_area_struct *vma);
 * 
 * 3. ¿QUÉ ES struct vm_area_struct (VMA)?
 *    Es la estructura fundamental del subsistema de memoria virtual de Linux.
 *    Representa un intervalo contiguo de memoria virtual en el proceso de usuario:
 *      - vma->vm_start : Dirección virtual inicial en el espacio del usuario.
 *      - vma->vm_end   : Dirección virtual final.
 *      - vma->vm_page_prot : Permisos de la página (Lectura/Escritura/Ejecución).
 *      - vma->vm_flags : Banderas del VMA (VM_SHARED, VM_READ, VM_WRITE, etc.).
 * 
 * 4. LA FUNCIÓN MÁGICA: remap_pfn_range()
 *    Prototipo del kernel:
 *        int remap_pfn_range(struct vm_area_struct *vma,
 *                            unsigned long addr,
 *                            unsigned long pfn,
 *                            unsigned long size,
 *                            pgprot_t prot);
 * 
 *    ¿Qué hace internamente?
 *    a) Recorre las tablas de páginas multinivel del proceso (PGD -> P4D -> PUD -> PMD -> PTE).
 *    b) Asigna las entradas PTE del usuario para que apunten DIRECTAMENTE al Page Frame
 *       Number (PFN) de las páginas físicas del kernel.
 *    c) A partir de ese instante:
 *       ¡LA MEMORIA DEL KERNEL Y LA MEMORIA DEL USUARIO SON FÍSICAMENTE LA MISMA!
 *       Cualquier lectura o escritura ocurre a velocidad pura de hardware RAM (Zero-Copy).
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>

#define DEVICE_NAME     "demo_mmap"
#define CLASS_NAME      "demo_mmap_class"

/*
 * Queremos 16 páginas de 4 KB = 64 KB de memoria contigua.
 * Como 2^4 = 16 páginas, el ORDER para alloc_pages es 4.
 */
#define PAGE_ORDER      4
#define ALLOC_PAGES_CNT (1 << PAGE_ORDER)
#define BUFFER_SIZE     (ALLOC_PAGES_CNT * PAGE_SIZE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Docencia SO 2026-B");
MODULE_DESCRIPTION("Driver de Kernel con soporte mmap Zero-Copy");
MODULE_VERSION("1.0");

static int major_number;
static struct class *demo_class = NULL;
static struct device *demo_device = NULL;
static struct cdev demo_cdev;

/* Estructura de página y puntero virtual del kernel */
static struct page *kernel_pages = NULL;
static char *kernel_buffer = NULL;

static int dev_open(struct inode *inodep, struct file *filep)
{
    pr_info("[kmod_mmap] Dispositivo abierto por proceso: %s (PID: %d)\n",
            current->comm, current->pid);
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    pr_info("[kmod_mmap] Dispositivo cerrado por proceso: %s (PID: %d)\n",
            current->comm, current->pid);
    return 0;
}

/**
 * dev_mmap - Callback invocado cuando el usuario llama a mmap(fd, ...)
 * @filp: Puntero al descriptor de archivo en el kernel
 * @vma:  Estructura de memoria virtual asignada al proceso
 */
static int dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret;

    pr_info("[kmod_mmap] Solicitud de mmap(): Rango virtual [0x%lx - 0x%lx], tamano: %lu bytes\n",
            vma->vm_start, vma->vm_end, size);

    /* 1. Validación de seguridad de límites */
    if (size > BUFFER_SIZE) {
        pr_err("[kmod_mmap] Error: El proceso solicito %lu bytes, pero el buffer es de %lu bytes\n",
               size, (unsigned long)BUFFER_SIZE);
        return -EINVAL;
    }

    /* 2. Obtener el Page Frame Number (PFN) de nuestras páginas físicas */
    pfn = page_to_pfn(kernel_pages);

    /*
     * 3. Configuración de atributos de memoria:
     *    Para memoria RAM normal compartida, conservamos la protección estándar
     *    con soporte para caché de CPU (L1/L2/L3).
     */
    vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);

    /*
     * 4. LA LLAMADA CLAVE EN EL KERNEL: remap_pfn_range()
     *    Parámetros:
     *      vma:            Área de memoria virtual del proceso destino
     *      vma->vm_start:  Dirección virtual en el espacio del usuario
     *      pfn:            Número de marco de página física del kernel
     *      size:           Tamaño a mapear
     *      vma->vm_page_prot: Permisos de acceso (R/W)
     */
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret < 0) {
        pr_err("[kmod_mmap] Fallo en remap_pfn_range: %d\n", ret);
        return -EAGAIN;
    }

    pr_info("[kmod_mmap] remap_pfn_range() exitoso: PFN 0x%lx mapeado a direccion virtual 0x%lx\n",
            pfn, vma->vm_start);

    return 0;
}

/**
 * Función auxiliar para verificar desde el kernel qué escribió el usuario en RAM
 */
static ssize_t dev_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    /*
     * Solo imprimimos en el log del kernel lo que hay en el buffer para
     * comprobar que el usuario modificó la memoria del kernel directamente.
     */
    if (*ppos > 0) return 0;
    pr_info("[kmod_mmap] Inspeccion del kernel: Primeros 64 bytes del buffer = '%.64s'\n",
            kernel_buffer);
    *ppos = 1;
    return 0;
}

/* Tabla de operaciones con .mmap registrado */
static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .mmap    = dev_mmap,   /* <-- ¡AQUÍ ESTÁ LA MAGIA DE ZERO-COPY! */
    .read    = dev_read,
};

static int __init demo_mmap_init(void)
{
    dev_t dev_num;
    int ret;
    int i;

    pr_info("[kmod_mmap] Inicializando driver optimizado con mmap...\n");

    /* 1. Reservar páginas contiguas en memoria física del Kernel */
    kernel_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO, PAGE_ORDER);
    if (!kernel_pages) {
        pr_err("[kmod_mmap] Error al reservar paginas con alloc_pages\n");
        return -ENOMEM;
    }

    /* Obtener la dirección virtual del kernel para acceder a las páginas */
    kernel_buffer = page_address(kernel_pages);

    /*
     * 2. Marcar las páginas como RESERVADAS en la capa kernel.
     *    Esto le indica al subsistema de memoria virtual de Linux que estas
     *    páginas están ancladas (pinned) y no deben ser enviadas a Swap.
     */
    for (i = 0; i < ALLOC_PAGES_CNT; i++) {
        SetPageReserved(kernel_pages + i);
    }

    /* Inicializar el contenido del búfer con un mensaje claro */
    snprintf(kernel_buffer, 256,
             "DATOS_DEL_KERNEL: Hola desde la memoria fisica del Kernel de Linux!\n"
             "Este mensaje fue leido mediante mmap() SIN NINGUNA LLAMADA A READ O COPY_TO_USER.\n");

    /* 3. Registrar el dispositivo de caracteres */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("[kmod_mmap] Error en alloc_chrdev_region\n");
        goto err_free_pages;
    }
    major_number = MAJOR(dev_num);

    cdev_init(&demo_cdev, &fops);
    demo_cdev.owner = THIS_MODULE;
    ret = cdev_add(&demo_cdev, dev_num, 1);
    if (ret < 0) goto err_unregister;

    demo_class = class_create(CLASS_NAME);
    if (IS_ERR(demo_class)) {
        ret = PTR_ERR(demo_class);
        goto err_cdev_del;
    }

    demo_device = device_create(demo_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(demo_device)) {
        ret = PTR_ERR(demo_device);
        goto err_class_destroy;
    }

    pr_info("[kmod_mmap] Dispositivo /dev/%s listo para mapeo de memoria en espacio de usuario\n",
            DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(demo_class);
err_cdev_del:
    cdev_del(&demo_cdev);
err_unregister:
    unregister_chrdev_region(dev_num, 1);
err_free_pages:
    for (i = 0; i < ALLOC_PAGES_CNT; i++) {
        ClearPageReserved(kernel_pages + i);
    }
    __free_pages(kernel_pages, PAGE_ORDER);
    return ret;
}

static void __exit demo_mmap_exit(void)
{
    dev_t dev_num = MKDEV(major_number, 0);
    int i;

    device_destroy(demo_class, dev_num);
    class_destroy(demo_class);
    cdev_del(&demo_cdev);
    unregister_chrdev_region(dev_num, 1);

    /* Desmarcar páginas reservadas y devolver la memoria al kernel */
    if (kernel_pages) {
        for (i = 0; i < ALLOC_PAGES_CNT; i++) {
            ClearPageReserved(kernel_pages + i);
        }
        __free_pages(kernel_pages, PAGE_ORDER);
        kernel_pages = NULL;
        kernel_buffer = NULL;
    }

    pr_info("[kmod_mmap] Modulo descargado y paginas fisicas liberadas.\n");
}

module_init(demo_mmap_init);
module_exit(demo_mmap_exit);
