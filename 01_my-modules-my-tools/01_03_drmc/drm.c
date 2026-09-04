/*
 * drm.c - Adobe DRM EPUB decrypter for Kobo (ARM cross-compiled)
 * Usage: drm -k KEY.der -f encrypted.epub [-o output.epub]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <getopt.h>

#include <zip.h>
#include <zlib.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#define VERSION "1.0"
#define AES_KEY_SIZE 16
#define AES_IV_SIZE 16
#define RSA_KEY_SIZE 2048
#define TEMP_DIR_TEMPLATE "/tmp/drm_XXXXXX"

#ifndef ZIP_CM_DEFLATE
#define ZIP_CM_DEFLATE 8
#endif

#ifndef ZIP_CM_STORE
#define ZIP_CM_STORE 0
#endif

/* ============================================================================
 * STRUCTURES
 * ============================================================================ */

typedef struct {
    char* uri;
    size_t expected_size;
    zip_uint16_t compression_method;
} EncryptedFile;

typedef struct {
    EncryptedFile* files;
    int count;
    int capacity;
} FileList;

typedef struct {
    unsigned char aes_key[AES_KEY_SIZE];
    char* temp_dir;
    char* output_path;
    int verbose;
} DRMContext;

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

static void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("* ");
    vprintf(fmt, args);
    printf("\n");
    fflush(stdout);
    va_end(args);
}

static void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "ERROR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(args);
}

static void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        log_error("Out of memory (trying to allocate %zu bytes)", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static char* str_concat(const char* a, const char* b) {
    size_t len = strlen(a) + strlen(b) + 1;
    char* result = safe_malloc(len);
    strcpy(result, a);
    strcat(result, b);
    return result;
}

static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static size_t file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_size;
}

static bool read_file(const char* path, unsigned char** data, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    *data = safe_malloc(*size);
    size_t read = fread(*data, 1, *size, f);
    fclose(f);

    return read == *size;
}

static bool write_file(const char* path, const unsigned char* data, size_t size) {
    char* dir = strdup(path);
    char* dname = dirname(dir);
    struct stat st;
    if (stat(dname, &st) != 0) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dname);
        system(cmd);
    }
    free(dir);

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

static void delete_file(const char* path) {
    unlink(path);
}

static bool create_temp_dir(char* template) {
    char* result = mkdtemp(template);
    return result != NULL;
}

static void remove_directory_recursive(const char* path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    system(cmd);
}

/* ============================================================================
 * ZIP OPERATIONS
 * ============================================================================ */

static bool extract_zip(const char* zip_path, const char* output_dir, FileList* file_list) {
    int err = 0;
    struct zip* z = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!z) {
        log_error("Cannot open ZIP file: %s (error %d)", zip_path, err);
        return false;
    }

    zip_int64_t num_entries = zip_get_num_entries(z, 0);
    for (zip_int64_t i = 0; i < num_entries; i++) {
        struct zip_stat st;
        zip_stat_index(z, i, 0, &st);

        if (st.size == 0 || st.name[0] == '\0') continue;

        zip_uint16_t compression_method = st.comp_method;

        char* full_path = str_concat(output_dir, "/");
        char* full_path2 = str_concat(full_path, st.name);
        free(full_path);

        struct zip_file* zf = zip_fopen_index(z, i, 0);
        if (!zf) {
            log_error("Cannot open file in ZIP: %s", st.name);
            free(full_path2);
            continue;
        }

        unsigned char* data = safe_malloc(st.size);
        zip_int64_t read = zip_fread(zf, data, st.size);
        zip_fclose(zf);

        if (read != (zip_int64_t)st.size) {
            log_error("Failed to read complete file: %s", st.name);
            free(data);
            free(full_path2);
            continue;
        }

        char* dir_copy = strdup(full_path2);
        char* dname = dirname(dir_copy);
        struct stat dir_st;
        if (stat(dname, &dir_st) != 0) {
            char mkdir_cmd[1024];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", dname);
            system(mkdir_cmd);
        }
        free(dir_copy);

        write_file(full_path2, data, st.size);
        free(data);
        free(full_path2);

        if (file_list) {
            for (int j = 0; j < file_list->count; j++) {
                if (strcmp(file_list->files[j].uri, st.name) == 0) {
                    file_list->files[j].compression_method = compression_method;
                    break;
                }
            }
        }
    }

    zip_close(z);
    return true;
}

