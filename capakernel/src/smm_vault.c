// SPDX-License-Identifier: GPL-2.0
/*
 * smm_vault.c - Boveda de paginas cifradas
 *
 * El "archivo confidencial" vive en memoria de kernel SIEMPRE cifrado
 * (AES-256 en modo CTR, API de cifrado del kernel). Nunca existe una copia
 * en texto plano compartida: cada mapeo autorizado recibe sus propias
 * paginas descifradas bajo demanda (ver smm_main.c::smm_fault).
 *
 * Diseño criptografico:
 *   - Implementacion: crypto_alloc_skcipher("ctr(aes)") SIN restringir a
 *     algoritmos sincronos. En x86 la version rapida "ctr-aes-aesni" (AES-NI
 *     sobre la pagina completa) se registra como asincrona; la API sincrona
 *     (crypto_alloc_sync_skcipher) la excluye y cae en la plantilla generica
 *     "ctr(aes-aesni)", que cifra bloque a bloque (16 B) guardando y
 *     restaurando la FPU 256 veces por pagina: ~300 us/pagina medidos con
 *     ftrace en la VM. Con la version asincrona + crypto_wait_req() el costo
 *     cae a unos pocos microsegundos. Todas las llamadas ocurren en contexto
 *     de proceso con vault_lock tomado, asi que se usa UNA sola solicitud
 *     preasignada (sin kmalloc en la ruta del page fault).
 *   - Llave de 256 bits generada con get_random_bytes() al cargar el modulo.
 *     Se borra (memzero_explicit) apenas se instala en el tfm; solo queda la
 *     version expandida dentro del contexto de cifrado del kernel.
 *   - IV de 128 bits por pagina = [ indice (32b) | generacion (32b) | 0 (64b) ].
 *     Los 64 bits bajos son el contador de bloques de CTR (una pagina = 256
 *     bloques), y la generacion aumenta en cada re-cifrado, de modo que NUNCA
 *     se reutiliza el mismo flujo de llave (reutilizarlo en CTR filtraria
 *     P1 XOR P2).
 *   - Integridad: CRC32 del texto plano por pagina. Detecta manipulacion del
 *     texto cifrado (cambiar 1 bit del cifrado cambia 1 bit del plano).
 *     Limitacion documentada: CRC32 no es un MAC; un atacante con acceso de
 *     escritura a la memoria del kernel podria recalcularlo. Trabajo futuro:
 *     AES-GCM (AEAD) o HMAC-SHA256.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/crc32.h>
#include <linux/string.h>
#include <linux/scatterlist.h>
#include <crypto/skcipher.h>

#include "smm.h"

struct smm_vault_page {
	struct page *ct;  /* pagina con el texto cifrado              */
	u32 crc;          /* CRC32 del texto plano (integridad)        */
	u32 gen;          /* generacion: forma parte del IV            */
};

static struct smm_vault_page *vault;
static unsigned int vault_n;
static DEFINE_MUTEX(vault_lock);  /* serializa cifrado/descifrado de la boveda */
static struct crypto_skcipher *tfm;
static struct skcipher_request *req;  /* unica solicitud, protegida por vault_lock */

static u32 smm_crc(const void *buf)
{
	return crc32_le(~0U, buf, PAGE_SIZE);
}

/*
 * AES-CTR de una pagina completa. En CTR cifrar y descifrar es la misma operacion.
 * Llamar con vault_lock tomado (usa la solicitud compartida "req").
 */
static int smm_ctr_page(unsigned long idx, u32 gen, void *dst, const void *src)
{
	DECLARE_CRYPTO_WAIT(wait);
	struct scatterlist sg_src, sg_dst;
	__be32 iv[4];
	int ret;

	iv[0] = cpu_to_be32((u32)idx);
	iv[1] = cpu_to_be32(gen);
	iv[2] = 0;
	iv[3] = 0;

	sg_init_one(&sg_src, src, PAGE_SIZE);
	sg_init_one(&sg_dst, dst, PAGE_SIZE);

	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
				      crypto_req_done, &wait);
	skcipher_request_set_crypt(req, &sg_src, &sg_dst, PAGE_SIZE, (u8 *)iv);
	/* Si el algoritmo responde asincronamente, dormimos hasta que termine. */
	ret = crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
	memzero_explicit(iv, sizeof(iv));
	return ret;
}

/* Llamar con vault_lock tomado. */
static int __smm_vault_encrypt(unsigned long idx, const void *src)
{
	struct smm_vault_page *vp = &vault[idx];
	int ret;

	vp->gen++;
	ret = smm_ctr_page(idx, vp->gen, page_address(vp->ct), src);
	if (!ret)
		vp->crc = smm_crc(src);
	return ret;
}

/* Llamar con vault_lock tomado. */
static int __smm_vault_decrypt(unsigned long idx, void *dst)
{
	struct smm_vault_page *vp = &vault[idx];
	int ret;

	ret = smm_ctr_page(idx, vp->gen, dst, page_address(vp->ct));
	if (ret)
		return ret;
	if (smm_crc(dst) != vp->crc)
		return -EBADMSG;
	return 0;
}

unsigned int smm_vault_npages(void)
{
	return vault_n;
}

