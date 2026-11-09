// Small C bridge so Swift can call the C++ engine (core/akkhara_engine.*).
// All strings are UTF-8. Lists come back as one string with the items separated by '\n'.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AkkEngine AkkEngine;

// Load the model folder (encoder.int8.onnx, decoder.int8.onnx, tokenizer.json, abbreviations.tsv).
// NULL on failure, with the reason in err.
AkkEngine* akk_open(const char* model_dir, char* err, size_t err_len);
void akk_close(AkkEngine* e);

// Every key press gets a new generation number; work for an older one stops early (cancelled).
void akk_set_generation(AkkEngine* e, int64_t gen);

// #1 only, fast (greedy). Returns 0 when cancelled.
int akk_quick(AkkEngine* e, int64_t gen, const char* latin, char* out, size_t out_len);
// Top-5, '\n'-separated. Returns the number of items, 0 when cancelled.
int akk_candidates(AkkEngine* e, int64_t gen, const char* latin, char* out, size_t out_len);

#ifdef __cplusplus
}
#endif