typedef struct {
    char* path;
    char* arc;
    int is_mimetype;
} FileEntry;

static int compare_files(const void* a, const void* b) {
    const FileEntry* fa = (const FileEntry*)a;
    const FileEntry* fb = (const FileEntry*)b;

    if (fa->is_mimetype && !fb->is_mimetype) return -1;
    if (!fa->is_mimetype && fb->is_mimetype) return 1;
    return strcmp(fa->arc, fb->arc);
}

static void collect_files(const char* base_dir, const char* rel_path, FileEntry** entries, int* count, int* capacity) {
    char full_path[4096];
    if (rel_path && strlen(rel_path) > 0) {
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, rel_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", base_dir);
    }

    DIR* dir = opendir(full_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child_path[4096];
        char child_rel[4096];

        if (rel_path && strlen(rel_path) > 0) {
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel_path, entry->d_name);
        } else {
            snprintf(child_rel, sizeof(child_rel), "%s", entry->d_name);
        }

        snprintf(child_path, sizeof(child_path), "%s/%s", full_path, entry->d_name);

        struct stat st;
        if (stat(child_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                collect_files(base_dir, child_rel, entries, count, capacity);
            } else {
                if (*count >= *capacity) {
                    *capacity = *capacity == 0 ? 64 : *capacity * 2;
                    *entries = realloc(*entries, *capacity * sizeof(FileEntry));
                }

                (*entries)[*count].path = strdup(child_path);
                (*entries)[*count].arc = strdup(child_rel);
                (*entries)[*count].is_mimetype = (strcmp(child_rel, "mimetype") == 0);
                (*count)++;
            }
        }
    }
    closedir(dir);
}

static bool rebuild_epub(const char* temp_dir, const char* output_path, FileList* file_list, bool verbose) {
    int err = 0;
    struct zip* z = zip_open(output_path, ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!z) {
        log_error("Cannot create output ZIP: %s (error %d)", output_path, err);
        return false;
    }

    FileEntry* entries = NULL;
    int count = 0;
    int capacity = 0;

    collect_files(temp_dir, NULL, &entries, &count, &capacity);

    if (count == 0) {
        log_error("No files found in temp directory");
        free(entries);
        zip_close(z);
        return false;
    }

    qsort(entries, count, sizeof(FileEntry), compare_files);

    for (int i = 0; i < count; i++) {
        FileEntry* fe = &entries[i];

        if (strcmp(fe->arc, "META-INF/rights.xml") == 0 ||
            strcmp(fe->arc, "META-INF/encryption.xml") == 0) {
            continue;
            }

            zip_source_t* source = zip_source_file(z, fe->path, 0, -1);
        if (!source) {
            log_error("Failed to create source for: %s", fe->arc);
            continue;
        }

        zip_int64_t idx = zip_file_add(z, fe->arc, source, ZIP_FL_OVERWRITE);
        if (idx < 0) {
            zip_source_free(source);
            log_error("Failed to add file: %s", fe->arc);
            continue;
        }

        if (fe->is_mimetype) {
            zip_file_set_encryption(z, idx, ZIP_EM_NONE, NULL);
        }

        if (verbose) {
            printf("   Added: %s\n", fe->arc);
        }
    }

    for (int i = 0; i < count; i++) {
        free(entries[i].path);
        free(entries[i].arc);
    }
    free(entries);

    if (zip_close(z) != 0) {
        log_error("Failed to close ZIP: %s", zip_strerror(z));
        return false;
    }

    return true;
}

/* ============================================================================
 * XML PARSING
 * ============================================================================ */

