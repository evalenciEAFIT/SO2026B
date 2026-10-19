/**
 * @file kmod_demand_paging.c
 * @brief PASO 6: Paginación por Demanda Avanzada en Kernel con vm_operations_struct
 * @course Sistemas Operativos - DOCENCIA 2026-B
 * 
 * ==============================================================================
 * ¿POR QUÉ ESTE PASO ES REVOLUCIONARIO EN LA ENSEÑANZA DE SISTEMAS OPERATIVOS?
 * ==============================================================================
 * 
 * En el Paso 4 usamos remap_pfn_range(), que mapea TODAS las páginas físicas del
 * búfer a la tabla de páginas del usuario de inmediato en el momento de la syscall.
 * 
 * Sin embargo, en sistemas reales (como controladores de GPU NVIDIA/AMD, subsistemas
 * de archivos o bases de datos como MongoDB/PostgreSQL):
 * - El búfer puede medir Gigabytes o Terabytes.
 * - Mapear todo por adelantado desperdicia memoria RAM en tablas de páginas y tiempo de CPU.
 * 
 * LA SOLUCIÓN DEL KERNEL: struct vm_operations_struct y el método .fault
 * ---------------------------------------------------------------------
 * 1. Cuando el usuario llama a mmap():
 *    - El driver NO mapea ninguna página física a la PTE.
 *    - Solo asigna el puntero de operaciones de memoria virtual:
 *          vma->vm_ops = &demo_vm_ops;
 *    - La syscall retorna casi instantáneamente (0 microsegundos).
 * 
 * 2. Cuando el proceso lee ptr[0] o ptr[16384]:
 *    - La MMU de la CPU intenta traducir la dirección y detecta que la entrada PTE está VACÍA.
 *    - La CPU suspende la instrucción y dispara la excepción de hardware Page Fault (#PF, Vector 14).
 *    - El manejador central del kernel (do_fault()) invoca nuestro callback:
 *          vm_fault_t demo_vma_fault(struct vm_fault *vmf);
 *    - Nuestro driver inspecciona qué página exacta fue solicitada (vmf->pgoff),
 *      asigna o localiza el marco físico (struct page), incrementa su contador
 *      de referencias con get_page(page), y lo asigna a vmf->page.
 *    - El kernel escribe la entrada PTE correspondiente y reanuda el proceso de usuario.
 * 
 * ¡Las páginas no accedidas NUNCA consumen memoria de tabla de páginas ni tiempo de CPU!
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>

#define DEVICE_NAME     "demo_demand_fault"
#define CLASS_NAME      "demo_demand_class"
#define NUM_PAGES       8   /* 8 páginas = 32 KB */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Docencia SO 2026-B");
MODULE_DESCRIPTION("Driver de Kernel con Fallos de Pagina bajo Demanda (vm_ops->fault)");
MODULE_VERSION("1.0");

static int major_number;
static struct class *demo_class = NULL;
static struct device *demo_device = NULL;
static struct cdev demo_cdev;

/* Arreglo de punteros a páginas físicas independientes */
static struct page *driver_pages[NUM_PAGES];

/**
 * demo_vma_fault - Invocado por el Kernel cuando la CPU sufre un Page Fault
 * @vmf: Estructura suministrada por Linux con los detalles del fallo
 */
static vm_fault_t demo_vma_fault(struct vm_fault *vmf)
{
    unsigned long offset = vmf->pgoff;
    struct page *page;

    pr_info("[kmod_demand_paging] >>> PAGE FAULT CAPTURADO EN KERNEL <<<\n");
    pr_info("  - Proceso causante  : %s (PID: %d)\n", current->comm, current->pid);
    pr_info("  - Direccion Virtual : 0x%lx\n", vmf->address);
    pr_info("  - Pagina (Offset)   : #%lu (de 0 a %d)\n", offset, NUM_PAGES - 1);

    if (offset >= NUM_PAGES) {
        pr_warn("[kmod_demand_paging] Acceso fuera de rango (%lu >= %d) -> SIGBUS\n",
                offset, NUM_PAGES);
        return VM_FAULT_SIGBUS;
    }

    /* Obtener la página física correspondiente */
    page = driver_pages[offset];
    if (!page)
        return VM_FAULT_SIGBUS;

    /*
     * REGLA DE ORO DEL KERNEL:
     * Incrementar el refcount de la página. El subsistema de memoria virtual de
     * Linux decrecerá este contador automáticamente cuando la página sea desmapeada.
     */
    get_page(page);
    vmf->page = page;

    pr_info("[kmod_demand_paging] Pagina fisica PFN 0x%lx vinculada exitosamente a la PTE!\n",
            page_to_pfn(page));

    return 0; /* Exito (Página asignada a la tabla de páginas del proceso) */
}

