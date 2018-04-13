/*
 * Copyright (c) 2015, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __CRYPTO_MOD_H__
#define __CRYPTO_MOD_H__

#include <img_parser_mod.h>

#if TRUSTED_BOARD_BOOT == 1
#include <mbedtls_config.h>
#endif

/* Return values */
enum crypto_ret_value {
	CRYPTO_SUCCESS = 0,
	CRYPTO_ERR_INIT,
	CRYPTO_ERR_HASH,
	CRYPTO_ERR_SIGNATURE,
	CRYPTO_ERR_DECRYPT,
	CRYPTO_ERR_UNKNOWN
};

/*
 * Encrypted image descriptor
 */
typedef struct crypto_img_desc_s {
	unsigned int img_id;
	img_type_t img_type;
	unsigned int tbbr_cipher_type_id;
} crypto_img_desc_t;

/*
 * Cryptographic library descriptor
 */
typedef struct crypto_lib_desc_s {
	const char *name;

	/* Initialize library. This function is not expected to fail. All errors
	 * must be handled inside the function, asserting or panicing in case of
	 * a non-recoverable error */
	void (*init)(void);

	/* Verify a digital signature. Return one of the
	 * 'enum crypto_ret_value' options */
	int (*verify_signature)(void *data_ptr, unsigned int data_len,
				void *sig_ptr, unsigned int sig_len,
				void *sig_alg, unsigned int sig_alg_len,
				void *pk_ptr, unsigned int pk_len);

	/* Verify a hash. Return one of the 'enum crypto_ret_value' options */
	int (*verify_hash)(void *data_ptr, unsigned int data_len,
			   void *digest_info_ptr, unsigned int digest_info_len);

	/* Decrypt image */
	int (*decrypt_image)(void *data_ptr, unsigned int data_len,
			     unsigned int cipher_type, unsigned char **key,
			     unsigned int *key_len);
} crypto_lib_desc_t;

/* Public functions */
void crypto_mod_init(void);
int crypto_mod_verify_signature(void *data_ptr, unsigned int data_len,
				void *sig_ptr, unsigned int sig_len,
				void *sig_alg, unsigned int sig_alg_len,
				void *pk_ptr, unsigned int pk_len);
int crypto_mod_verify_hash(void *data_ptr, unsigned int data_len,
			   void *digest_info_ptr, unsigned int digest_info_len);
int crypto_mod_decrypt_image(unsigned int img_id, void *data_ptr,
			     unsigned int data_len, unsigned char **key,
			     unsigned int *key_len);

/* Macro to register a cryptographic library */
#define REGISTER_CRYPTO_LIB(_name, _init, _verify_signature, _verify_hash, _decrypt_image) \
	const crypto_lib_desc_t crypto_lib_desc = { \
		.name = _name, \
		.init = _init, \
		.verify_signature = _verify_signature, \
		.verify_hash = _verify_hash, \
		.decrypt_image = _decrypt_image \
	}

/* Macro to register a list of encrypted images from CoT */
#define REGISTER_CRYPTO_PARAMS(_crypto_params) \
	const crypto_img_desc_t *const crypto_params_ptr = \
		(const crypto_img_desc_t *const)&_crypto_params[0]

#endif /* __CRYPTO_MOD_H__ */
