#include "../chain.c"
#include "../state.c"
#include "../block.c"
#include "../pseudorand.c"
#include "../minimal_log.c"
#include "../difficulty.c"
#include "../block_shard.c"
#include "easy_genesis.c"
#include <ccan/strmap/strmap.h>
#include <ccan/tal/str/str.h>

/* AUTOGENERATED MOCKS START */
/* Generated stub for check_proof */
bool check_proof(const struct protocol_proof *proof,
		 const struct block *b,
		 const union protocol_tx *tx,
		 const struct protocol_input_ref *refs) { fprintf(stderr, "check_proof called!\n"); abort(); }
/* Generated stub for check_tx */
enum protocol_ecode check_tx(struct state *state, const union protocol_tx *tx,
			     const struct block *inside_block) { fprintf(stderr, "check_tx called!\n"); abort(); }
/* Generated stub for check_tx_inputs */
enum input_ecode check_tx_inputs(struct state *state,
				 const struct block *block,
				 const struct txhash_elem *me,
				 const union protocol_tx *tx,
				 unsigned int *bad_input_num) { fprintf(stderr, "check_tx_inputs called!\n"); abort(); }
/* Generated stub for complain_bad_prev_txhashes */
void complain_bad_prev_txhashes(struct state *state,
				struct block *block,
				const struct block *bad_prev,
				u16 bad_prev_shard) { fprintf(stderr, "complain_bad_prev_txhashes called!\n"); abort(); }
/* Generated stub for hash_tx_and_refs */
void hash_tx_and_refs(const union protocol_tx *tx,
		      const struct protocol_input_ref *refs,
		      struct protocol_txrefhash *txrefhash) { fprintf(stderr, "hash_tx_and_refs called!\n"); abort(); }
