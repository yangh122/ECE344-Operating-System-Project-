#include "server_thread.h"
#include "common.h"
#include "request.h"


#define HASH_TABLE_MIN_BUCKETS (256 * 256)

typedef struct file_data file_data_t;

// struct for cache item
struct cache_entry {
    // members hash table
    struct file_data* file;
    struct cache_entry* b_next;

    // members for lru
    struct cache_entry* a_prev;
    struct cache_entry* a_next;

    int n_ref;
};
typedef struct cache_entry entry_t;

// hash table
typedef struct hash_bucket {
    entry_t* head;
    entry_t senti;
} hash_bucket_t;

typedef struct hash_table {
    int size;
    int buckets_size;
    hash_bucket_t* buckets;
} hash_table_t;

// doubly linked list for access order
typedef struct {
    entry_t senti;
    int size;
} lru_t;

typedef struct {
    int cur_size;
    int max_size;

    hash_table_t ht;
    lru_t lru;

    pthread_mutex_t lock;

    int n_insert;
    int n_evict;
    int n_hit;
    int n_miss;
} cache_t;

struct server {
    int nr_threads;
    int max_requests;
    int max_cache_size;
    int exiting;
    /* add any other parameters you need */
    int* conn_buf;
    pthread_t* threads;
    int request_head;
    int request_tail;
    pthread_mutex_t mutex;
    pthread_cond_t prod_cond;
    pthread_cond_t cons_cond;

    /* cache */
    cache_t cache;
};

/* initialize file data */
static struct file_data*
file_data_init(void)
{
    struct file_data* data;

    data = Malloc(sizeof(struct file_data));
    data->file_name = NULL;
    data->file_buf = NULL;
    data->file_size = 0;
    return data;
}

/* free all file data */
static void
file_data_free(struct file_data* data)
{
    free(data->file_name);
    free(data->file_buf);
    free(data);
}


static inline
entry_t* cache_entry_new(struct file_data* file)
{
    entry_t* e = calloc(1, sizeof(entry_t));
    e->file = file;

    return e;
}

static inline
void cache_entry_free(entry_t* e)
{
    file_data_free(e->file);
    free(e);
}

static inline
void hash_bucket_init(hash_bucket_t* b)
{
    b->senti.b_next = NULL;
    b->head = &b->senti;
}

static inline
void hash_bucket_free(hash_bucket_t* b)
{
    entry_t* e = b->head->b_next;
    while (e) {
        entry_t* del = e;
        e = del->b_next;
        cache_entry_free(del);
    }

    b->senti.b_next = NULL;
    b->head = &b->senti;
}

static inline
void hash_bucket_insert(hash_bucket_t* b, entry_t* e)
{
    e->b_next = b->head->b_next;
    b->head->b_next = e;
}

static inline
entry_t* hash_bucket_remove(hash_bucket_t* b, const char* name)
{
    entry_t* prev = b->head;
    while (prev && strcmp(prev->b_next->file->file_name, name) != 0) {
        prev = prev->b_next;
    }
    if (!prev) {
        return NULL;
    }

    entry_t* e = prev->b_next;
    prev->b_next = e->b_next;
    return e;
}

static inline
entry_t* hash_bucket_search(hash_bucket_t* b, const char* name)
{
    entry_t* e = b->head->b_next;
    while (e && strcmp(e->file->file_name, name) != 0) {
        e = e->b_next;
    }

    return e;
}

static inline
void hash_table_init(hash_table_t* ht, int buckets_size)
{
    if (buckets_size < HASH_TABLE_MIN_BUCKETS) {
        buckets_size = HASH_TABLE_MIN_BUCKETS;
    }

    ht->buckets_size = buckets_size;
    ht->buckets = calloc(buckets_size, sizeof(hash_bucket_t));
    for (int i = 0; i < buckets_size; i++) {
        hash_bucket_init(ht->buckets + i);
    }
}

static inline
void hash_table_free(hash_table_t* ht)
{
    for (int i = 0; i < ht->buckets_size; i++) {
        hash_bucket_free(ht->buckets + i);
    }
    free(ht->buckets);
}

static inline
unsigned long hash(const char* name)
{
    unsigned long h = 5381;
    unsigned char c;
    while ((c = *(name++))) {
		h = (h << 5) + h + c;
    }

    return h;
}

static inline
hash_bucket_t* hash_table_get_bucket(hash_table_t* ht, const char* name)
{
    unsigned long h = hash(name);
    return ht->buckets + (h % ht->buckets_size);
}

static inline
entry_t* hash_table_lookup(hash_table_t* ht, const char* name)
{
    hash_bucket_t* b = hash_table_get_bucket(ht, name);
    return hash_bucket_search(b, name);
}

