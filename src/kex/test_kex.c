#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oqs/rand.h>
#include <oqs/kex.h>

#define KEX_TEST_ITERATIONS 500

#define PRINT_HEX_STRING(label, str, len) { \
	printf("%-20s (%4zu bytes):  ", (label), (size_t) (len)); \
	for (size_t i = 0; i < (len); i++) { \
		printf("%02X", ((unsigned char *) (str)) [i]); \
	} \
	printf("\n"); \
}

static int kex_test_correctness(OQS_RAND *rand, OQS_KEX * (*new_method)(OQS_RAND *, const uint8_t *, const size_t, const char *), const uint8_t *seed, const size_t seed_len, const char *named_parameters, const int print, unsigned long occurrences[256]) {

	OQS_KEX *kex = NULL;
	int rc;

	void *alice_priv = NULL;
	uint8_t *alice_msg = NULL;
	size_t alice_msg_len;
	uint8_t *alice_key = NULL;
	size_t alice_key_len;

	uint8_t *bob_msg = NULL;
	size_t bob_msg_len;
	uint8_t *bob_key = NULL;
	size_t bob_key_len;

	/* setup KEX */
	kex = new_method(rand, seed, seed_len, named_parameters);
	if (kex == NULL) {
		goto err;
	}

	if (print) {
		printf("================================================================================\n");
		printf("Sample computation for key exchange method %s\n", kex->method_name);
		printf("================================================================================\n");
	}

	/* Alice's initial message */
	rc = OQS_KEX_alice_0(kex, &alice_priv, &alice_msg, &alice_msg_len);
	if (rc != 1) {
		goto err;
	}

	if (print) {
		PRINT_HEX_STRING("Alice message", alice_msg, alice_msg_len)
	}

	/* Bob's response */
	rc = OQS_KEX_bob(kex, alice_msg, alice_msg_len, &bob_msg, &bob_msg_len, &bob_key, &bob_key_len);
	if (rc != 1) {
		goto err;
	}

	if (print) {
		PRINT_HEX_STRING("Bob message", bob_msg, bob_msg_len)
		PRINT_HEX_STRING("Bob session key", bob_key, bob_key_len)
	}

	/* Alice processes Bob's response */
	rc = OQS_KEX_alice_1(kex, alice_priv, bob_msg, bob_msg_len, &alice_key, &alice_key_len);
	if (rc != 1) {
		goto err;
	}

	if (print) {
		PRINT_HEX_STRING("Alice session key", alice_key, alice_key_len)
	}

	/* compare session key lengths and values */
	if (alice_key_len != bob_key_len) {
		fprintf(stderr, "ERROR: Alice's session key and Bob's session key are different lengths (%zu vs %zu)\n", alice_key_len, bob_key_len);
		goto err;
	}
	rc = memcmp(alice_key, bob_key, alice_key_len);
	if (rc != 0) {
		fprintf(stderr, "ERROR: Alice's session key and Bob's session key are not equal\n");
		PRINT_HEX_STRING("Alice session key", alice_key, alice_key_len)
		PRINT_HEX_STRING("Bob session key", bob_key, bob_key_len)
		goto err;
	}
	if (print) {
		printf("Alice and Bob's session keys match.\n");
		printf("\n\n");
	}

	/* record generated bytes for statistical analysis */
	for (size_t i = 0; i < alice_key_len; i++) {
		OQS_RAND_test_record_occurrence(alice_key[i], occurrences);
	}

	rc = 1;
	goto cleanup;

err:
	rc = 0;

cleanup:
	free(alice_msg);
	free(alice_key);
	free(bob_msg);
	free(bob_key);
	OQS_KEX_alice_priv_free(kex, alice_priv);
	OQS_KEX_free(kex);

	return rc;

}

static int kex_test_correctness_wrapper(OQS_RAND *rand, OQS_KEX * (*new_method)(OQS_RAND *, const uint8_t *, const size_t, const char *), const uint8_t *seed, const size_t seed_len, const char *named_parameters, int iterations) {

	OQS_KEX *kex = NULL;
	int ret;

	unsigned long occurrences[256];
	for (int i = 0; i < 256; i++) {
		occurrences[i] = 0;
	}

	ret = kex_test_correctness(rand, new_method, seed, seed_len, named_parameters, 1, occurrences);
	if (ret != 1) goto err;

	/* setup KEX */
	kex = new_method(rand, seed, seed_len, named_parameters);
	if (kex == NULL) {
		goto err;
	}

	printf("================================================================================\n");
	printf("Testing correctness and randomness of key exchange method %s (params=%s) for %d iterations\n", kex->method_name, named_parameters, iterations);
	printf("================================================================================\n");
	for (int i = 0; i < iterations; i++) {
		ret = kex_test_correctness(rand, new_method, seed, seed_len, named_parameters, 0, occurrences);
		if (ret != 1) goto err;
	}
	printf("All session keys matched.\n");
	printf("Statistical distance from uniform: %12.10f\n", OQS_RAND_test_statistical_distance_from_uniform(occurrences));

	ret = 1;
	goto cleanup;

err:
	ret = 0;

cleanup:
	OQS_KEX_free(kex);

	return ret;

}

int main() {

	int success;

	/* setup RAND */
	OQS_RAND *rand = NULL;
	rand = OQS_RAND_new();
	if (rand == NULL) {
		goto err;
	}

	success = kex_test_correctness_wrapper(rand, &OQS_KEX_new, NULL, 0, NULL, KEX_TEST_ITERATIONS);
	if (success != 1) {
		goto err;
	}

	success = 1;
	goto cleanup;

err:
	success = 0;
	fprintf(stderr, "ERROR!\n");

cleanup:
	OQS_RAND_free(rand);

	return (success == 1) ? EXIT_SUCCESS : EXIT_FAILURE;

}