/* Generated stub for inputhash_hashfn */
size_t inputhash_hashfn(const struct inputhash_key *key) { fprintf(stderr, "inputhash_hashfn called!\n"); abort(); }
/* Generated stub for inputhash_keyof */
const struct inputhash_key *inputhash_keyof(const struct inputhash_elem *ie) { fprintf(stderr, "inputhash_keyof called!\n"); abort(); }
/* Generated stub for log_to_file */
void log_to_file(int fd, const struct log *log) { fprintf(stderr, "log_to_file called!\n"); abort(); }
/* Generated stub for logv */
void logv(struct log *log, enum log_level level, const char *fmt, va_list ap) { fprintf(stderr, "logv called!\n"); abort(); }
/* Generated stub for marshal_tx_len */
size_t marshal_tx_len(const union protocol_tx *tx) { fprintf(stderr, "marshal_tx_len called!\n"); abort(); }
/* Generated stub for merkle_txs */
void merkle_txs(const struct block_shard *shard,
		struct protocol_double_sha *merkle) { fprintf(stderr, "merkle_txs called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

void block_to_pending(struct state *state, const struct block *block)
{
}

void check_block(struct state *state, const struct block *block)
{
}

bool check_prev_txhashes(struct state *state, const struct block *block,
			 const struct block **bad_prev,
			 u16 *bad_shard)
{
	return true;
}

void restart_generating(struct state *state)
{
}

void todo_forget_about_block(struct state *state,
			     const struct protocol_double_sha *block)
{
}

void wake_peers(struct state *state)
{
}

void save_block(struct state *state, struct block *new)
{
}

struct log *new_log(const tal_t *ctx,
		    const struct log *parent, const char *prefix,
		    enum log_level printlevel, size_t max_mem)
{
	return NULL;
}

struct pending_block *new_pending_block(struct state *state)
{
	return talz(state, struct pending_block);
}

u8 pending_features(const struct block *block)
{
	return 0;
}

void todo_add_get_shard(struct state *state,
			const struct protocol_double_sha *block,
			u16 shardnum)
{
}

struct strmap_block {
	STRMAP_MEMBERS(struct block *);
};
static struct strmap_block blockmap;

static struct block *add_next_block(struct state *state,
				    struct block *prev, const char *name,
				    unsigned int num_txs)
{
	struct block *b;
	struct protocol_block_header *hdr;
	struct protocol_block_tailer *tailer;
	u8 *shard_nums;
	struct protocol_double_sha dummy = { { 0 } };

	hdr = tal(state, struct protocol_block_header);
	hdr->shard_order = PROTOCOL_INITIAL_SHARD_ORDER;
	hdr->depth = cpu_to_le32(le32_to_cpu(prev->hdr->depth) + 1);
	hdr->prev_block = prev->sha;

	tailer = tal(state, struct protocol_block_tailer);
	tailer->difficulty = prev->tailer->difficulty;

	shard_nums = tal_arrz(state, u8, 1 << hdr->shard_order);
	shard_nums[0] = num_txs;

	memcpy(&dummy, name,
	       strlen(name) < sizeof(dummy) ? strlen(name) : sizeof(dummy));

	b = block_add(state, prev, &dummy, hdr, shard_nums, NULL, NULL,
		      tailer);

	strmap_add(&blockmap, name, b);
	return b;
}

static void create_chain(struct state *state, struct block *base,
			 const char *prefix, unsigned int num, bool known)
{
	unsigned int i;

	for (i = 0; i < num; i++) {
		char *name = tal_fmt(state, "%s-%u", prefix, i);
		base = add_next_block(state, base, name, known ? 0 : 1);
		known = true;
	}
}

int main(void)
{
	struct state *state;

	strmap_init(&blockmap);

	pseudorand_init();
	state = new_state(true);

	/* genesis -> block1-0 ... block1-10. */
	create_chain(state, &genesis, "block1", 10, true);

	assert(tal_count(state->longest_knowns) == 1);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "block1-9"));
	assert(tal_count(state->longest_chains) == 1);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "block1-9"));
	assert(state->preferred_chain == strmap_get(&blockmap, "block1-9"));
	assert(strmap_get(&blockmap, "block1-9")->all_known);

	/* Now add one we don't know all of. */
	add_next_block(state, strmap_get(&blockmap, "block1-9"),
		       "block1-10", 1);
	assert(tal_count(state->longest_knowns) == 1);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "block1-9"));
	assert(tal_count(state->longest_chains) == 1);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "block1-10"));
	assert(state->preferred_chain == strmap_get(&blockmap, "block1-10"));
	assert(strmap_get(&blockmap, "block1-9")->all_known);
	assert(!strmap_get(&blockmap, "block1-10")->all_known);

	/* Now add another all-known one to that. */
	add_next_block(state, strmap_get(&blockmap, "block1-10"),
		       "block1-11", 0);
	assert(tal_count(state->longest_knowns) == 1);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "block1-9"));
	assert(tal_count(state->longest_chains) == 1);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "block1-11"));
	assert(state->preferred_chain == strmap_get(&blockmap, "block1-11"));
	assert(strmap_get(&blockmap, "block1-9")->all_known);
	assert(!strmap_get(&blockmap, "block1-10")->all_known);
	assert(!strmap_get(&blockmap, "block1-11")->all_known);

	/* Create a all-known competitor to block1-9. */
	add_next_block(state, strmap_get(&blockmap, "block1-8"),
		       "block2-9", 0);
	assert(tal_count(state->longest_knowns) == 2);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "block1-9"));
	assert(state->longest_knowns[1] == strmap_get(&blockmap, "block2-9"));
	assert(tal_count(state->longest_chains) == 1);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "block1-11"));
	assert(state->preferred_chain == strmap_get(&blockmap, "block1-11"));
	assert(strmap_get(&blockmap, "block1-9")->all_known);
	assert(!strmap_get(&blockmap, "block1-10")->all_known);
	assert(!strmap_get(&blockmap, "block1-11")->all_known);
	assert(strmap_get(&blockmap, "block2-9")->all_known);

	/* Extend it by one more, and it will become preferred. */
	add_next_block(state, strmap_get(&blockmap, "block2-9"),
		       "block2-10", 0);
	assert(tal_count(state->longest_knowns) == 1);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "block2-10"));
	assert(tal_count(state->longest_chains) == 1);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "block1-11"));
	assert(state->preferred_chain == strmap_get(&blockmap, "block2-10"));
	assert(strmap_get(&blockmap, "block1-9")->all_known);
	assert(!strmap_get(&blockmap, "block1-10")->all_known);
	assert(!strmap_get(&blockmap, "block1-11")->all_known);
	assert(strmap_get(&blockmap, "block2-10")->all_known);

	/* Add tx to shard to complete block1-10. */
	strmap_get(&blockmap, "block1-10")->shard[0]->txcount++;
	update_block_ptrs_new_shard(state, strmap_get(&blockmap, "block1-10"),
				    0);
	assert(tal_count(state->longest_knowns) == 1);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "block1-11"));
	assert(tal_count(state->longest_chains) == 1);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "block1-11"));
	assert(state->preferred_chain == strmap_get(&blockmap, "block1-11"));
	assert(strmap_get(&blockmap, "block1-9")->all_known);
	assert(strmap_get(&blockmap, "block1-10")->all_known);
	assert(strmap_get(&blockmap, "block1-11")->all_known);

	/* But, if block-1-10 is invalidated, we go back... */
	strmap_get(&blockmap, "block1-10")->complaint = "foo";
	strmap_get(&blockmap, "block1-11")->complaint = "foo";
	update_block_ptrs_invalidated(state,
				      strmap_get(&blockmap, "block1-10"));
	
	assert(tal_count(state->longest_knowns) == 1);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "block2-10"));
	assert(tal_count(state->longest_chains) == 1);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "block2-10"));
	assert(state->preferred_chain == strmap_get(&blockmap, "block2-10"));

	/* Now create multiple known chains, and multiple longest
	 * chains. */
	create_chain(state, strmap_get(&blockmap, "block2-10"), "known1", 5,
		     true);
	create_chain(state, strmap_get(&blockmap, "block2-10"), "known2", 5,
		     true);
	create_chain(state, strmap_get(&blockmap, "block2-10"), "known3", 5,
		     true);

	create_chain(state, strmap_get(&blockmap, "block2-10"), "unknown1",
		     10, false);
	create_chain(state, strmap_get(&blockmap, "block2-10"), "unknown2",
		     10, false);
	create_chain(state, strmap_get(&blockmap, "block2-10"), "unknown3",
		     10, false);

	/* Default order must be first in, best dressed. */
	assert(tal_count(state->longest_knowns) == 3);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "known1-4"));
	assert(state->longest_knowns[1] == strmap_get(&blockmap, "known2-4"));
	assert(state->longest_knowns[2] == strmap_get(&blockmap, "known3-4"));
	assert(tal_count(state->longest_chains) == 3);
	assert(state->longest_chains[0] == strmap_get(&blockmap, "unknown1-9"));
	assert(state->longest_chains[1] == strmap_get(&blockmap, "unknown2-9"));
	assert(state->longest_chains[2] == strmap_get(&blockmap, "unknown3-9"));
	assert(state->preferred_chain == strmap_get(&blockmap, "known1-4"));

	/* If we make one of the known chains equal to the longest chains,
	 * it will become preferred. */
	create_chain(state, strmap_get(&blockmap, "known3-4"), "known3-unknown",
		     5, false);
	assert(tal_count(state->longest_knowns) == 3);
	assert(state->longest_knowns[0] == strmap_get(&blockmap, "known3-4"));
	assert(state->longest_knowns[1] == strmap_get(&blockmap, "known2-4"));
	assert(state->longest_knowns[2] == strmap_get(&blockmap, "known1-4"));
	assert(tal_count(state->longest_chains) == 4);
	assert(state->longest_chains[0]
	       == strmap_get(&blockmap, "known3-unknown-4"));
	assert(state->longest_chains[1] == strmap_get(&blockmap, "unknown2-9"));
	assert(state->longest_chains[2] == strmap_get(&blockmap, "unknown3-9"));
	assert(state->longest_chains[3] == strmap_get(&blockmap, "unknown1-9"));
	assert(state->preferred_chain == strmap_get(&blockmap, "known3-unknown-4"));

	strmap_clear(&blockmap);
	tal_free(state);
	return 0;
}
