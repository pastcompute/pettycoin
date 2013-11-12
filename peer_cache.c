#include "peer_cache.h"
#include "peer.h"
#include "protocol_net.h"
#include "state.h"
#include "pseudorand.h"
#include "log.h"
#include <ccan/err/err.h>
#include <ccan/noerr/noerr.h>
#include <ccan/hash/hash.h>
#include <ccan/cast/cast.h>
#include <ccan/time/time.h>
#include <ccan/array_size/array_size.h>
#include <ccan/tal/tal.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

/* We don't need more than 2,000 peer addresses. */
#define PEER_HASH_BITS 12
struct peer_hash_entry {
	struct protocol_net_address addr;
	u32 last_used;
};

struct peer_cache_file {
	le64 randbytes;
	struct peer_hash_entry h[1 << PEER_HASH_BITS];
};

struct peer_cache {
	int fd;
	struct peer_cache_file file;
};

static bool get_lock(int fd)
{
	struct flock fl;
	int ret;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	/* Note: non-blocking! */
	do {
		ret = fcntl(fd, F_SETLK, &fl);
	} while (ret == -1 && errno == EINTR);

	return ret == 0;
}

/* Create (and lock) peer_cache. */
static int new_peer_cache(struct state *state, le64 *randbytes)
{
	int fd;

	log_unusual(state->log, "Creating peer_cache file");

	/* O_EXCL prevents race. */
	fd = open("peer_cache", O_RDWR|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		fatal(state, "Could not create peer_cache");

	if (!get_lock(fd))
		fatal(state, "Someone else using peer_cache during creation");

	if (RAND_bytes((unsigned char *)randbytes, sizeof(*randbytes)) != 1) {
		unlink_noerr("peer_cache");
		fatal(state, "Could not seed peer_cache: %s",
		     ERR_error_string(ERR_get_error(), NULL));
	}

	if (write(fd, randbytes, sizeof(*randbytes)) != sizeof(*randbytes)) {
		unlink_noerr("peer_cache");
		fatal(state, "Writing seed to peer_cache gave %s",
		      strerror(errno));
	}
	if (ftruncate(fd, sizeof(struct peer_cache_file)) != 0) {
		unlink_noerr("peer_cache");
		fatal(state, "Extending peer_cache gave %s", strerror(errno));
	}
	if (lseek(fd, SEEK_SET, 0) != 0) {
		unlink_noerr("peer_cache");
		fatal(state, "Seeking to start of peer_cache gave %s",
		      strerror(errno));
	}
	return fd;
}

static struct peer_hash_entry *
peer_hash_entry(const struct peer_cache *pc,
		const struct protocol_net_address *addr)
{
	u32 h = hash64((u8 *)addr, sizeof(*addr), pc->file.randbytes);

	return cast_const(struct peer_hash_entry *,
			  &pc->file.h[h % ARRAY_SIZE(pc->file.h)]);
}

static bool is_zero_addr(const struct protocol_net_address *addr)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(addr->addr); i++)
		if (addr->addr[i])
			return false;
	return true;
}

static bool check_peer_cache(struct state *state, const struct peer_cache *pc)
{
	unsigned int i;
	u32 time = time_to_sec(time_now());

	for (i = 0; i < ARRAY_SIZE(pc->file.h); i++) {
		if (is_zero_addr(&pc->file.h[i].addr))
			continue;
		if (pc->file.h[i].last_used > time) {
			log_unusual(state->log,
				    "peer_cache entry %i is in future (%u > %u)",
				    i, pc->file.h[i].last_used, time);
			return false;
		}
		if (peer_hash_entry(pc, &pc->file.h[i].addr) != &pc->file.h[i]) {
			log_unusual(state->log,
				    "peer_cache entry %i should be %i, address ",
				    i, peer_hash_entry(pc, &pc->file.h[i].addr)
				    - pc->file.h);
			log_add_struct(state->log, struct protocol_net_address,
				       &pc->file.h[i].addr);
			return false;
		}
	}
	return true;
}

void init_peer_cache(struct state *state)
{
	struct peer_cache *pc;

	state->peer_cache = pc = tal(state, struct peer_cache);

	pc->fd = open("peer_cache", O_RDWR);
	if (pc->fd < 0) {
		pc->fd = new_peer_cache(state, &pc->file.randbytes);
		memset(pc->file.h, 0, sizeof(pc->file.h));
	} else {
		if (!get_lock(pc->fd))
			fatal(state, "Someone else using peer_cache");

		if (read(pc->fd, &pc->file, sizeof(pc->file))!=sizeof(pc->file)
		    || !check_peer_cache(state, pc)) {
			log_unusual(state->log, "Corrupt peer_cache: removing");
			close(pc->fd);
			unlink("peer_cache");
			pc->fd = new_peer_cache(state, &pc->file.randbytes);
		}
	}
}