static bool parse_rights_xml(const char* temp_dir, unsigned char** enc_key, size_t* enc_len) {
    char* rights_path = str_concat(temp_dir, "/META-INF/rights.xml");

    if (!file_exists(rights_path)) {
        log_error("rights.xml not found");
        free(rights_path);
        return false;
    }

    xmlDocPtr doc = xmlReadFile(rights_path, NULL, 0);
    if (!doc) {
        log_error("Failed to parse rights.xml");
        free(rights_path);
        return false;
    }

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) {
        log_error("Failed to create XPath context");
        xmlFreeDoc(doc);
        free(rights_path);
        return false;
    }

    if (xmlXPathRegisterNs(ctx, BAD_CAST "adept", BAD_CAST "http://ns.adobe.com/adept") != 0) {
        log_error("Failed to register namespace");
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
        free(rights_path);
        return false;
    }

    xmlXPathObjectPtr result = xmlXPathEvalExpression(
        BAD_CAST "//adept:encryptedKey", ctx);

    if (!result || xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        log_error("encryptedKey not found in rights.xml");
        if (result) xmlXPathFreeObject(result);
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
        free(rights_path);
        return false;
    }

    xmlNodePtr node = result->nodesetval->nodeTab[0];
    xmlChar* content = xmlNodeGetContent(node);

    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new_mem_buf(content, -1);
    BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    *enc_key = safe_malloc(RSA_KEY_SIZE);
    *enc_len = BIO_read(b64, *enc_key, RSA_KEY_SIZE);

    if (*enc_len <= 0) {
        log_error("Failed to decode base64 encryptedKey");
        free(*enc_key);
        *enc_key = NULL;
        *enc_len = 0;
        BIO_free_all(b64);
        xmlFree(content);
        xmlXPathFreeObject(result);
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);
        free(rights_path);
        return false;
    }

    BIO_free_all(b64);
    xmlFree(content);
    xmlXPathFreeObject(result);
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
    free(rights_path);

    return true;
}

static bool parse_encryption_xml(const char* temp_dir, FileList* file_list) {
    char* enc_path = str_concat(temp_dir, "/META-INF/encryption.xml");

    if (!file_exists(enc_path)) {
        log_error("encryption.xml not found");
        free(enc_path);
        return false;
    }

    unsigned char* data;
    size_t size;
    if (!read_file(enc_path, &data, &size)) {
        free(enc_path);
        return false;
    }

    char* content = (char*)safe_malloc(size + 1);
    memcpy(content, data, size);
    content[size] = '\0';
    free(data);

    file_list->files = NULL;
    file_list->count = 0;
    file_list->capacity = 0;

    const char* ptr = content;
    while ((ptr = strstr(ptr, "URI=\"")) != NULL) {
        ptr += 5;
        const char* end_uri = strchr(ptr, '"');
        if (!end_uri) break;

        int uri_len = end_uri - ptr;
        char* uri = safe_malloc(uri_len + 1);
        strncpy(uri, ptr, uri_len);
        uri[uri_len] = '\0';

        const char* size_ptr = strstr(end_uri, "ResourceSize>");
        if (!size_ptr) {
            free(uri);
            break;
        }
        size_ptr += 13;

        const char* end_size = strchr(size_ptr, '<');
        if (!end_size) {
            free(uri);
            break;
        }

        int size_len = end_size - size_ptr;
        char* size_str = safe_malloc(size_len + 1);
        strncpy(size_str, size_ptr, size_len);
        size_str[size_len] = '\0';

        size_t expected_size = strtoul(size_str, NULL, 10);
        free(size_str);

        if (strncmp(uri, "META-INF", 8) != 0) {
            bool exists = false;
            for (int i = 0; i < file_list->count; i++) {
                if (strcmp(file_list->files[i].uri, uri) == 0) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                if (file_list->count >= file_list->capacity) {
                    file_list->capacity = file_list->capacity == 0 ? 64 : file_list->capacity * 2;
                    file_list->files = realloc(file_list->files,
                                               file_list->capacity * sizeof(EncryptedFile));
                }
                file_list->files[file_list->count].uri = uri;
                file_list->files[file_list->count].expected_size = expected_size;
                file_list->files[file_list->count].compression_method = ZIP_CM_DEFLATE;
                file_list->count++;
            } else {
                free(uri);
            }
        } else {
            free(uri);
        }

        ptr = end_size;
    }

    free(content);
    free(enc_path);
    return true;
}

/* ============================================================================
 * CRYPTOGRAPHY
 * ============================================================================ */

