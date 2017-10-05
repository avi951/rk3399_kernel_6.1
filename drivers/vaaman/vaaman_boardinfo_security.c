#include "vaaman_boardinfo.h"
#include <linux/types.h>


static struct boardinfo_data *boardinfo;

static int boardinfo_alloc_akcipher(void)
{
	int ret;

	boardinfo->akcipher = crypto_alloc_akcipher("rsa", 0, 0);

	if (IS_ERR(boardinfo->akcipher)) {
		ret = PTR_ERR(boardinfo->akcipher);
		pr_err("failed to allocate rsa crypto\n");
		return ret;
	}

	return 0;
}

static void boardinfo_free_akcipher(void)
{
	crypto_free_akcipher(boardinfo->akcipher);
}

static int boardinfo_sign(
		size_t data_len, u8 *sig, size_t *sig_len)
{
	struct scatterlist sg_in, sg_out;
	int ret;

	boardinfo->req = akcipher_request_alloc(boardinfo->akcipher, GFP_KERNEL);
	if (!boardinfo->req) {
		pr_err("failed to allocate akcipher request\n");
		return -ENOMEM;
	}

	akcipher_request_set_callback(boardinfo->req, 0, NULL, NULL);

	akcipher_request_set_crypt(boardinfo->req, &sg_in, &sg_out, data_len, *sig_len);

	sg_init_one(&sg_in, data, data_len);

	sg_init_one(&sg_out, sig, *sig_len);

	ret = crypto_akcipher_sign(boardinfo->req);

	akcipher_request_free(boardinfo->req);

	// Print the signature
	pr_info("Signature: %s\n", sig);

	return ret;

}

static int boardinfo_verify(
		size_t data_len, u8 *sig, size_t sig_len)
{
	struct scatterlist sg_in, sg_out;
	int ret;

	boardinfo->req = akcipher_request_alloc(boardinfo->akcipher, GFP_KERNEL);
	if (!boardinfo->req) {
		pr_err("failed to allocate akcipher request\n");
		return -ENOMEM;
	}

	akcipher_request_set_callback(boardinfo->req, 0, NULL, NULL);

	akcipher_request_set_crypt(boardinfo->req, &sg_in, &sg_out, data_len, sig_len);

	sg_init_one(&sg_in, data, data_len);

	sg_init_one(&sg_out, sig, sig_len);

	ret = crypto_akcipher_verify(boardinfo->req);

	akcipher_request_free(boardinfo->req);

	return ret;

}

static int boardinfo_encrypt(
		size_t data_len, u8 *enc, size_t *enc_len)
{
	struct scatterlist sg_in, sg_out;
	int ret;

	boardinfo->req = akcipher_request_alloc(boardinfo->akcipher, GFP_KERNEL);
	if (!boardinfo->req) {
		pr_err("failed to allocate akcipher request\n");
		return -ENOMEM;
	}

	akcipher_request_set_callback(boardinfo->req, 0, NULL, NULL);

	akcipher_request_set_crypt(boardinfo->req, &sg_in, &sg_out, data_len, *enc_len);

	sg_init_one(&sg_in, data, data_len);

	sg_init_one(&sg_out, enc, *enc_len);

	ret = crypto_akcipher_encrypt(boardinfo->req);

	akcipher_request_free(boardinfo->req);

	return ret;

}

static int boardinfo_decrypt(
		size_t enc_len, u8 *dec, size_t *dec_len)
{
	struct scatterlist sg_in, sg_out;
	int ret;

	boardinfo->req = akcipher_request_alloc(boardinfo->akcipher, GFP_KERNEL);
	if (!boardinfo->req) {
		pr_err("failed to allocate akcipher request\n");
		return -ENOMEM;
	}

	akcipher_request_set_callback(boardinfo->req, 0, NULL, NULL);

	akcipher_request_set_crypt(boardinfo->req, &sg_in, &sg_out, enc_len, *dec_len);

	sg_init_one(&sg_in, enc, enc_len);

	sg_init_one(&sg_out, dec, *dec_len);

	ret = crypto_akcipher_decrypt(boardinfo->req);

	akcipher_request_free(boardinfo->req);

	return ret;

}