static inline
entry_t* hash_table_insert(hash_table_t* ht, file_data_t* f)
{
    // entry_t* e = hash_table_lookup(ht, f->file_name);
    // if (e) {
    //     return e;
    // }

    entry_t* e = cache_entry_new(f);
    hash_bucket_t* b = hash_table_get_bucket(ht, f->file_name);
    hash_bucket_insert(b, e);

    return e;
}

static inline
int hash_table_delete(hash_table_t* ht, const char* name)
{
    hash_bucket_t* b = hash_table_get_bucket(ht, name);
    entry_t* e = hash_bucket_remove(b, name);
    if (!e) {
        return 0;
    }

    // log("[cache] delete %s[%p, %d]", e->file->file_name, e->file->file_buf, e->file->file_size);
    cache_entry_free(e);
    return 1;
}

static inline
void lru_init(lru_t* lru)
{
    lru->senti.a_next = &(lru->senti);
    lru->senti.a_prev = &(lru->senti);
    lru->size = 0;
}

static inline
void lru_free(lru_t* lru)
{

}


static inline
void lru_update(lru_t* lru, entry_t* e)
{
    e->a_prev = lru->senti.a_prev;
    e->a_next = &(lru->senti);
    e->a_prev->a_next = e;
    e->a_next->a_prev = e;
    lru->size++;
}

static inline
void lru_remove(lru_t* lru, entry_t* e)
{
    e->a_prev->a_next = e->a_next;
    e->a_next->a_prev = e->a_prev;
    // e->a_next = NULL;
    // e->a_prev = NULL;
    lru->size--;
}

static inline
entry_t* lru_evict(lru_t* lru)
{
    entry_t* e = lru->senti.a_next;
    while (e != &(lru->senti) && e->n_ref > 0) {
        e = e->a_next;
    }

    if (e == &(lru->senti)) {
        return NULL;
    }

    lru_remove(lru, e);

    return e;
}

static inline
void cache_init(cache_t* c, int max_size)
{
    c->cur_size = 0;
    c->max_size = max_size;
    if (max_size <= 0) {
        return;
    }

    hash_table_init(&c->ht, c->max_size / 1024);
    lru_init(&c->lru);
    pthread_mutex_init(&c->lock, NULL);

    c->n_hit = 0;
    c->n_miss = 0;
    c->n_evict = 0;
    c->n_insert = 0;
}

static inline
void cache_free(cache_t* c)
{
    if (c->max_size <= 0) {
        return;
    }

    hash_table_free(&c->ht);
    lru_free(&c->lru);
    pthread_mutex_destroy(&c->lock);
}

static inline
void cache_deref_entry(cache_t* c, entry_t* e)
{
    if (e) {
        pthread_mutex_lock(&c->lock);
        e->n_ref--;
        pthread_mutex_unlock(&c->lock);
    }
}

static inline
entry_t* cache_lookup(cache_t* c, const char* name)
{
    pthread_mutex_lock(&c->lock);
    entry_t* e = hash_table_lookup(&c->ht, name);
    if (e) {
        lru_remove(&c->lru, e);
        lru_update(&c->lru, e);

        e->n_ref++;
		c->n_hit++;
    } else {
		c->n_miss++;
	}

    pthread_mutex_unlock(&c->lock);
    return e;
}

static inline
int cache_evict(cache_t* c, int file_size)
{
    while (c->max_size - c->cur_size < file_size) {
        entry_t* e = lru_evict(&c->lru);
        if (!e) {
            return 0;
        }
        c->n_evict++;
        c->cur_size -= e->file->file_size;

        hash_table_delete(&c->ht, e->file->file_name);
    }
    return 1;
}

static inline
entry_t* cache_insert(cache_t* c, file_data_t* file)
{
    entry_t* e;
    pthread_mutex_lock(&c->lock);

    e = hash_table_lookup(&c->ht, file->file_name);
    if (e) {
        e->n_ref++;
        pthread_mutex_unlock(&c->lock);
        return e;
    }

    if (c->max_size - c->cur_size < file->file_size) {
        if (!cache_evict(c, file->file_size)) {
            pthread_mutex_unlock(&c->lock);
            return NULL;
        }
    }

    e = hash_table_insert(&c->ht, file);
    lru_update(&c->lru, e);

    c->cur_size += file->file_size;
    e->n_ref++;

    c->n_insert++;

    pthread_mutex_unlock(&c->lock);

    return e;
}