static bool rsa_decrypt_key(const unsigned char* der_key, size_t der_len,
                            const unsigned char* enc_key, size_t enc_len,
                            unsigned char* aes_key) {
    BIO* der_bio = BIO_new_mem_buf(der_key, der_len);
    RSA* rsa = d2i_RSAPrivateKey_bio(der_bio, NULL);
    BIO_free(der_bio);

    if (!rsa) {
        log_error("Failed to parse DER private key");
        return false;
    }

    unsigned char decrypted[RSA_KEY_SIZE];
    int dec_len = RSA_private_decrypt(enc_len, enc_key, decrypted, rsa, RSA_PKCS1_PADDING);
    RSA_free(rsa);

    if (dec_len != AES_KEY_SIZE) {
        log_error("RSA decryption returned %d bytes, expected %d", dec_len, AES_KEY_SIZE);
        return false;
    }

    memcpy(aes_key, decrypted, AES_KEY_SIZE);
    return true;
                            }

                            static bool aes_decrypt_file(const char* path, const unsigned char* aes_key,
                                                         unsigned char** output, size_t* output_size) {
                                unsigned char* data;
                                size_t data_size;

                                if (!read_file(path, &data, &data_size)) {
                                    return false;
                                }

                                if (data_size < AES_IV_SIZE) {
                                    free(data);
                                    return false;
                                }

                                unsigned char iv[AES_IV_SIZE];
                                memcpy(iv, data, AES_IV_SIZE);

                                unsigned char* encrypted = data + AES_IV_SIZE;
                                size_t enc_len = data_size - AES_IV_SIZE;

                                EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
                                EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, aes_key, iv);

                                unsigned char* decrypted = safe_malloc(enc_len + EVP_MAX_BLOCK_LENGTH);
                                int dec_len = 0;
                                int total_len = 0;

                                EVP_DecryptUpdate(ctx, decrypted, &dec_len, encrypted, enc_len);
                                total_len = dec_len;

                                EVP_DecryptFinal_ex(ctx, decrypted + total_len, &dec_len);
                                total_len += dec_len;

                                EVP_CIPHER_CTX_free(ctx);
                                free(data);

                                *output = decrypted;
                                *output_size = total_len;
                                return true;
                                                         }

                                                         /* ============================================================================
                                                          * COMPRESSION DETECTION
                                                          * ============================================================================ */

                                                         typedef struct {
                                                             unsigned char* data;
                                                             size_t size;
                                                             const char* method;
                                                         } DecompressionResult;

                                                         static DecompressionResult detect_compression(const unsigned char* data, size_t size, size_t expected_size) {
                                                             DecompressionResult result = {NULL, 0, "none"};
                                                             int ret;
                                                             z_stream strm;

                                                             memset(&strm, 0, sizeof(strm));
                                                             ret = inflateInit2(&strm, -15);
                                                             if (ret == Z_OK) {
                                                                 strm.avail_in = size;
                                                                 strm.next_in = (Bytef*)data;
                                                                 strm.avail_out = expected_size > 0 ? expected_size : size * 4;
                                                                 unsigned char* out = safe_malloc(strm.avail_out);
                                                                 strm.next_out = out;

                                                                 ret = inflate(&strm, Z_FINISH);
                                                                 if (ret == Z_STREAM_END) {
                                                                     size_t out_size = strm.total_out;
                                                                     inflateEnd(&strm);
                                                                     result.data = out;
                                                                     result.size = out_size;
                                                                     result.method = "raw_deflate";
                                                                     return result;
                                                                 }
                                                                 inflateEnd(&strm);
                                                                 free(out);
                                                             }

                                                             memset(&strm, 0, sizeof(strm));
                                                             ret = inflateInit2(&strm, 15);
                                                             if (ret == Z_OK) {
                                                                 strm.avail_in = size;
                                                                 strm.next_in = (Bytef*)data;
                                                                 strm.avail_out = expected_size > 0 ? expected_size : size * 4;
                                                                 unsigned char* out = safe_malloc(strm.avail_out);
                                                                 strm.next_out = out;

                                                                 ret = inflate(&strm, Z_FINISH);
                                                                 if (ret == Z_STREAM_END) {
                                                                     size_t out_size = strm.total_out;
                                                                     inflateEnd(&strm);
                                                                     result.data = out;
                                                                     result.size = out_size;
                                                                     result.method = "zlib";
                                                                     return result;
                                                                 }
                                                                 inflateEnd(&strm);
                                                                 free(out);
                                                             }

                                                             memset(&strm, 0, sizeof(strm));
                                                             ret = inflateInit2(&strm, 31);
                                                             if (ret == Z_OK) {
                                                                 strm.avail_in = size;
                                                                 strm.next_in = (Bytef*)data;
                                                                 strm.avail_out = expected_size > 0 ? expected_size : size * 4;
                                                                 unsigned char* out = safe_malloc(strm.avail_out);
                                                                 strm.next_out = out;

                                                                 ret = inflate(&strm, Z_FINISH);
                                                                 if (ret == Z_STREAM_END) {
                                                                     size_t out_size = strm.total_out;
                                                                     inflateEnd(&strm);
                                                                     result.data = out;
                                                                     result.size = out_size;
                                                                     result.method = "gzip";
                                                                     return result;
                                                                 }
                                                                 inflateEnd(&strm);
                                                                 free(out);
                                                             }

                                                             result.data = safe_malloc(size);
                                                             memcpy(result.data, data, size);
                                                             result.size = size;
                                                             result.method = "none";
                                                             return result;
                                                         }

                                                         /* ============================================================================
                                                          * MAIN LOGIC
                                                          * ============================================================================ */

                                                         static bool process_epub(DRMContext* ctx, const char* input_path, const char* key_path) {
                                                             log_info("Processing: %s", input_path);

                                                             unsigned char* der_key;
                                                             size_t der_len;
                                                             if (!read_file(key_path, &der_key, &der_len)) {
                                                                 log_error("Cannot read key file: %s", key_path);
                                                                 return false;
                                                             }

                                                             char temp_dir_template[] = TEMP_DIR_TEMPLATE;
                                                             if (!create_temp_dir(temp_dir_template)) {
                                                                 log_error("Failed to create temp directory");
                                                                 free(der_key);
                                                                 return false;
                                                             }
                                                             ctx->temp_dir = strdup(temp_dir_template);
                                                             log_info("Temp dir: %s", ctx->temp_dir);

                                                             char* temp_input = str_concat(ctx->temp_dir, "/input.epub");
                                                             log_info("Copying input to temp: %s", temp_input);

                                                             unsigned char* input_data;
                                                             size_t input_size;
                                                             if (!read_file(input_path, &input_data, &input_size)) {
                                                                 log_error("Failed to read input file");
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(der_key);
                                                                 free(temp_input);
                                                                 return false;
                                                             }

                                                             if (!write_file(temp_input, input_data, input_size)) {
                                                                 log_error("Failed to copy input to temp");
                                                                 free(input_data);
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(der_key);
                                                                 free(temp_input);
                                                                 return false;
                                                             }
                                                             free(input_data);

                                                             /* PRIMERO extraer el EPUB */
                                                             log_info("Extracting EPUB...");
                                                             if (!extract_zip(temp_input, ctx->temp_dir, NULL)) {
                                                                 log_error("Failed to extract EPUB");
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(der_key);
                                                                 free(temp_input);
                                                                 return false;
                                                             }
                                                             free(temp_input);

                                                             /* DESPUÉS parsear los XML */
                                                             FileList file_list = {0};
                                                             if (!parse_encryption_xml(ctx->temp_dir, &file_list)) {
                                                                 log_error("Failed to parse encryption.xml");
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(der_key);
                                                                 return false;
                                                             }

                                                             unsigned char* enc_rsa_key;
                                                             size_t enc_rsa_len;
                                                             if (!parse_rights_xml(ctx->temp_dir, &enc_rsa_key, &enc_rsa_len)) {
                                                                 log_error("Failed to parse rights.xml");
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(der_key);
                                                                 free(file_list.files);
                                                                 return false;
                                                             }

                                                             if (!rsa_decrypt_key(der_key, der_len, enc_rsa_key, enc_rsa_len, ctx->aes_key)) {
                                                                 log_error("Failed to decrypt RSA key");
                                                                 free(enc_rsa_key);
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(der_key);
                                                                 free(file_list.files);
                                                                 return false;
                                                             }
                                                             free(enc_rsa_key);
                                                             free(der_key);

                                                             if (ctx->verbose) {
                                                                 printf("   AES Key: ");
                                                                 for (int i = 0; i < AES_KEY_SIZE; i++) {
                                                                     printf("%02X", ctx->aes_key[i]);
                                                                 }
                                                                 printf("\n");
                                                             }

                                                             log_info("Decrypting %d files...", file_list.count);
                                                             log_info("Using automatic compression detection");

                                                             int methods_count = 0;
                                                             const char* methods_names[10];
                                                             int methods_counts[10] = {0};

                                                             for (int i = 0; i < file_list.count; i++) {
                                                                 EncryptedFile* ef = &file_list.files[i];
                                                                 char* full_path = str_concat(ctx->temp_dir, "/");
                                                                 char* full_path2 = str_concat(full_path, ef->uri);
                                                                 free(full_path);

                                                                 if (!file_exists(full_path2)) {
                                                                     free(full_path2);
                                                                     continue;
                                                                 }

                                                                 unsigned char* decrypted;
                                                                 size_t dec_size;
                                                                 if (!aes_decrypt_file(full_path2, ctx->aes_key, &decrypted, &dec_size)) {
                                                                     free(full_path2);
                                                                     continue;
                                                                 }

                                                                 DecompressionResult decomp = detect_compression(decrypted, dec_size, ef->expected_size);
                                                                 free(decrypted);

                                                                 bool found = false;
                                                                 for (int j = 0; j < methods_count; j++) {
                                                                     if (strcmp(methods_names[j], decomp.method) == 0) {
                                                                         methods_counts[j]++;
                                                                         found = true;
                                                                         break;
                                                                     }
                                                                 }
                                                                 if (!found && methods_count < 10) {
                                                                     methods_names[methods_count] = decomp.method;
                                                                     methods_counts[methods_count] = 1;
                                                                     methods_count++;
                                                                 }

                                                                 write_file(full_path2, decomp.data, decomp.size);
                                                                 free(decomp.data);
                                                                 free(full_path2);

                                                                 if ((i + 1) % 10 == 0 || i == file_list.count - 1) {
                                                                     printf("   %d/%d files\n", i + 1, file_list.count);
                                                                 }
                                                             }

                                                             for (int i = 0; i < methods_count; i++) {
                                                                 printf("   %s: %d files\n", methods_names[i], methods_counts[i]);
                                                             }

                                                             char* rights_path = str_concat(ctx->temp_dir, "/META-INF/rights.xml");
                                                             char* enc_path = str_concat(ctx->temp_dir, "/META-INF/encryption.xml");
                                                             delete_file(rights_path);
                                                             delete_file(enc_path);
                                                             free(rights_path);
                                                             free(enc_path);

                                                             char* temp_output = str_concat(ctx->temp_dir, "/output.epub");
                                                             log_info("Creating EPUB in temp: %s", temp_output);

                                                             if (!rebuild_epub(ctx->temp_dir, temp_output, &file_list, ctx->verbose)) {
                                                                 log_error("Failed to rebuild EPUB");
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(file_list.files);
                                                                 free(temp_output);
                                                                 return false;
                                                             }

                                                             log_info("Copying to final destination: %s", ctx->output_path);

                                                             unsigned char* output_data;
                                                             size_t output_size;
                                                             if (!read_file(temp_output, &output_data, &output_size)) {
                                                                 log_error("Failed to read temp output");
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(file_list.files);
                                                                 free(temp_output);
                                                                 return false;
                                                             }

                                                             if (!write_file(ctx->output_path, output_data, output_size)) {
                                                                 log_error("Failed to write final output");
                                                                 free(output_data);
                                                                 remove_directory_recursive(ctx->temp_dir);
                                                                 free(file_list.files);
                                                                 free(temp_output);
                                                                 return false;
                                                             }
                                                             free(output_data);
                                                             free(temp_output);

                                                             remove_directory_recursive(ctx->temp_dir);
                                                             free(file_list.files);

                                                             return true;
                                                         }

                                                         /* ============================================================================
                                                          * MAIN
                                                          * ============================================================================ */

                                                         static void print_usage(const char* progname) {
                                                             printf("drm v%s - Adobe DRM EPUB decrypter\n\n", VERSION);
                                                             printf("Usage: %s -k KEY -f FILE [-o OUTPUT] [-v]\n\n", progname);
                                                             printf("Options:\n");
                                                             printf("  -k, --key KEY     Path to .der private key file\n");
                                                             printf("  -f, --file FILE   Path to encrypted EPUB file\n");
                                                             printf("  -o, --output OUT  Output path (optional)\n");
                                                             printf("  -v, --verbose     Verbose output\n");
                                                             printf("  -h, --help        Show this help\n\n");
                                                             printf("Example:\n");
                                                             printf("  %s -k private.der -f book.epub -o book_open.epub\n", progname);
                                                         }

                                                         int main(int argc, char** argv) {
                                                             OpenSSL_add_all_algorithms();
                                                             ERR_load_crypto_strings();
                                                             xmlInitParser();

                                                             DRMContext ctx = {0};
                                                             ctx.output_path = NULL;
                                                             ctx.verbose = 0;

                                                             char* key_path = NULL;
                                                             char* input_path = NULL;
                                                             char* output_path = NULL;

                                                             static struct option long_options[] = {
                                                                 {"key", required_argument, 0, 'k'},
                                                                 {"file", required_argument, 0, 'f'},
                                                                 {"output", required_argument, 0, 'o'},
                                                                 {"verbose", no_argument, 0, 'v'},
                                                                 {"help", no_argument, 0, 'h'},
                                                                 {0, 0, 0, 0}
                                                             };

                                                             int opt;
                                                             while ((opt = getopt_long(argc, argv, "k:f:o:vh", long_options, NULL)) != -1) {
                                                                 switch (opt) {
                                                                     case 'k':
                                                                         key_path = optarg;
                                                                         break;
                                                                     case 'f':
                                                                         input_path = optarg;
                                                                         break;
                                                                     case 'o':
                                                                         output_path = optarg;
                                                                         break;
                                                                     case 'v':
                                                                         ctx.verbose = 1;
                                                                         break;
                                                                     case 'h':
                                                                         print_usage(argv[0]);
                                                                         return EXIT_SUCCESS;
                                                                     default:
                                                                         print_usage(argv[0]);
                                                                         return EXIT_FAILURE;
                                                                 }
                                                             }

                                                             if (!key_path || !input_path) {
                                                                 log_error("Missing required arguments");
                                                                 print_usage(argv[0]);
                                                                 return EXIT_FAILURE;
                                                             }

                                                             if (!file_exists(key_path)) {
                                                                 log_error("Key file does not exist: %s", key_path);
                                                                 return EXIT_FAILURE;
                                                             }

                                                             if (!file_exists(input_path)) {
                                                                 log_error("Input file does not exist: %s", input_path);
                                                                 return EXIT_FAILURE;
                                                             }

                                                             if (output_path) {
                                                                 ctx.output_path = output_path;
                                                             } else {
                                                                 char* base = strdup(input_path);
                                                                 char* ext = strrchr(base, '.');
                                                                 if (ext) *ext = '\0';
                                                                 ctx.output_path = safe_malloc(strlen(base) + 10);
                                                                 sprintf(ctx.output_path, "%s_open.epub", base);
                                                                 free(base);
                                                             }

                                                             if (file_exists(ctx.output_path)) {
                                                                 log_error("Output file already exists: %s", ctx.output_path);
                                                                 return EXIT_FAILURE;
                                                             }

                                                             bool success = process_epub(&ctx, input_path, key_path);

                                                             EVP_cleanup();
                                                             ERR_free_strings();
                                                             xmlCleanupParser();

                                                             if (success) {
                                                                 size_t out_size = file_size(ctx.output_path);
                                                                 log_info("Success! Output: %s", ctx.output_path);
                                                                 log_info("Size: %zu bytes", out_size);
                                                                 fflush(stdout);
                                                                 exit(0);
                                                             } else {
                                                                 log_error("Failed to decrypt EPUB");
                                                                 fflush(stderr);
                                                                 exit(EXIT_FAILURE);
                                                             }
                                                         }