/* Operaciones de Memoria Virtual del VMA */
static const struct vm_operations_struct demo_vm_ops = {
    .fault = demo_vma_fault,
};

static int dev_open(struct inode *inodep, struct file *filep)
{
    pr_info("[kmod_demand_paging] Dispositivo abierto por PID %d\n", current->pid);
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    pr_info("[kmod_demand_paging] Dispositivo cerrado por PID %d\n", current->pid);
    return 0;
}

/**
 * dev_mmap - Callback invocado por sys_mmap
 */
static int dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;

    if (size > (NUM_PAGES * PAGE_SIZE)) {
        pr_err("[kmod_demand_paging] Tamano solicitado (%lu) excede limite (%lu)\n",
               size, (unsigned long)(NUM_PAGES * PAGE_SIZE));
        return -EINVAL;
    }

    /*
     * OBSERVACIÓN PEDAGÓGICA CRÍTICA:
     * ¡AQUÍ NO LLAMAMOS A remap_pfn_range()!
     * Solo asignamos la tabla de operaciones virtuales (vm_ops).
     * Las páginas se asignarán UNA A UNA en tiempo de ejecución
     * a medida que la CPU genere fallos de página (#PF).
     */
    vma->vm_ops = &demo_vm_ops;

    pr_info("[kmod_demand_paging] mmap() exitoso: vm_ops registrado. Ninguna pagina mapeada aun.\n");
    return 0;
}

static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .mmap    = dev_mmap,
};

static int __init demo_demand_init(void)
{
    dev_t dev_num;
    int ret, i;

    pr_info("[kmod_demand_paging] Inicializando driver de paginacion por demanda...\n");

    /* 1. Reservar las 8 páginas físicas individualmente e inicializar sus contenidos */
    for (i = 0; i < NUM_PAGES; i++) {
        driver_pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
        if (!driver_pages[i]) {
            pr_err("[kmod_demand_paging] Error alojando pagina %d\n", i);
            ret = -ENOMEM;
            goto err_free_pages;
        }

        /* Escribir un mensaje pedagógico distintivo en cada página */
        char *kaddr = page_address(driver_pages[i]);
        snprintf(kaddr, PAGE_SIZE,
                 "[PAGINA_FISICA_%d]: Hola! Esta es la pagina %d del kernel.\n"
                 "Fue cargada dinamicamente en RAM mediante un Page Fault (#PF)!\n",
                 i, i);
    }

    /* 2. Registrar número de dispositivo */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) goto err_free_pages;
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

    pr_info("[kmod_demand_paging] Dispositivo /dev/%s creado con exito.\n", DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(demo_class);
err_cdev_del:
    cdev_del(&demo_cdev);
err_unregister:
    unregister_chrdev_region(dev_num, 1);
err_free_pages:
    for (int j = 0; j < i; j++) {
        if (driver_pages[j])
            __free_page(driver_pages[j]);
    }
    return ret;
}

static void __exit demo_demand_exit(void)
{
    dev_t dev_num = MKDEV(major_number, 0);
    int i;

    device_destroy(demo_class, dev_num);
    class_destroy(demo_class);
    cdev_del(&demo_cdev);
    unregister_chrdev_region(dev_num, 1);

    for (i = 0; i < NUM_PAGES; i++) {
        if (driver_pages[i]) {
            __free_page(driver_pages[i]);
            driver_pages[i] = NULL;
        }
    }

    pr_info("[kmod_demand_paging] Modulo descargado y paginas liberadas.\n");
}

module_init(demo_demand_init);
module_exit(demo_demand_exit);
