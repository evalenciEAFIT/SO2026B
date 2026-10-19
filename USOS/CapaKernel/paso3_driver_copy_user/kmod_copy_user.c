/**
 * @file kmod_copy_user.c
 * @brief PASO 3: Driver de Kernel clásico usando .read y copy_to_user()
 * @course Sistemas Operativos - DOCENCIA 2026-B
 * 
 * ¿QUÉ OCURRE EN LA CAPA KERNEL AQUÍ?
 * -----------------------------------
 * Este módulo crea un dispositivo de caracteres (/dev/demo_copy_user).
 * Implementa la interfaz estándar de operaciones de archivo (struct file_operations)
 * con la función .read.
 * 
 * ANÁLISIS DE LA FUNCIÓN dev_read():
 * 1. El kernel tiene los datos en su propio espacio de memoria (kernel_buffer,
 *    alojado con kzalloc en memoria del kernel).
 * 2. El usuario proporciona un puntero virtual (char __user *buf) que pertenece
 *    a la tabla de páginas del proceso en espacio de usuario.
 * 3. ¿Por qué el kernel NO PUEDE hacer memcpy(buf, kernel_buffer, count)?
 *    - SEGURIDAD: 'buf' podría ser una dirección inválida, un puntero nulo, o
 *      apuntar a memoria del propio kernel (ataque de escalada de privilegios).
 *    - HARDWARE (SMAP): Las CPUs modernas tienen SMAP (Supervisor Mode Access
 *      Prevention) que genera un fallo de CPU si el kernel desreferencia
 *      directamente un puntero de usuario sin deshabilitar temporalmente la bandera.
 *    - FALLOS DE PÁGINA: La página de usuario podría estar paginada (swapped)
 *      o no asignada aún. copy_to_user() maneja excepciones de página del kernel de
 *      manera segura con tablas de excepción (fixup tables).
 * 4. EL COSTO DE copy_to_user():
 *    - CPU Lee 'kernel_buffer' de RAM -> Registros CPU -> Escribe en 'buf' en RAM.
 *    - Duplica el tráfico en el bus de memoria (Memory Bus Contention).
 *    - Consume ciclos de cómputo proporcionales al tamaño de los datos transferidos.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVICE_NAME     "demo_copy_user"
#define CLASS_NAME      "demo_copy_class"
#define BUFFER_SIZE     (64 * 1024)   /* 64 KB de búfer interno en kernel */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Docencia SO 2026-B");
MODULE_DESCRIPTION("Driver demostrativo de E/S clasica con copy_to_user");
MODULE_VERSION("1.0");

static int major_number;
static struct class *demo_class = NULL;
static struct device *demo_device = NULL;
static struct cdev demo_cdev;

/* Búfer en espacio de memoria del Kernel */
static char *kernel_buffer = NULL;

static int dev_open(struct inode *inodep, struct file *filep)
{
    pr_info("[kmod_copy_user] Dispositivo abierto por proceso: %s (PID: %d)\n",
            current->comm, current->pid);
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    pr_info("[kmod_copy_user] Dispositivo cerrado por proceso: %s (PID: %d)\n",
            current->comm, current->pid);
    return 0;
}

/**
 * dev_read - Implementación tradicional en la capa kernel
 * @filep: Puntero al descriptor de archivo del kernel
 * @buf:   Puntero al búfer en ESPACIO DE USUARIO (__user)
 * @count: Cantidad de bytes solicitados
 * @ppos:  Desplazamiento actual (offset)
 */
static ssize_t dev_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    size_t bytes_to_copy;
    unsigned long not_copied;

    if (*ppos >= BUFFER_SIZE)
        return 0; /* EOF */

    /* Calcular cuántos bytes podemos entregar */
    bytes_to_copy = min(count, (size_t)(BUFFER_SIZE - *ppos));

    /*
     * AQUÍ ESTÁ EL CUELLO DE BOTELLA:
     * copy_to_user() realiza una copia explícita de memoria byte por byte
     * desde el espacio del kernel (kernel_buffer) hacia el espacio de usuario (buf).
     */
    not_copied = copy_to_user(buf, kernel_buffer + *ppos, bytes_to_copy);
    if (not_copied != 0) {
        pr_warn("[kmod_copy_user] Advertencia: %lu bytes no pudieron copiarse al usuario\n",
                not_copied);
        return -EFAULT;
    }

    *ppos += bytes_to_copy;
    pr_info("[kmod_copy_user] dev_read(): copiados %zu bytes a espacio de usuario mediante copy_to_user\n",
            bytes_to_copy);

    return bytes_to_copy;
}

/* Tabla de operaciones de archivo (VFS Interface) */
static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
};

static int __init demo_copy_user_init(void)
{
    dev_t dev_num;
    int ret;

    pr_info("[kmod_copy_user] Inicializando modulo en Capa Kernel...\n");

    /* 1. Alojar búfer en memoria del Kernel con GFP_KERNEL */
    kernel_buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        pr_err("[kmod_copy_user] Error al reservar memoria con kzalloc\n");
        return -ENOMEM;
    }

    /* Llenar con datos demostrativos */
    memset(kernel_buffer, 'K', BUFFER_SIZE);
    snprintf(kernel_buffer, 256,
             "=== MENSAJE DESDE EL KERNEL DE LINUX ===\n"
             "Estos datos residen en la memoria física del kernel (RAM).\n"
             "Fueron transferidos al usuario usando la syscall read() y copy_to_user().\n");

    /* 2. Asignar dinámicamente número Mayor y Menor para el dispositivo */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("[kmod_copy_user] Error en alloc_chrdev_region: %d\n", ret);
        kfree(kernel_buffer);
        return ret;
    }
    major_number = MAJOR(dev_num);

    /* 3. Inicializar la estructura cdev y vincular con fops */
    cdev_init(&demo_cdev, &fops);
    demo_cdev.owner = THIS_MODULE;
    ret = cdev_add(&demo_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        kfree(kernel_buffer);
        return ret;
    }

    /* 4. Crear clase de dispositivo en sysfs (/sys/class/demo_copy_class) */
    demo_class = class_create(CLASS_NAME);
    if (IS_ERR(demo_class)) {
        cdev_del(&demo_cdev);
        unregister_chrdev_region(dev_num, 1);
        kfree(kernel_buffer);
        return PTR_ERR(demo_class);
    }

    /* 5. Crear nodo de dispositivo en devtmpfs (/dev/demo_copy_user) */
    demo_device = device_create(demo_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(demo_device)) {
        class_destroy(demo_class);
        cdev_del(&demo_cdev);
        unregister_chrdev_region(dev_num, 1);
        kfree(kernel_buffer);
        return PTR_ERR(demo_device);
    }

    pr_info("[kmod_copy_user] Dispositivo /dev/%s registrado con exito (Major %d)\n",
            DEVICE_NAME, major_number);
    return 0;
}

static void __exit demo_copy_user_exit(void)
{
    dev_t dev_num = MKDEV(major_number, 0);

    device_destroy(demo_class, dev_num);
    class_destroy(demo_class);
    cdev_del(&demo_cdev);
    unregister_chrdev_region(dev_num, 1);

    if (kernel_buffer) {
        kfree(kernel_buffer);
        kernel_buffer = NULL;
    }

    pr_info("[kmod_copy_user] Modulo descargado y memoria del kernel liberada.\n");
}

module_init(demo_copy_user_init);
module_exit(demo_copy_user_exit);
