#include "base58.h"
#include "block.h"
#include "check_tx.h"
#include "jsonrpc.h"
#include "pending.h"
#include "protocol.h"
#include "shard.h"
#include "state.h"
#include "tal_arr.h"
#include "timestamp.h"
#include "tx.h"
#include <ccan/structeq/structeq.h>
#include <ccan/take/take.h>
#include <ccan/tal/str/str.h>

static void json_add_input(char **response, const char *fieldname,
			   const struct protocol_input *inp)
{
	json_object_start(response, fieldname);
	json_add_double_sha(response, "input", &inp->input);
	json_add_num(response, "output", le16_to_cpu(inp->output));
	json_object_end(response);
}

static void json_add_inputs(char **response, const union protocol_tx *tx)
{
	unsigned int i;

	json_array_start(response, "vin");
	for (i = 0; i < num_inputs(tx); i++)
		json_add_input(response, NULL, tx_input(tx, i));
	json_array_end(response);
}

static void json_add_outputs(char **response,
			     struct state *state, const union protocol_tx *tx)
{
	unsigned int i;
	struct protocol_gateway_payment *outputs;

	outputs = get_from_gateway_outputs(&tx->from_gateway);

	json_array_start(response, "vout");
	for (i = 0; i < num_outputs(tx); i++) {
		json_object_start(response, NULL);
		json_add_num(response, "send_amount",
			 le32_to_cpu(outputs[i].send_amount));
		json_add_address(response, "output_addr", state->test_net,
				 &outputs[i].output_addr);
		json_object_end(response);
	}
	json_array_end(response);
}

/* Caller does json_object_start & json_object_end */
static void json_append_tx(char **response,
			   struct state *state,
			   const union protocol_tx *tx,
			   const struct block *block,
			   unsigned int confirms)
{
	struct protocol_double_sha sha;

	hash_tx(tx, &sha);
	json_add_double_sha(response, "txid", &sha);
	if (block)
		json_add_double_sha(response, "block", &block->sha);
	json_add_num(response, "confirmations", confirms);
	json_add_num(response, "version", tx->hdr.version);
	json_add_num(response, "features", tx->hdr.features);

	switch (tx_type(tx)) {
	case TX_NORMAL:
		json_add_string(response, "type", "TX_NORMAL");
		json_add_pubkey(response, "input_key", &tx->normal.input_key);
		json_add_address(response, "output_addr",
				 state->test_net, &tx->normal.output_addr);
		json_add_num(response, "send_amount",
			     le32_to_cpu(tx->normal.send_amount));
		json_add_num(response, "change_amount",
			     le32_to_cpu(tx->normal.change_amount));
		json_add_signature(response, "signature",
				   &tx->normal.signature);
		json_add_inputs(response, tx);
		return;
	case TX_FROM_GATEWAY:
		json_add_string(response, "type", "TX_FROM_GATEWAY");
		json_add_pubkey(response, "gateway_key",
				&tx->from_gateway.gateway_key);
		json_add_signature(response, "signature",
				   &tx->normal.signature);
		json_add_outputs(response, state, tx);
		return;
	case TX_TO_GATEWAY:
		json_add_string(response, "type", "TX_TO_GATEWAY");
		json_add_pubkey(response, "input_key",
				&tx->to_gateway.input_key);
		json_add_address(response, "output_addr",
				 state->test_net,
				 &tx->to_gateway.to_gateway_addr);
		json_add_num(response, "send_amount",
			     le32_to_cpu(tx->to_gateway.send_amount));
		json_add_num(response, "change_amount",
			     le32_to_cpu(tx->to_gateway.change_amount));
		json_add_signature(response, "signature",
				   &tx->to_gateway.signature);
		json_add_inputs(response, tx);
		return;
	case TX_CLAIM:
		json_add_string(response, "type", "TX_CLAIM");
		json_add_pubkey(response, "input_key", &tx->claim.input_key);
		json_add_num(response, "amount", le32_to_cpu(tx->claim.amount));
		json_add_signature(response, "claim", &tx->claim.signature);
		json_add_input(response, "input", &tx->claim.input);
		return;
	}
	abort();
}

static bool unspent_output_affects(struct json_connection *jcon,
				   const union protocol_tx *tx,
				   const struct protocol_address *address)
{
	unsigned int i;
	struct txhash_elem *te;
	struct protocol_input inp;
	bool ret = false;

	hash_tx(tx, &inp.input);

	for (i = 0; i < num_outputs(tx); i++) {
		struct protocol_address addr;
		u32 amount;

		inp.output = cpu_to_le16(i);

		/* This is the equivalent search. */
		te = tx_find_doublespend(jcon->state,
					 jcon->state->preferred_chain,
					 NULL, &inp);
		if (te)
			continue;

		/* This must succeed, as i < num_outputs(tx) */
		if (!find_output(tx, i, &addr, &amount))
			abort();

		if (structeq(&addr, address))
			return true;
	}
	return ret;
}