int smm_vault_decrypt_page(unsigned long idx, void *dst)
{
	int ret;

	if (idx >= vault_n)
		return -EINVAL;
	mutex_lock(&vault_lock);
	ret = __smm_vault_decrypt(idx, dst);
	mutex_unlock(&vault_lock);
	if (ret)
		memzero_explicit(dst, PAGE_SIZE); /* nunca entregar datos alterados */
	return ret;
}

int smm_vault_encrypt_page(unsigned long idx, const void *src)
{
	int ret;

	if (idx >= vault_n)
		return -EINVAL;
	mutex_lock(&vault_lock);
	ret = __smm_vault_encrypt(idx, src);
	mutex_unlock(&vault_lock);
	return ret;
}

bool smm_vault_page_changed(unsigned long idx, const void *src)
{
	bool changed;

	if (idx >= vault_n)
		return false;
	mutex_lock(&vault_lock);
	changed = smm_crc(src) != vault[idx].crc;
	mutex_unlock(&vault_lock);
	return changed;
}

/*
 * Lectura-modificacion-escritura de [off, off+len) dentro de la pagina idx.
 * src debe ser memoria de KERNEL ya copiada desde el usuario: si hicieramos
 * copy_from_user() aqui con vault_lock tomado y el buffer de usuario fuera
 * la propia region mmap de este dispositivo, el page fault resultante
 * intentaria tomar vault_lock otra vez -> deadlock (auto-bloqueo).
 */
int smm_vault_update(unsigned long idx, size_t off, const void *src,
		     size_t len, void *scratch)
{
	int ret;

	if (idx >= vault_n || off >= PAGE_SIZE || len > PAGE_SIZE - off)
		return -EINVAL;

	mutex_lock(&vault_lock);
	ret = __smm_vault_decrypt(idx, scratch);
	if (!ret) {
		memcpy((u8 *)scratch + off, src, len);
		ret = __smm_vault_encrypt(idx, scratch);
	}
	mutex_unlock(&vault_lock);
	memzero_explicit(scratch, PAGE_SIZE);
	return ret;
}

int smm_vault_corrupt(unsigned long idx)
{
	u8 *ct;

	if (idx >= vault_n)
		return -EINVAL;
	mutex_lock(&vault_lock);
	ct = page_address(vault[idx].ct);
	ct[64] ^= 0xFF;  /* un solo byte del texto cifrado */
	mutex_unlock(&vault_lock);
	return 0;
}

/* Contenido de demostracion: un "documento confidencial" por pagina. */
static void smm_fill_demo(char *buf, unsigned int idx, unsigned int total)
{
	size_t pos;
	unsigned int line = 0;

	memset(buf, 0, PAGE_SIZE);
	pos = scnprintf(buf, PAGE_SIZE,
			"=== DOCUMENTO CONFIDENCIAL EAFIT-SO 2026-2 | pagina %u/%u ===\n",
			idx, total - 1);
	while (pos < PAGE_SIZE - 96) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
				 "pag %04u linea %03u: dato sensible protegido por secure_mmap\n",
				 idx, line++);
	}
}

int smm_vault_init(unsigned int npages)
{
	u8 key[32];
	void *tmp;
	unsigned int i;
	int ret;

	if (!npages)
		return -EINVAL;

	tfm = crypto_alloc_skcipher("ctr(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		tfm = NULL;
		pr_err(SMM_TAG "no se pudo crear ctr(aes): %d\n", ret);
		return ret;
	}

	get_random_bytes(key, sizeof(key));
	ret = crypto_skcipher_setkey(tfm, key, sizeof(key));
	memzero_explicit(key, sizeof(key));
	if (ret)
		goto err_tfm;

	req = skcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto err_tfm;
	}

	vault = kvcalloc(npages, sizeof(*vault), GFP_KERNEL);
	if (!vault) {
		ret = -ENOMEM;
		goto err_tfm;
	}

	tmp = (void *)__get_free_page(GFP_KERNEL);
	if (!tmp) {
		ret = -ENOMEM;
		goto err_vault;
	}

	vault_n = npages;
	for (i = 0; i < npages; i++) {
		vault[i].ct = alloc_page(GFP_KERNEL);
		if (!vault[i].ct) {
			ret = -ENOMEM;
			goto err_pages;
		}
		smm_fill_demo(tmp, i, npages);
		ret = __smm_vault_encrypt(i, tmp);
		if (ret)
			goto err_pages;
	}
	memzero_explicit(tmp, PAGE_SIZE);
	free_page((unsigned long)tmp);

	pr_info(SMM_TAG "boveda lista: %u paginas cifradas con %s\n", npages,
		crypto_skcipher_driver_name(tfm));
	return 0;

err_pages:
	memzero_explicit(tmp, PAGE_SIZE);
	free_page((unsigned long)tmp);
	for (i = 0; i < npages; i++)
		if (vault[i].ct)
			__free_page(vault[i].ct);
err_vault:
	kvfree(vault);
	vault = NULL;
	vault_n = 0;
err_tfm:
	skcipher_request_free(req);
	req = NULL;
	crypto_free_skcipher(tfm);
	tfm = NULL;
	return ret;
}

void smm_vault_exit(void)
{
	unsigned int i;

	if (vault) {
		for (i = 0; i < vault_n; i++)
			if (vault[i].ct)
				__free_page(vault[i].ct);
		kvfree(vault);
		vault = NULL;
	}
	vault_n = 0;
	skcipher_request_free(req);
	req = NULL;
	if (tfm) {
		crypto_free_skcipher(tfm);
		tfm = NULL;
	}
}