static void
do_server_request(struct server* sv, int connfd)
{
    int ret;
    struct request* rq;
    struct file_data* data;

    data = file_data_init();

    /* fill data->file_name with name of the file being requested */
    rq = request_init(connfd, data);
    if (!rq) {
        file_data_free(data);
        return;
    }
    /* read file,
     * fills data->file_buf with the file contents,
     * data->file_size with file size. */
    entry_t* cache_entry = NULL;
    if (sv->max_cache_size > 0) {
        cache_entry = cache_lookup(&sv->cache, data->file_name);
        if (cache_entry) {
            request_set_data(rq, cache_entry->file);
        } else {
            ret = request_readfile(rq);
            if (ret == 0) { /* couldn't read file */
                goto out;
            }

            cache_entry = cache_insert(&sv->cache, data);
        }

        request_sendfile(rq);

		if (cache_entry) {
			cache_deref_entry(&sv->cache, cache_entry);
		}

    } else {
        ret = request_readfile(rq);
        if (ret == 0) { /* couldn't read file */
            goto out;
        }

        /* send file to client */
        request_sendfile(rq);
    }

out:
    request_destroy(rq);
    if (!cache_entry) {
        file_data_free(data);
    }
}

static void*
do_server_thread(void* arg)
{
    struct server* sv = (struct server*)arg;
    int connfd;

    while (1) {
        pthread_mutex_lock(&sv->mutex);
        while (sv->request_head == sv->request_tail) {
            /* buffer is empty */
            if (sv->exiting) {
                pthread_mutex_unlock(&sv->mutex);
                goto out;
            }
            pthread_cond_wait(&sv->cons_cond, &sv->mutex);
        }
        /* get request from tail */
        connfd = sv->conn_buf[sv->request_tail];
        /* consume request */
        sv->conn_buf[sv->request_tail] = -1;
        sv->request_tail = (sv->request_tail + 1) % sv->max_requests;

        pthread_cond_signal(&sv->prod_cond);
        pthread_mutex_unlock(&sv->mutex);
        /* now serve request */
        do_server_request(sv, connfd);
    }
out:
    return NULL;
}

/* entry point functions */

struct server*
server_init(int nr_threads, int max_requests, int max_cache_size)
{
    struct server* sv;
    int i;

    sv = Malloc(sizeof(struct server));
    sv->nr_threads = nr_threads;
    /* we add 1 because we queue at most max_request - 1 requests */
    sv->max_requests = max_requests + 1;
    sv->max_cache_size = max_cache_size;
    sv->exiting = 0;

    /* Lab 4: create queue of max_request size when max_requests > 0 */
    sv->conn_buf = Malloc(sizeof(*sv->conn_buf) * sv->max_requests);
    for (i = 0; i < sv->max_requests; i++) {
        sv->conn_buf[i] = -1;
    }
    sv->request_head = 0;
    sv->request_tail = 0;

    /* Lab 5: init server cache and limit its size to max_cache_size */
    cache_init(&sv->cache, max_cache_size);

    /* Lab 4: create worker threads when nr_threads > 0 */
    pthread_mutex_init(&sv->mutex, NULL);
    pthread_cond_init(&sv->prod_cond, NULL);
    pthread_cond_init(&sv->cons_cond, NULL);
    sv->threads = Malloc(sizeof(pthread_t) * nr_threads);
    for (i = 0; i < nr_threads; i++) {
        SYS(pthread_create(&(sv->threads[i]), NULL, do_server_thread,
            (void*)sv));
    }
    return sv;
}

void server_request(struct server* sv, int connfd)
{
    if (sv->nr_threads == 0) { /* no worker threads */
        do_server_request(sv, connfd);
    } else {
        /*  Save the relevant info in a buffer and have one of the
         *  worker threads do the work. */

        pthread_mutex_lock(&sv->mutex);
        while (((sv->request_head - sv->request_tail + sv->max_requests)
                   % sv->max_requests)
            == (sv->max_requests - 1)) {
            /* buffer is full */
            pthread_cond_wait(&sv->prod_cond, &sv->mutex);
        }
        /* fill conn_buf with this request */
        assert(sv->conn_buf[sv->request_head] == -1);
        sv->conn_buf[sv->request_head] = connfd;
        sv->request_head = (sv->request_head + 1) % sv->max_requests;
        pthread_cond_signal(&sv->cons_cond);
        pthread_mutex_unlock(&sv->mutex);
    }
}

void server_exit(struct server* sv)
{
    int i;
    /* when using one or more worker threads, use sv->exiting to indicate to
     * these threads that the server is exiting. make sure to call
     * pthread_join in this function so that the main server thread waits
     * for all the worker threads to exit before exiting. */
    pthread_mutex_lock(&sv->mutex);
    sv->exiting = 1;
    pthread_cond_broadcast(&sv->cons_cond);
    pthread_mutex_unlock(&sv->mutex);
    for (i = 0; i < sv->nr_threads; i++) {
        pthread_join(sv->threads[i], NULL);
    }

    cache_free(&sv->cache);

    /* make sure to free any allocated resources */
    free(sv->conn_buf);
    free(sv->threads);
    free(sv);
}