/* Find unspent tx outputs which affect this address, and report them. */
static void add_existing_txs(struct json_connection *jcon,
			     const struct protocol_address *address,
			     char **response)
{
	const struct block *b;
	unsigned int shard, txoff;
	int i, depth = 1;
	char **txs = tal_arr(jcon, char *, 0);

	for (b = jcon->state->preferred_chain; b; b = b->prev, depth++) {
		/* Once block is past horizon, we can't spend it */
		if (le32_to_cpu(b->tailer->timestamp) 
		    + PROTOCOL_TX_HORIZON_SECS(jcon->state->test_net)
		    < current_time())
			break;

		for (shard = 0; shard < num_shards(b->hdr); shard++) {
			for (txoff = 0; txoff < b->shard[shard]->size; txoff++) {
				union protocol_tx *tx;
				char *txstring;

				tx = block_get_tx(b, shard, txoff);
				if (!tx)
					continue;

				if (!unspent_output_affects(jcon, tx, address))
					continue;

				txstring = tal_arr(jcon, char, 0);
				json_append_tx(&txstring, jcon->state,
					       tx, b, depth);
				tal_arr_append(&txs, txstring);
			}
		}
	}

	/* Array is backwards, but want to report forwards. */
	for (i = tal_count(txs) - 1; i >= 0; i--) {
		json_object_start(response, NULL);
		*response = tal_strcat(NULL, take(*response), txs[i]);
		json_object_end(response);
	}
	tal_free(txs);
}

/* Pending txs have depth 0. */
static void add_pending_txs(struct json_connection *jcon,
			    const struct protocol_address *address,
			    char **response)
{
	unsigned int txoff, shard, num_shards;
	const struct pending_block *pend = jcon->state->pending;
	const struct pending_unknown_tx *utx;

	/* First get everything in the block about to be mined. */
	num_shards = next_shard_order(jcon->state->longest_knowns[0]);
	for (shard = 0; shard < num_shards; shard++) {
		for (txoff = 0; txoff < tal_count(pend->pend[shard]); txoff++) {
			const union protocol_tx *tx;

			tx = pend->pend[shard][txoff]->tx;
			/* FIXME: This results in double-counting, since
			 * this won't find outputs spent by other pending txs */
			if (!unspent_output_affects(jcon, tx, address))
				continue;

			json_object_start(response, NULL);
			json_append_tx(response, jcon->state, tx, NULL, 0);
			json_object_end(response);
		}
	}

	/* Now the ones with unknown inputs. */
	list_for_each(&jcon->state->pending->unknown_tx, utx, list) {
		/* FIXME: This results in double-counting, since
		 * this won't find outputs spent by other pending txs */
		if (!unspent_output_affects(jcon, utx->tx, address))
			continue;

		json_object_start(response, NULL);
		json_append_tx(response, jcon->state, utx->tx, NULL, 0);
		json_object_end(response);
	}
}

static char *json_list_transactions(struct json_connection *jcon,
				    const jsmntok_t *params,
				    char **response)
{
	struct protocol_address address;
	const jsmntok_t *addr, *minconf;
	bool test_net;
	unsigned int minimum_confirms;

	json_get_params(jcon->buffer, params,
			"address", &addr,
			"minconf", &minconf, NULL);

	if (!addr)
		return tal_fmt(jcon, "Needs 'address'");

	if (addr->type != JSMN_STRING
	    || !pettycoin_from_base58(&test_net, &address,
				      jcon->buffer + addr->start,
				      addr->end - addr->start)) {
		return tal_fmt(jcon, "address %.*s not valid",
			       json_tok_len(addr),
			       json_tok_contents(jcon->buffer, addr));
	}

	if (test_net != jcon->state->test_net)
		return tal_fmt(jcon, "address %.*s %s test net",
			       json_tok_len(addr),
			       json_tok_contents(jcon->buffer, addr),
			       test_net ? "on" : "not on");

	if (!minconf)
		minimum_confirms = 1;
	else {
		if (!json_tok_number(jcon->buffer, minconf, &minimum_confirms))
			return tal_fmt(jcon, "minconf %.*s not valid",
				       json_tok_len(minconf),
				       json_tok_contents(jcon->buffer,
							 minconf));
	}

	json_array_start(response, NULL);
	add_existing_txs(jcon, &address, response);
	if (minimum_confirms == 0)
		add_pending_txs(jcon, &address, response);
	json_array_end(response);

	return NULL;
}
	
struct json_command listtransactions_command = {
	"listtransactions", json_list_transactions,
	"show transactions to/from a given address",
	"<address> <minconf> - list all the transactions to/from a specific address as they appear in a block on the preferred chain, if they are >= <minconf> confirmations.  minconf defaults to 1; if 0, shows pending transactions."
};