static bool peer_already(struct state *state, const struct protocol_net_address *a)
{
	struct peer *p;

	list_for_each(&state->peers, p, list)
		if (memcmp(&p->you, a, sizeof(a)) == 0)
			return true;
	return false;
}

struct protocol_net_address *read_peer_cache(struct state *state)
{
	struct peer_cache *pc = state->peer_cache;
	int start;
	static int i;

	BUILD_ASSERT(sizeof(start) * CHAR_BIT >= PEER_HASH_BITS);

	start = i;
	do {
		if (!is_zero_addr(&pc->file.h[i].addr)) {
			if (!peer_already(state, &pc->file.h[i].addr)) {
				log_debug(state->log, "Address peer cache ");
				log_add_struct(state->log,
					       struct protocol_net_address,
					       &pc->file.h[i].addr);
				return &pc->file.h[i].addr;
			}
			log_debug(state->log, "Peer cache gave repeat ");
			log_add_struct(state->log,
				       struct protocol_net_address,
				       &pc->file.h[i].addr);
		}
		i = (i+1) % ARRAY_SIZE(pc->file.h);
	} while (i != start);

	log_debug(state->log, "Peer cache is empty");
	return NULL;
}

static void update_on_disk(struct state *state,
			   struct peer_cache *pc,
			   const struct peer_hash_entry *e)
{
	lseek(pc->fd, (char *)e - (char *)&pc->file, SEEK_SET);
	if (write(pc->fd, e, sizeof(*e)) != sizeof(*e))
		log_unusual(state->log, "Trouble writing peer_cache: %s",
			    strerror(errno));
}

void peer_cache_update(struct state *state, 
		       const struct protocol_net_address *addr,
		       u32 last_used)
{
	struct peer_hash_entry *e = peer_hash_entry(state->peer_cache, addr);

	/* Don't update if not in cache. */
	if (memcmp(&e->addr, addr, sizeof(*addr)) != 0) {
		log_debug(state->log, "Can't update address not in peer_cache ");
		log_add_struct(state->log, struct protocol_net_address, addr);
		return;
	}

	/* Don't go backwards (eg. if peer hands us known address. */
	if (last_used < e->last_used) {
		log_debug(state->log, "peer_cache time %u newer than %u for ",
			  e->last_used, last_used);
		log_add_struct(state->log, struct protocol_net_address, addr);
		return;
	}

	log_debug(state->log, "peer_cache update to %u for ", last_used);
	log_add_struct(state->log, struct protocol_net_address, addr);

	e->last_used = last_used;
	update_on_disk(state, state->peer_cache, e);
}

void peer_cache_add(struct state *state, 
		    const struct protocol_net_address *addr)
{
	struct peer_hash_entry *e = peer_hash_entry(state->peer_cache, addr);

	if (memcmp(&e->addr, addr, sizeof(*addr)) == 0) {
		log_debug(state->log, "peer_cache adding repeat for ");
		log_add_struct(state->log, struct protocol_net_address, addr);
		return;
	}

	/* 50% chance of replacing different entry. */
	if (!is_zero_addr(&e->addr) && isaac64_next_uint(isaac64, 2)) {
		log_debug(state->log, "peer_cache not replacing ");
		log_add_struct(state->log, struct protocol_net_address,
			       &e->addr);
		log_add(state->log, " with ");
		log_add_struct(state->log, struct protocol_net_address, addr);
		return;
	}

	log_debug(state->log, "peer_cache replacing ");
	log_add_struct(state->log, struct protocol_net_address, &e->addr);
	log_add(state->log, " with ");
	log_add_struct(state->log, struct protocol_net_address, addr);

	e->addr = *addr;
	e->last_used = 0;
	update_on_disk(state, state->peer_cache, e);
}

void peer_cache_del(struct state *state,
		    const struct protocol_net_address *addr,
		    bool del_on_disk)
{
	struct peer_hash_entry *e = peer_hash_entry(state->peer_cache, addr);

	if (memcmp(&e->addr, addr, sizeof(*addr)) == 0) {
		log_debug(state->log, "peer_cache deleting ");
		log_add_struct(state->log, struct protocol_net_address, addr);
		memset(e, 0, sizeof(*e));
		if (del_on_disk) {
			log_debug(state->log, "peer_cache updating disk");
			update_on_disk(state, state->peer_cache, e);
		}
	} else {
		log_debug(state->log, "peer_cache not deleting ");
		log_add_struct(state->log, struct protocol_net_address, addr);
		log_add(state->log, " already have ");
		log_add_struct(state->log, struct protocol_net_address,
			       &e->addr);
	}
}